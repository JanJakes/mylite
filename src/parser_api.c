#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "lexer.h"
#include "parser_bison.h"
#include "parser_internal.h"

int yyparse(mylite_parser *parser);

static void match_compound_control_tokens(mylite_parser *parser);
static int is_compound_control_start_token(const mylite_parser *parser, size_t token_index);
static int is_begin_work_statement(const mylite_parser *parser, size_t token_index);
static int is_if_function_call(const mylite_parser *parser, size_t token_index);
static int is_if_exists_clause(const mylite_parser *parser, size_t token_index);
static int is_compound_control_end_token(int token);
static int compound_control_tokens_match(int start_token, int end_token);
static void merge_compound_control_statement_spans(mylite_parser *parser);
static int statement_starts_with_matched_compound_control(const mylite_parser *parser,
                                                          const mylite_statement *statement,
                                                          size_t *end_token);
static size_t last_compound_control_statement_token(const mylite_parser *parser, size_t end_token);
static int compound_control_end_allows_label(int token);
static void set_statement_end_from_token(const mylite_parser *parser,
                                         mylite_statement *statement,
                                         size_t token_index);
static void remove_statements_covered_by_previous(mylite_parser *parser, size_t statement_index);
static void validate_statement_syntax(mylite_parser *parser);
static int validate_create_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_create_role_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index);
static int validate_create_loadable_function_statement_syntax(const mylite_parser *parser,
                                                              size_t token_index,
                                                              size_t last_token_index);
static int token_starts_create_loadable_function_statement(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index);
static int token_is_loadable_function_return_type(const mylite_parser *parser, size_t token_index);
static int validate_create_routine_statement_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index);
static int validate_routine_parameter_list_syntax(const mylite_parser *parser,
                                                  size_t open_token_index,
                                                  int is_function);
static int validate_create_routine_characteristic_syntax(const mylite_parser *parser,
                                                         size_t token_index,
                                                         size_t last_token_index,
                                                         size_t *next_token_index);
static int token_starts_create_routine_statement(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index);
static int token_is_routine_parameter_mode(const mylite_parser *parser, size_t token_index);
static int token_starts_create_routine_characteristic(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index);
static int token_can_start_stored_function_body(const mylite_parser *parser, size_t token_index);
static int validate_create_index_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int validate_create_index_key_part_list_syntax(const mylite_parser *parser, size_t open_token_index);
static int validate_create_index_option_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index,
                                               size_t *next_token_index);
static int token_starts_create_index_statement(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index);
static int token_is_create_index_modifier(const mylite_parser *parser, size_t token_index);
static int token_is_index_type_value(const mylite_parser *parser, size_t token_index);
static int validate_create_view_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index);
static int token_starts_view_statement(const mylite_parser *parser,
                                       size_t token_index,
                                       size_t last_token_index);
static int validate_create_event_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int validate_alter_event_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index);
static int token_starts_event_statement(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index);
static int validate_create_trigger_statement_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index);
static int token_starts_trigger_statement(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index);
static int validate_create_table_compact_statement_syntax(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index);
static int token_starts_create_table_compact_statement(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index);
static int validate_create_table_definition_statement_syntax(const mylite_parser *parser,
                                                             size_t token_index,
                                                             size_t last_token_index);
static int token_starts_create_table_definition_statement(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index);
static int validate_create_database_statement_syntax(const mylite_parser *parser,
                                                     size_t token_index,
                                                     size_t last_token_index);
static int validate_database_option_list_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index,
                                                int allow_read_only);
static int validate_database_option_syntax(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index,
                                           size_t *next_token_index,
                                           int allow_read_only);
static int token_starts_database_option(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index,
                                        int allow_read_only);
static int token_is_database_option_value(const mylite_parser *parser, size_t token_index);
static int token_is_read_only_value(const mylite_parser *parser, size_t token_index);
static int validate_create_resource_group_statement_syntax(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index);
static int validate_create_logfile_group_statement_syntax(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index);
static int validate_logfile_group_add_undofile_clause_syntax(const mylite_parser *parser,
                                                             size_t token_index,
                                                             size_t last_token_index,
                                                             size_t *next_token_index);
static int validate_storage_size_clause_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index,
                                               const char *clause_name,
                                               size_t *next_token_index);
static int validate_storage_nodegroup_clause_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    size_t *next_token_index);
static int validate_storage_comment_clause_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index,
                                                  size_t *next_token_index);
static int token_is_storage_size_literal(const mylite_parser *parser, size_t token_index);
static int source_span_is_storage_size_literal(const char *source, size_t start_offset, size_t end_offset);
static int validate_create_tablespace_statement_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       int is_undo_tablespace);
static int validate_tablespace_datafile_clause_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index,
                                                      const char *action,
                                                      size_t *next_token_index);
static int validate_tablespace_use_logfile_group_clause_syntax(const mylite_parser *parser,
                                                               size_t token_index,
                                                               size_t last_token_index,
                                                               size_t *next_token_index);
static int validate_tablespace_encryption_clause_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index,
                                                        size_t *next_token_index);
static int validate_tablespace_engine_attribute_clause_syntax(const mylite_parser *parser,
                                                              size_t token_index,
                                                              size_t last_token_index,
                                                              size_t *next_token_index);
static int validate_tablespace_optional_storage_tail_syntax(const mylite_parser *parser,
                                                            size_t token_index,
                                                            size_t last_token_index);
static int validate_create_spatial_reference_system_statement_syntax(const mylite_parser *parser,
                                                                     size_t token_index,
                                                                     size_t last_token_index);
static int validate_spatial_reference_system_attribute_syntax(const mylite_parser *parser,
                                                              size_t token_index,
                                                              size_t last_token_index,
                                                              size_t *next_token_index,
                                                              int *attribute_kind);
static int token_is_create_spatial_reference_system_start(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index);
static int validate_create_server_statement_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index);
static int validate_server_options_clause_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 size_t *next_token_index);
static int validate_server_option_syntax(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index,
                                         size_t *next_token_index);
static int token_is_server_character_option(const mylite_parser *parser, size_t token_index);
static int token_is_server_numeric_option(const mylite_parser *parser, size_t token_index);
static int validate_create_user_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index);
static int validate_create_user_entry_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index,
                                             size_t *next_token_index);
static int validate_create_user_auth_option_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index,
                                                   size_t *next_token_index,
                                                   int allow_initial_auth);
static int validate_create_user_identified_tail_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index,
                                                       int allow_initial_auth);
static int validate_create_user_initial_auth_option_syntax(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index,
                                                           size_t *next_token_index);
static int validate_create_user_auth_mfa_tail_syntax(const mylite_parser *parser,
                                                     size_t token_index,
                                                     size_t last_token_index,
                                                     size_t *next_token_index);
static int validate_create_user_tail_syntax(const mylite_parser *parser,
                                            size_t token_index,
                                            size_t last_token_index);
static int validate_create_user_default_role_clause_syntax(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index,
                                                           size_t *next_token_index);
static int validate_create_user_tls_clause_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index,
                                                  size_t *next_token_index);
static int validate_create_user_tls_option_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index,
                                                  size_t *next_token_index);
static int validate_create_user_resource_clause_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index);
static int validate_create_user_password_or_lock_option_syntax(const mylite_parser *parser,
                                                               size_t token_index,
                                                               size_t last_token_index,
                                                               size_t *next_token_index);
static int token_is_create_user_tail_boundary(const mylite_parser *parser, size_t token_index);
static int token_is_create_user_auth_string(const mylite_parser *parser, size_t token_index);
static int token_is_create_user_auth_hash(const mylite_parser *parser, size_t token_index);
static int token_can_be_create_user_auth_plugin(const mylite_parser *parser, size_t token_index);
static int token_is_create_user_resource_option(const mylite_parser *parser, size_t token_index);
static int validate_alter_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_view_statement_syntax(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index);
static int validate_view_algorithm_clause_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 size_t *next_token_index);
static int validate_definer_clause_syntax(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index,
                                          size_t *next_token_index);
static int validate_view_sql_security_clause_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    size_t *next_token_index);
static size_t find_view_check_option_clause_token(const mylite_parser *parser,
                                                  size_t first_query_token,
                                                  size_t last_token_index);
static int token_is_view_algorithm_value(const mylite_parser *parser, size_t token_index);
static int token_can_start_view_query(const mylite_parser *parser, size_t token_index);
static int validate_event_statement_syntax(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index,
                                           int is_create);
static int validate_event_schedule_clause_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 size_t *next_token_index);
static int validate_event_interval_syntax(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index,
                                          size_t *next_token_index);
static int validate_event_completion_clause_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index,
                                                   size_t *next_token_index);
static int validate_event_status_clause_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index,
                                               size_t *next_token_index);
static int validate_event_comment_clause_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index,
                                                size_t *next_token_index);
static int validate_event_rename_clause_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index,
                                               size_t *next_token_index);
static size_t find_event_clause_boundary_token(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index);
static size_t find_event_schedule_subclause_boundary_token(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index);
static int token_starts_event_clause_boundary(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index);
static int token_starts_event_schedule_subclause_boundary(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index);
static int token_is_event_interval_unit(const mylite_parser *parser, size_t token_index);
static int validate_trigger_order_clause_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index,
                                                size_t *next_token_index);
static int token_is_trigger_time(const mylite_parser *parser, size_t token_index);
static int token_is_trigger_event(const mylite_parser *parser, size_t token_index);
static size_t first_token_after_create_table_head(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int validate_create_table_like_tail_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int validate_create_table_select_tail_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index);
static int validate_create_table_definition_group_syntax(const mylite_parser *parser, size_t open_token_index);
static int token_can_start_create_table_query(const mylite_parser *parser, size_t token_index);
static int validate_alter_database_statement_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index);
static int validate_alter_routine_statement_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index);
static int validate_alter_routine_characteristic_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index,
                                                        size_t *next_token_index);
static int validate_alter_table_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index);
static int validate_alter_table_action_list_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index);
static int validate_alter_table_action_syntax(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index,
                                              size_t *next_token_index);
static int token_starts_alter_table_action(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index);
static size_t find_alter_table_action_boundary(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index);
static int validate_alter_table_add_action_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int validate_alter_table_key_action_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int validate_alter_table_drop_action_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index);
static int validate_alter_table_change_or_modify_action_syntax(const mylite_parser *parser,
                                                               size_t token_index,
                                                               size_t last_token_index);
static int validate_alter_table_alter_action_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index);
static int validate_alter_table_rename_action_syntax(const mylite_parser *parser,
                                                     size_t token_index,
                                                     size_t last_token_index);
static int validate_alter_table_order_action_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index);
static int validate_alter_table_algorithm_or_lock_action_syntax(const mylite_parser *parser,
                                                                size_t token_index,
                                                                size_t last_token_index);
static int validate_alter_table_validation_action_syntax(const mylite_parser *parser,
                                                         size_t token_index,
                                                         size_t last_token_index);
static int validate_alter_table_enable_or_disable_action_syntax(const mylite_parser *parser,
                                                                size_t token_index,
                                                                size_t last_token_index);
static int validate_alter_table_convert_action_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index);
static int validate_alter_table_partition_action_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index);
static int validate_alter_table_table_option_action_syntax(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index);
static int validate_alter_table_nonempty_list_syntax(const mylite_parser *parser,
                                                     size_t token_index,
                                                     size_t last_token_index);
static int alter_table_action_has_group(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index);
static int token_can_start_alter_table_name(const mylite_token *token);
static int token_is_alter_table_column_keyword(const mylite_parser *parser, size_t token_index);
static int token_is_alter_table_key_action_head(const mylite_parser *parser, size_t token_index);
static int token_is_alter_table_algorithm_value(const mylite_parser *parser, size_t token_index);
static int token_is_alter_table_table_option_start(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index);
static int token_is_alter_table_partition_action_start(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index);
static int validate_alter_instance_statement_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index);
static int token_is_alter_instance_tls_channel(const mylite_parser *parser, size_t token_index);
static int validate_alter_resource_group_statement_syntax(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index);
static int validate_alter_logfile_group_statement_syntax(const mylite_parser *parser,
                                                         size_t token_index,
                                                         size_t last_token_index);
static int validate_alter_tablespace_statement_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index,
                                                      int is_undo_tablespace);
static int validate_alter_tablespace_file_action_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index,
                                                        size_t *next_token_index);
static int validate_alter_tablespace_rename_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index,
                                                   size_t *next_token_index);
static int validate_alter_tablespace_set_state_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index,
                                                      size_t *next_token_index);
static int validate_alter_server_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int validate_alter_user_statement_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index);
static int validate_alter_user_entry_syntax(const mylite_parser *parser,
                                            size_t token_index,
                                            size_t last_token_index,
                                            size_t *next_token_index);
static int validate_alter_user_name_syntax(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index,
                                           size_t *next_token_index,
                                           int *is_user_function);
static int validate_alter_user_entry_option_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index,
                                                   size_t *next_token_index,
                                                   int is_user_function);
static int validate_alter_user_identified_tail_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index,
                                                      size_t *next_token_index,
                                                      int is_user_function);
static int validate_alter_user_auth_option_tail_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index);
static int validate_alter_user_factor_operation_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index);
static int validate_alter_user_factor_auth_option_syntax(const mylite_parser *parser,
                                                         size_t token_index,
                                                         size_t last_token_index,
                                                         size_t *next_token_index);
static int validate_alter_user_registration_option_syntax(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index,
                                                          size_t *next_token_index);
static int validate_alter_user_tail_syntax(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index);
static int validate_alter_user_default_role_clause_syntax(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index,
                                                          size_t *next_token_index);
static int token_starts_alter_user_entry_option(const mylite_parser *parser, size_t token_index);
static int token_starts_alter_user_factor(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index);
static int token_is_resource_group_sequence(const mylite_parser *parser,
                                            size_t token_index,
                                            size_t last_token_index);
static int validate_resource_group_vcpu_clause_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index,
                                                      size_t *next_token_index);
static int validate_resource_group_vcpu_spec_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    size_t *next_token_index);
static int validate_resource_group_thread_priority_clause_syntax(const mylite_parser *parser,
                                                                 size_t token_index,
                                                                 size_t last_token_index,
                                                                 size_t *next_token_index);
static int validate_use_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_truncate_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_rename_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_rename_table_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int validate_rename_table_pair_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index,
                                             size_t *next_token_index);
static int validate_rename_user_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index);
static int validate_rename_user_pair_syntax(const mylite_parser *parser,
                                            size_t token_index,
                                            size_t last_token_index,
                                            size_t *next_token_index);
static int validate_call_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_call_argument_list_syntax(const mylite_parser *parser, size_t open_token_index);
static int validate_do_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_expression_list_syntax(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index);
static int validate_values_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_values_row_constructor_list_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index);
static int validate_values_tail_syntax(const mylite_parser *parser,
                                       size_t token_index,
                                       size_t last_token_index);
static int validate_values_order_by_tail_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index,
                                                size_t *next_token_index);
static size_t find_values_statement_token(const mylite_parser *parser, const mylite_statement *statement);
static int values_statement_is_explained_query(const mylite_parser *parser,
                                               const mylite_statement *statement,
                                               size_t values_token_index);
static int token_is_values_tail_boundary(const mylite_parser *parser, size_t token_index);
static int validate_insert_or_replace_statement_syntax(const mylite_parser *parser,
                                                       const mylite_statement *statement);
static int skip_insert_or_replace_modifiers(const mylite_parser *parser,
                                            size_t token_index,
                                            size_t last_token_index,
                                            int is_insert,
                                            size_t *next_token_index);
static int validate_insert_or_replace_source_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    int is_insert);
static int validate_insert_or_replace_values_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    int is_insert);
static int validate_insert_parenthesized_value_list_syntax(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index,
                                                           size_t *next_token_index);
static int validate_insert_row_constructor_list_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index);
static int validate_insert_or_replace_set_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 int is_insert);
static int validate_insert_or_replace_query_source_syntax(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index);
static int validate_insert_or_replace_tail_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index,
                                                  int is_insert);
static int validate_insert_row_alias_clause_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index,
                                                   size_t *next_token_index);
static int validate_insert_on_duplicate_key_update_tail_syntax(const mylite_parser *parser,
                                                               size_t token_index,
                                                               size_t last_token_index);
static int validate_dml_assignment_list_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index);
static int validate_dml_name_list_group_syntax(const mylite_parser *parser,
                                               size_t open_token_index,
                                               int allow_empty);
static size_t find_insert_assignment_tail_token(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index,
                                                int is_insert);
static int token_starts_insert_or_replace_query_source(const mylite_parser *parser, size_t token_index);
static int token_is_insert_priority_modifier(const mylite_parser *parser,
                                             size_t token_index,
                                             int is_insert);
static int validate_update_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int skip_update_modifiers(const mylite_parser *parser,
                                 size_t token_index,
                                 size_t last_token_index,
                                 size_t *next_token_index);
static size_t find_update_set_token(const mylite_parser *parser,
                                    size_t token_index,
                                    size_t last_token_index);
static int validate_update_table_reference_span_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index);
static int validate_update_assignment_and_tail_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index);
static size_t find_update_tail_token(const mylite_parser *parser,
                                     size_t token_index,
                                     size_t last_token_index);
static int validate_update_tail_syntax(const mylite_parser *parser,
                                       size_t token_index,
                                       size_t last_token_index);
static int validate_update_where_tail_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index,
                                             size_t *next_token_index);
static int validate_update_limit_tail_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index);
static int token_starts_update_tail(const mylite_parser *parser, size_t token_index);
static int validate_delete_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int skip_delete_modifiers(const mylite_parser *parser,
                                 size_t token_index,
                                 size_t last_token_index,
                                 size_t *next_token_index);
static int validate_delete_from_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index);
static int validate_delete_multi_from_statement_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index);
static int validate_delete_single_table_statement_syntax(const mylite_parser *parser,
                                                         size_t token_index,
                                                         size_t last_token_index);
static int validate_delete_target_list_syntax(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index);
static int validate_delete_target_syntax(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index,
                                         size_t *next_token_index);
static int validate_delete_table_reference_span_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index);
static size_t find_delete_clause_token(const mylite_parser *parser,
                                       size_t token_index,
                                       size_t last_token_index,
                                       int clause_token);
static size_t find_delete_tail_token(const mylite_parser *parser,
                                     size_t token_index,
                                     size_t last_token_index);
static int validate_delete_single_table_tail_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index);
static int validate_delete_multi_table_tail_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index);
static int token_starts_delete_tail(const mylite_parser *parser, size_t token_index);
static int validate_import_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_binlog_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_install_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_component_uri_list_syntax(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index,
                                              int allow_set_clause);
static int validate_install_component_set_clause_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index);
static int token_is_install_component_set_scope(const mylite_parser *parser, size_t token_index);
static int validate_lock_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_lock_table_list_syntax(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index);
static int token_can_start_lock_table_name(const mylite_parser *parser, size_t token_index);
static int token_can_start_lock_table_alias(const mylite_parser *parser, size_t token_index);
static int token_is_lock_table_mode(const mylite_parser *parser, size_t token_index);
static int validate_unlock_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_cursor_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int cursor_statement_is_compound_label_token(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index);
static int validate_open_close_cursor_statement_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index);
static int validate_fetch_cursor_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int validate_fetch_variable_list_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index);
static int token_can_start_fetch_variable(const mylite_token *token);
static int validate_handler_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_handler_open_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int validate_handler_read_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int validate_handler_read_key_comparison_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index);
static int validate_handler_read_tail_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index);
static int validate_handler_where_clause_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index,
                                                size_t *next_token_index);
static int token_can_start_handler_index_name(const mylite_parser *parser, size_t token_index);
static int token_is_handler_read_direction(const mylite_parser *parser, size_t token_index);
static int token_is_handler_indexed_read_direction(const mylite_parser *parser, size_t token_index);
static int token_is_handler_read_comparison_operator(const mylite_parser *parser, size_t token_index);
static int validate_cache_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_load_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_load_data_or_xml_statement_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index,
                                                      int is_xml);
static int validate_load_partition_clause_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 size_t *next_token_index);
static int validate_load_character_set_clause_syntax(const mylite_parser *parser,
                                                     size_t token_index,
                                                     size_t last_token_index,
                                                     size_t *next_token_index);
static int validate_load_fields_or_lines_clause_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index);
static int validate_load_ignore_rows_clause_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index,
                                                   size_t *next_token_index);
static int validate_load_rows_identified_clause_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index);
static int validate_load_column_list_syntax(const mylite_parser *parser, size_t open_token_index);
static int validate_load_set_clause_syntax(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index);
static int validate_load_set_assignment_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index,
                                               size_t *next_token_index);
static int validate_load_index_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_table_index_list_syntax(const mylite_parser *parser,
                                            size_t token_index,
                                            size_t last_token_index,
                                            size_t *next_token_index,
                                            int allow_ignore_leaves);
static int validate_table_index_entry_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index,
                                             size_t *next_token_index,
                                             int allow_ignore_leaves);
static int validate_name_list_group_syntax(const mylite_parser *parser,
                                           size_t open_token_index,
                                           int allow_all,
                                           int allow_primary);
static int token_can_start_table_index_table_name(const mylite_parser *parser, size_t token_index);
static int token_can_start_table_index_item_name(const mylite_parser *parser,
                                                 size_t token_index,
                                                 int allow_primary);
static int token_can_start_key_cache_name(const mylite_parser *parser, size_t token_index);
static int validate_purge_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_nonempty_expression_tail_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index);
static int validate_set_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_set_role_statement_syntax(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index);
static int validate_set_default_role_statement_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index);
static int validate_set_password_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int validate_set_password_auth_option_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    size_t *next_token_index);
static int validate_set_password_tail_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index);
static int token_is_set_password_string_value(const mylite_parser *parser, size_t token_index);
static int validate_set_resource_group_statement_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index);
static int validate_set_names_statement_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index);
static int validate_set_character_set_statement_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index);
static int validate_set_assignment_tail_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index);
static int token_can_be_character_set_value(const mylite_parser *parser, size_t token_index);
static int token_can_be_collation_value(const mylite_parser *parser, size_t token_index);
static int validate_reset_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_reset_persist_statement_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index);
static int validate_reset_option_syntax(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index,
                                        size_t *next_token_index);
static int validate_reset_binary_logs_option_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    size_t *next_token_index);
static int validate_reset_replica_option_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index,
                                                size_t *next_token_index);
static int validate_flush_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_flush_table_option_syntax(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index);
static int validate_flush_table_name_list_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 size_t *next_token_index);
static int token_can_start_flush_table_name(const mylite_parser *parser, size_t token_index);
static int validate_flush_option_list_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index);
static int validate_flush_option_syntax(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index,
                                        size_t *next_token_index);
static int validate_maintenance_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_analyze_statement_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index);
static int validate_histogram_column_list_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 size_t *next_token_index);
static int token_can_start_histogram_column_name(const mylite_parser *parser, size_t token_index);
static int validate_check_statement_syntax(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index);
static int validate_checksum_statement_syntax(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index);
static int validate_optimize_statement_syntax(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index);
static int validate_repair_statement_syntax(const mylite_parser *parser,
                                            size_t token_index,
                                            size_t last_token_index);
static int validate_maintenance_table_name_list_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index);
static int token_can_start_maintenance_table_name(const mylite_parser *parser, size_t token_index);
static int token_is_maintenance_table_keyword(const mylite_parser *parser, size_t token_index);
static int token_is_check_table_option(const mylite_parser *parser, size_t token_index);
static int token_is_checksum_table_option(const mylite_parser *parser, size_t token_index);
static int token_is_repair_table_option(const mylite_parser *parser, size_t token_index);
static int validate_single_token_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_savepoint_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_release_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_commit_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_rollback_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_rollback_savepoint_clause(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index);
static int validate_principal_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static size_t find_principal_statement_marker(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index,
                                              int marker_token);
static int validate_principal_target_list_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 int is_grant,
                                                 size_t *next_token_index);
static int validate_principal_statement_tail_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    int is_grant);
static int token_starts_principal_statement_tail(const mylite_parser *parser,
                                                 size_t token_index,
                                                 int is_grant);
static int validate_start_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_start_transaction_statement_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index);
static int validate_start_transaction_characteristic_syntax(const mylite_parser *parser,
                                                            size_t token_index,
                                                            size_t last_token_index,
                                                            size_t *next_token_index,
                                                            int *seen_snapshot,
                                                            int *seen_read_mode);
static int validate_start_replica_statement_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index);
static int validate_start_replica_until_clause_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index,
                                                      size_t *next_token_index);
static int validate_start_replica_log_position_until_clause_syntax(const mylite_parser *parser,
                                                                   size_t token_index,
                                                                   size_t last_token_index,
                                                                   const char *log_file_option,
                                                                   const char *log_pos_option,
                                                                   size_t *next_token_index);
static int validate_start_replica_gtid_until_clause_syntax(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index,
                                                           size_t *next_token_index);
static int validate_start_replica_connection_option_syntax(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index,
                                                           size_t *next_token_index,
                                                           int *seen_user,
                                                           int *seen_password,
                                                           int *seen_default_auth,
                                                           int *seen_plugin_dir);
static int token_is_start_replica_tail_boundary(const mylite_parser *parser, size_t token_index);
static int token_is_start_replica_gtid_set_token(const mylite_parser *parser, size_t token_index);
static int token_is_start_replica_connection_option(const mylite_parser *parser, size_t token_index);
static int validate_start_group_replication_statement_syntax(const mylite_parser *parser,
                                                             size_t token_index,
                                                             size_t last_token_index);
static int validate_start_group_replication_connection_option_syntax(const mylite_parser *parser,
                                                                     size_t token_index,
                                                                     size_t last_token_index,
                                                                     const char *option,
                                                                     size_t *next_token_index);
static int validate_stop_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_stop_replica_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int validate_replication_thread_type_list_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index,
                                                        size_t *next_token_index,
                                                        int *seen_io_thread,
                                                        int *seen_sql_thread);
static int validate_replication_channel_clause_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index,
                                                      size_t *next_token_index);
static int token_is_replication_thread_type(const mylite_parser *parser, size_t token_index);
static int validate_transaction_completion_clause(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int validate_prepare_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_execute_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_deallocate_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_drop_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_drop_prepare_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_drop_database_statement_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index);
static int validate_drop_stored_object_statement_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index);
static int validate_drop_server_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index);
static int validate_drop_spatial_reference_system_statement_syntax(const mylite_parser *parser,
                                                                   size_t token_index,
                                                                   size_t last_token_index);
static int validate_drop_tablespace_statement_syntax(const mylite_parser *parser,
                                                     size_t token_index,
                                                     size_t last_token_index,
                                                     int is_undo_tablespace);
static int validate_drop_logfile_group_statement_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index);
static int validate_storage_engine_tail_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index);
static int validate_storage_engine_clause_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 size_t *next_token_index);
static int validate_drop_principal_statement_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    int allow_current_user);
static int validate_principal_name_list_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index,
                                               int allow_current_user);
static int validate_principal_name_syntax(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index,
                                          int allow_current_user,
                                          size_t *next_token_index);
static int validate_drop_resource_group_statement_syntax(const mylite_parser *parser,
                                                         size_t token_index,
                                                         size_t last_token_index);
static int validate_drop_index_statement_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index);
static int validate_drop_index_option_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index,
                                             size_t *next_token_index);
static int validate_drop_table_or_view_statement_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index);
static int validate_drop_object_name_list_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 size_t *next_token_index);
static int token_is_drop_index_algorithm_value(const mylite_parser *parser, size_t token_index);
static int token_is_drop_index_lock_value(const mylite_parser *parser, size_t token_index);
static int token_is_storage_engine_name(const mylite_parser *parser, size_t token_index);
static int token_is_logfile_group_sequence(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index);
static int token_is_undo_tablespace_sequence(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index);
static int token_is_tablespace_token(const mylite_parser *parser, size_t token_index);
static int token_is_spatial_reference_system_sequence(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index);
static int token_is_drop_stored_object_token(int token);
static int token_is_drop_resource_group_token(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index);
static int token_is_drop_table_token(const mylite_parser *parser, size_t token_index);
static int token_is_drop_table_or_view_token(const mylite_parser *parser, size_t token_index);
static int token_is_drop_table_tail_option(const mylite_parser *parser, size_t token_index);
static int validate_kill_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_help_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_clone_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_clone_local_statement_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index);
static int validate_clone_remote_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index);
static int validate_clone_endpoint_syntax(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index,
                                          size_t *next_token_index);
static int validate_clone_data_directory_clause_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index);
static int token_can_be_clone_account_name(const mylite_parser *parser, size_t token_index);
static int token_can_be_clone_host_name(const mylite_parser *parser, size_t token_index);
static int validate_xa_statement_syntax(const mylite_parser *parser, const mylite_statement *statement);
static int validate_xa_xid_syntax(const mylite_parser *parser,
                                  size_t token_index,
                                  size_t last_token_index,
                                  size_t *next_token_index);
static int token_is_xa_string_value(const mylite_parser *parser, size_t token_index);
static int token_is_xa_format_id(const mylite_parser *parser, size_t token_index);
static int token_is_xa_prefixed_number_literal(const mylite_parser *parser, size_t token_index);
static void classify_statement_metadata(mylite_parser *parser);
static void classify_grouped_query_statement_kinds(mylite_parser *parser);
static mylite_statement_kind classify_grouped_query_statement_kind(const mylite_parser *parser,
                                                                   const mylite_statement *statement);
static mylite_statement_kind query_statement_kind_from_token(int token);
static void classify_with_statement_kinds(mylite_parser *parser);
static mylite_statement_kind classify_with_statement_kind(const mylite_parser *parser,
                                                          const mylite_statement *statement);
static void classify_labeled_statement_metadata(mylite_parser *parser);
static int classify_labeled_statement(mylite_parser *parser, mylite_statement *statement);
static mylite_statement_kind labeled_statement_kind_from_token(int token);
static void classify_statement_objects(mylite_parser *parser);
static void classify_statement_object(const mylite_parser *parser, mylite_statement *statement);
static size_t last_definer_clause_token(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index);
static int token_can_start_definer_account_name(const mylite_token *token);
static int classify_dml_statement_object(const mylite_parser *parser, mylite_statement *statement);
static size_t find_statement_kind_token(const mylite_parser *parser, const mylite_statement *statement);
static size_t find_insert_or_replace_name_token(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index);
static size_t find_update_name_token(const mylite_parser *parser,
                                     size_t token_index,
                                     size_t last_token_index);
static size_t find_table_reference_name_token(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index,
                                              int stop_token);
static int group_starts_query_expression(const mylite_parser *parser,
                                         size_t open_token_index);
static size_t skip_table_reference_alias(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index);
static size_t find_delete_name_token(const mylite_parser *parser,
                                     size_t token_index,
                                     size_t last_token_index);
static size_t skip_dml_modifiers(const mylite_parser *parser,
                                 size_t token_index,
                                 size_t last_token_index);
static int is_dml_modifier_token(int token);
static int classify_direct_statement_object(const mylite_parser *parser, mylite_statement *statement);
static int classify_select_statement_object(const mylite_parser *parser, mylite_statement *statement);
static size_t find_select_into_target_token(const mylite_parser *parser,
                                            const mylite_statement *statement,
                                            mylite_statement_object_kind *object_kind);
static size_t find_into_target_token(const mylite_parser *parser,
                                     size_t into_token_index,
                                     size_t last_token_index,
                                     mylite_statement_object_kind *object_kind);
static size_t find_import_sdi_file_token(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index);
static size_t find_call_procedure_name_token(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index);
static int classify_signal_statement_object(const mylite_parser *parser,
                                            mylite_statement *statement,
                                            size_t token_index,
                                            size_t last_token_index);
static int classify_condition_value_statement_object(const mylite_parser *parser,
                                                     mylite_statement *statement,
                                                     size_t token_index,
                                                     size_t last_token_index,
                                                     int allow_condition_class);
static int classify_get_statement_object(const mylite_parser *parser,
                                         mylite_statement *statement,
                                         size_t token_index,
                                         size_t last_token_index);
static int classify_label_target_statement_object(const mylite_parser *parser,
                                                  mylite_statement *statement,
                                                  size_t token_index,
                                                  size_t last_token_index);
static size_t find_get_diagnostics_target_token(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index);
static int token_can_start_diagnostics_condition_number(const mylite_token *token);
static int classify_describe_or_explain_statement_object(const mylite_parser *parser,
                                                         mylite_statement *statement,
                                                         size_t token_index,
                                                         size_t last_token_index);
static size_t find_explain_into_target_token(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index);
static size_t find_explain_connection_id_token(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index);
static size_t find_explainable_statement_token(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index);
static int token_is_explainable_statement_head(int token);
static int classify_table_statement_object(const mylite_parser *parser,
                                           mylite_statement *statement,
                                           size_t token_index,
                                           size_t last_token_index);
static size_t find_table_into_target_token(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index,
                                           mylite_statement_object_kind *object_kind);
static size_t find_table_statement_name_token(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index);
static size_t find_describe_or_explain_table_name_token(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index);
static size_t find_load_table_name_token(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index);
static size_t find_cache_index_name_token(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index);
static size_t find_lock_table_name_token(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index);
static size_t find_flush_table_name_token(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index);
static int classify_flush_statement_object(const mylite_parser *parser,
                                           mylite_statement *statement,
                                           size_t token_index,
                                           size_t last_token_index);
static size_t find_maintenance_table_name_token(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index);
static int classify_instance_statement_object(const mylite_parser *parser,
                                              mylite_statement *statement,
                                              size_t token_index,
                                              size_t last_token_index);
static int classify_kill_statement_object(const mylite_parser *parser,
                                          mylite_statement *statement,
                                          size_t token_index,
                                          size_t last_token_index);
static int token_can_start_processlist_id(const mylite_token *token);
static int token_can_start_processlist_expression(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int processlist_expression_is_single_target(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index);
static int classify_clone_statement_object(const mylite_parser *parser,
                                           mylite_statement *statement,
                                           size_t token_index,
                                           size_t last_token_index);
static size_t find_data_directory_value_token(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index);
static size_t find_clone_endpoint_last_token(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index);
static int classify_purge_statement_object(const mylite_parser *parser,
                                           mylite_statement *statement,
                                           size_t token_index,
                                           size_t last_token_index);
static int classify_replication_channel_statement_object(const mylite_parser *parser,
                                                         mylite_statement *statement,
                                                         size_t token_index,
                                                         size_t last_token_index);
static int is_replication_channel_operation(const mylite_parser *parser,
                                            mylite_statement_kind statement_kind,
                                            size_t token_index,
                                            size_t last_token_index);
static int classify_reset_statement_object(const mylite_parser *parser,
                                           mylite_statement *statement,
                                           size_t token_index,
                                           size_t last_token_index);
static size_t find_replication_channel_name_token(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int has_replication_channel_clause(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index);
static int classify_transaction_statement_object(const mylite_parser *parser,
                                                 mylite_statement *statement,
                                                 size_t token_index,
                                                 size_t last_token_index);
static int classify_set_statement_object(const mylite_parser *parser,
                                         mylite_statement *statement,
                                         size_t token_index,
                                         size_t last_token_index);
static size_t find_set_role_name_token(const mylite_parser *parser,
                                       size_t token_index,
                                       size_t last_token_index);
static int is_set_role_collection_target(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index);
static size_t find_set_default_role_user_name_token(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index);
static size_t find_set_password_name_token(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index);
static int classify_set_character_set_statement_object(const mylite_parser *parser,
                                                       mylite_statement *statement,
                                                       size_t token_index,
                                                       size_t last_token_index);
static size_t find_set_system_variable_name_token(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int token_can_start_set_system_variable_name(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index);
static int classify_install_statement_object(const mylite_parser *parser,
                                             mylite_statement *statement,
                                             size_t token_index,
                                             size_t last_token_index);
static int classify_xa_statement_object(const mylite_parser *parser,
                                        mylite_statement *statement,
                                        size_t token_index,
                                        size_t last_token_index);
static int classify_prepared_statement_object(const mylite_parser *parser,
                                              mylite_statement *statement,
                                              size_t token_index,
                                              size_t last_token_index);
static int classify_principal_statement_object(const mylite_parser *parser,
                                               mylite_statement *statement,
                                               size_t token_index,
                                               size_t last_token_index);
static int classify_savepoint_statement_object(const mylite_parser *parser,
                                               mylite_statement *statement,
                                               size_t token_index,
                                               size_t last_token_index);
static size_t find_rollback_savepoint_name_token(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index);
static int classify_declare_statement_object(const mylite_parser *parser,
                                             mylite_statement *statement,
                                             size_t token_index,
                                             size_t last_token_index);
static size_t find_declare_handler_condition_token(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index);
static int classify_cursor_statement_object(const mylite_parser *parser,
                                            mylite_statement *statement,
                                            size_t token_index,
                                            size_t last_token_index);
static size_t find_cursor_name_token(const mylite_parser *parser,
                                     mylite_statement_kind statement_kind,
                                     size_t token_index,
                                     size_t last_token_index);
static int statement_contains_token(const mylite_parser *parser,
                                    size_t token_index,
                                    size_t last_token_index,
                                    int wanted_token);
static size_t find_savepoint_name_token(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index);
static size_t find_principal_name_token(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index,
                                        int marker_token);
static void set_statement_account_name_from_first_token(const mylite_parser *parser,
                                                        mylite_statement *statement,
                                                        size_t first_name_token,
                                                        size_t last_token_index);
static size_t last_account_name_token(const mylite_parser *parser,
                                      size_t first_name_token,
                                      size_t last_token_index);
static int token_is_account_name_clause_boundary(const mylite_parser *parser, size_t token_index);
static int token_is_current_user_function_name(const mylite_parser *parser, size_t token_index);
static int token_pair_is_empty_parentheses(const mylite_parser *parser, size_t open_token_index);
static int token_is_account_host_suffix(const mylite_parser *parser, size_t token_index);
static int token_is_account_at_marker(const mylite_parser *parser, size_t token_index);
static int classify_show_statement_object(const mylite_parser *parser,
                                          mylite_statement *statement,
                                          size_t token_index,
                                          size_t last_token_index);
static int classify_show_diagnostics_statement_object(const mylite_parser *parser,
                                                      mylite_statement *statement,
                                                      size_t token_index,
                                                      size_t last_token_index);
static int classify_show_database_statement_object(const mylite_parser *parser,
                                                   mylite_statement *statement,
                                                   size_t token_index,
                                                   size_t last_token_index);
static int classify_show_collection_statement_object(const mylite_parser *parser,
                                                     mylite_statement *statement,
                                                     size_t token_index,
                                                     size_t last_token_index);
static int classify_show_binary_log_statement_object(const mylite_parser *parser,
                                                     mylite_statement *statement,
                                                     size_t token_index,
                                                     size_t last_token_index);
static int classify_show_replica_statement_object(const mylite_parser *parser,
                                                  mylite_statement *statement,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int classify_show_routine_status_statement_object(const mylite_parser *parser,
                                                         mylite_statement *statement,
                                                         size_t token_index,
                                                         size_t last_token_index);
static int classify_show_variable_statement_object(const mylite_parser *parser,
                                                   mylite_statement *statement,
                                                   size_t token_index,
                                                   size_t last_token_index);
static int classify_show_character_set_statement_object(const mylite_parser *parser,
                                                        mylite_statement *statement,
                                                        size_t token_index,
                                                        size_t last_token_index);
static int classify_show_schema_collection_target(const mylite_parser *parser,
                                                  mylite_statement *statement,
                                                  size_t first_filter_token,
                                                  size_t last_token_index,
                                                  mylite_statement_object_kind collection_object_kind,
                                                  mylite_statement_object_kind like_pattern_object_kind);
static size_t find_show_profile_query_id_token(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index);
static int has_show_profile_for_query_clause(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index);
static size_t find_show_like_pattern_token(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index);
static size_t find_show_binlog_events_name_token(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index);
static size_t skip_show_modifiers(const mylite_parser *parser,
                                  size_t token_index,
                                  size_t last_token_index);
static int is_show_modifier_token(const mylite_parser *parser, size_t token_index);
static size_t find_show_from_name_token(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index);
static size_t find_show_grants_name_token(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index);
static int set_statement_direct_object(mylite_statement *statement,
                                       mylite_statement_object_kind object_kind);
static int set_statement_direct_object_name(const mylite_parser *parser,
                                            mylite_statement *statement,
                                            mylite_statement_object_kind object_kind,
                                            size_t name_token_index,
                                            size_t last_token_index);
static int set_statement_direct_object_name_range(const mylite_parser *parser,
                                                  mylite_statement *statement,
                                                  mylite_statement_object_kind object_kind,
                                                  size_t first_name_token,
                                                  size_t last_name_token);
static mylite_statement_object_kind object_kind_from_token_sequence(const mylite_parser *parser,
                                                                    size_t token_index,
                                                                    size_t last_token_index);
static void set_statement_object_name(const mylite_parser *parser,
                                      mylite_statement *statement,
                                      size_t object_token_index,
                                      size_t last_token_index);
static int token_starts_alter_database_option(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index);
static void set_statement_object_name_from_first_token(const mylite_parser *parser,
                                                       mylite_statement *statement,
                                                       size_t first_name_token,
                                                       size_t last_token_index);
static size_t first_name_token_after_object(const mylite_parser *parser,
                                            size_t object_token_index,
                                            size_t last_token_index);
static size_t last_qualified_name_token(const mylite_parser *parser,
                                        size_t first_name_token,
                                        size_t last_token_index);
static int token_can_start_object_name(const mylite_token *token);
static int token_can_continue_object_name(const mylite_token *token);
static int token_can_continue_qualified_object_name(const mylite_token *token);
static int token_can_be_unquoted_object_name_keyword(int token);
static mylite_statement_object_kind variable_object_kind_from_token(const mylite_token *token);
static int token_can_start_local_variable_name(const mylite_token *token);
static int token_can_be_unquoted_local_name_keyword(int token);
static int token_can_start_label_name(const mylite_token *token);
static int token_can_be_unquoted_label_keyword(int token);
static int is_optional_name_modifier(int token);
static int token_text_equals(const mylite_parser *parser, size_t token_index, const char *expected);
static int token_is_assignment_operator(const mylite_parser *parser, size_t token_index);
static int statement_kind_uses_object_scan(mylite_statement_kind kind);
static mylite_token_kind token_kind_from_parser_token(int token);

void mylite_parser_init(mylite_parser *parser, const char *sql, size_t length)
{
	memset(parser, 0, sizeof(*parser));
	mylite_lexer_init(&parser->lexer, sql, length);
}

void mylite_parser_destroy(mylite_parser *parser)
{
	free(parser->tokens);
	free(parser->statements);
	parser->tokens = NULL;
	parser->token_count = 0;
	parser->token_capacity = 0;
	parser->statements = NULL;
	parser->statement_count = 0;
	parser->statement_capacity = 0;
}

void mylite_parser_begin_statement(mylite_parser *parser, mylite_statement_kind kind, int requires_body)
{
	mylite_parser_begin_statement_at_token(parser, kind, requires_body, parser->lexer.token_count);
}

void mylite_parser_begin_statement_at_token(mylite_parser *parser,
                                            mylite_statement_kind kind,
                                            int requires_body,
                                            size_t first_token)
{
	parser->active_statement_kind = kind;
	parser->active_statement_first_token = first_token;
	if (first_token > 0 && first_token <= parser->token_count) {
		const mylite_token *token = &parser->tokens[first_token - 1];
		parser->active_statement_start_offset = token->start_offset;
		parser->active_statement_start_line = token->start_line;
		parser->active_statement_start_column = token->start_column;
	} else {
		parser->active_statement_first_token = parser->lexer.token_count;
		parser->active_statement_start_offset = parser->lexer.token_start_offset;
		parser->active_statement_start_line = parser->lexer.token_start_line;
		parser->active_statement_start_column = parser->lexer.token_start_column;
	}
	parser->active_statement_body_items = 0;
	parser->active_statement_requires_body = requires_body;
}

int mylite_parser_record_token(mylite_parser *parser, int token)
{
	mylite_token *tokens;
	size_t new_capacity;

	if (parser->token_count == parser->token_capacity) {
		new_capacity = parser->token_capacity == 0 ? 32 : parser->token_capacity * 2;
		tokens = (mylite_token *)realloc(parser->tokens, new_capacity * sizeof(parser->tokens[0]));
		if (tokens == NULL) {
			mylite_parser_set_error(parser, "out of memory");
			return 0;
		}
		parser->tokens = tokens;
		parser->token_capacity = new_capacity;
	}

	parser->tokens[parser->token_count].kind = token_kind_from_parser_token(token);
	parser->tokens[parser->token_count].parser_token = token;
	parser->tokens[parser->token_count].matching_token = 0;
	parser->tokens[parser->token_count].start_offset = parser->lexer.token_start_offset;
	parser->tokens[parser->token_count].end_offset = parser->lexer.token_end_offset;
	parser->tokens[parser->token_count].start_line = parser->lexer.token_start_line;
	parser->tokens[parser->token_count].start_column = parser->lexer.token_start_column;
	parser->tokens[parser->token_count].end_line = parser->lexer.token_end_line;
	parser->tokens[parser->token_count].end_column = parser->lexer.token_end_column;
	parser->token_count++;
	return 1;
}

void mylite_parser_match_tokens(mylite_parser *parser, size_t left_token, size_t right_token)
{
	if (left_token == 0 || right_token == 0 ||
	    left_token > parser->token_count || right_token > parser->token_count) {
		return;
	}
	parser->tokens[left_token - 1].matching_token = right_token;
	parser->tokens[right_token - 1].matching_token = left_token;
}

int mylite_parser_add_statement(mylite_parser *parser, mylite_statement_kind kind)
{
	mylite_statement *statements;
	size_t new_capacity;

	if (parser->active_statement_requires_body && parser->active_statement_body_items == 0) {
		mylite_parser_set_error(parser, "statement requires a body");
		return 0;
	}
	if (parser->statement_count == parser->statement_capacity) {
		new_capacity = parser->statement_capacity == 0 ? 4 : parser->statement_capacity * 2;
		statements = (mylite_statement *)realloc(parser->statements,
		                                        new_capacity * sizeof(parser->statements[0]));
		if (statements == NULL) {
			mylite_parser_set_error(parser, "out of memory");
			return 0;
		}
		parser->statements = statements;
		parser->statement_capacity = new_capacity;
	}

	parser->statements[parser->statement_count].kind = kind;
	parser->statements[parser->statement_count].object_kind = MYLITE_STATEMENT_OBJECT_NONE;
	parser->statements[parser->statement_count].first_token = parser->active_statement_first_token;
	parser->statements[parser->statement_count].last_token = parser->lexer.last_significant_token;
	parser->statements[parser->statement_count].object_name_first_token = 0;
	parser->statements[parser->statement_count].object_name_last_token = 0;
	parser->statements[parser->statement_count].start_offset = parser->active_statement_start_offset;
	parser->statements[parser->statement_count].end_offset = parser->lexer.last_significant_token_end_offset;
	parser->statements[parser->statement_count].object_name_start_offset = 0;
	parser->statements[parser->statement_count].object_name_end_offset = 0;
	parser->statements[parser->statement_count].start_line = parser->active_statement_start_line;
	parser->statements[parser->statement_count].start_column = parser->active_statement_start_column;
	parser->statements[parser->statement_count].end_line = parser->lexer.last_significant_token_end_line;
	parser->statements[parser->statement_count].end_column = parser->lexer.last_significant_token_end_column;
	parser->statements[parser->statement_count].object_name_start_line = 0;
	parser->statements[parser->statement_count].object_name_start_column = 0;
	parser->statements[parser->statement_count].object_name_end_line = 0;
	parser->statements[parser->statement_count].object_name_end_column = 0;
	parser->statement_count++;
	return 1;
}

void mylite_parser_set_error(mylite_parser *parser, const char *message)
{
	if (parser->error[0] != '\0') {
		return;
	}
	if (parser->lexer.error[0] != '\0') {
		snprintf(parser->error, sizeof(parser->error), "%s", parser->lexer.error);
		parser->error_line = parser->lexer.error_line;
		parser->error_column = parser->lexer.error_column;
		return;
	}
	snprintf(parser->error, sizeof(parser->error), "%s", message);
	parser->error_line = parser->lexer.line;
	parser->error_column = parser->lexer.column;
}

int mylite_parse_sql(const char *sql, size_t length, mylite_parse_result *result)
{
	mylite_parser parser;
	int rc;

	memset(result, 0, sizeof(*result));
	mylite_parser_init(&parser, sql, length);
	rc = yyparse(&parser);
	if (parser.lexer.error[0] != '\0') {
		mylite_parser_set_error(&parser, parser.lexer.error);
	}
	if (parser.error[0] == '\0') {
		match_compound_control_tokens(&parser);
		merge_compound_control_statement_spans(&parser);
		validate_statement_syntax(&parser);
	}
	if (parser.error[0] == '\0') {
		classify_statement_metadata(&parser);
	}

	result->ok = rc == 0 && parser.error[0] == '\0';
	result->token_count = parser.token_count;
	if (parser.token_count > 0) {
		result->tokens = (mylite_token *)malloc(parser.token_count * sizeof(parser.tokens[0]));
		if (result->tokens == NULL) {
			result->ok = 0;
			result->token_count = 0;
			snprintf(result->error, sizeof(result->error), "out of memory");
			result->error_line = parser.lexer.line;
			result->error_column = parser.lexer.column;
			mylite_parser_destroy(&parser);
			return 0;
		}
		memcpy(result->tokens, parser.tokens, parser.token_count * sizeof(parser.tokens[0]));
	}
	result->statement_count = parser.statement_count;
	if (parser.statement_count > 0) {
		result->statements = (mylite_statement *)malloc(parser.statement_count * sizeof(parser.statements[0]));
		if (result->statements == NULL) {
			result->ok = 0;
			snprintf(result->error, sizeof(result->error), "out of memory");
			result->error_line = parser.lexer.line;
			result->error_column = parser.lexer.column;
			free(result->tokens);
			result->tokens = NULL;
			result->token_count = 0;
			mylite_parser_destroy(&parser);
			return 0;
		}
		memcpy(result->statements, parser.statements, parser.statement_count * sizeof(parser.statements[0]));
	}

	if (!result->ok) {
		snprintf(result->error, sizeof(result->error), "%s",
		         parser.error[0] == '\0' ? "syntax error" : parser.error);
		result->error_line = parser.error_line == 0 ? parser.lexer.line : parser.error_line;
		result->error_column = parser.error_column == 0 ? parser.lexer.column : parser.error_column;
	}

	mylite_parser_destroy(&parser);
	return result->ok;
}

static void match_compound_control_tokens(mylite_parser *parser)
{
	size_t *stack;
	size_t stack_size = 0;
	size_t token_index;

	if (parser->token_count == 0) {
		return;
	}

	stack = (size_t *)malloc(parser->token_count * sizeof(stack[0]));
	if (stack == NULL) {
		mylite_parser_set_error(parser, "out of memory");
		return;
	}

	for (token_index = 0; token_index < parser->token_count; token_index++) {
		int token = parser->tokens[token_index].parser_token;

		if (is_compound_control_start_token(parser, token_index)) {
			stack[stack_size++] = token_index;
			continue;
		}
		if (is_compound_control_end_token(token)) {
			while (stack_size > 0) {
				size_t start_token_index = stack[--stack_size];
				int start_token = parser->tokens[start_token_index].parser_token;
				if (compound_control_tokens_match(start_token, token)) {
					mylite_parser_match_tokens(parser, start_token_index + 1, token_index + 1);
					break;
				}
			}
		}
	}

	free(stack);
}

static int is_compound_control_start_token(const mylite_parser *parser, size_t token_index)
{
	int token = parser->tokens[token_index].parser_token;

	switch (token) {
	case BEGIN_T:
		return parser->tokens[token_index].matching_token == 0 &&
		       !is_begin_work_statement(parser, token_index);
	case IF_T:
		return !is_if_function_call(parser, token_index) &&
		       !is_if_exists_clause(parser, token_index);
	case CASE_T:
		return parser->tokens[token_index].matching_token == 0;
	case LOOP_T:
	case REPEAT_T:
	case WHILE_T:
		return parser->tokens[token_index].matching_token == 0;
	default:
		return 0;
	}
}

static int is_begin_work_statement(const mylite_parser *parser, size_t token_index)
{
	return token_index + 1 < parser->token_count &&
	       token_text_equals(parser, token_index + 1, "WORK");
}

static int is_if_function_call(const mylite_parser *parser, size_t token_index)
{
	return token_index + 1 < parser->token_count &&
	       parser->tokens[token_index + 1].parser_token == '(';
}

static int is_if_exists_clause(const mylite_parser *parser, size_t token_index)
{
	if (token_index + 1 < parser->token_count &&
	    parser->tokens[token_index + 1].parser_token == EXISTS_T) {
		return 1;
	}
	return token_index + 2 < parser->token_count &&
	       parser->tokens[token_index + 1].parser_token == NOT_T &&
	       parser->tokens[token_index + 2].parser_token == EXISTS_T;
}

static int is_compound_control_end_token(int token)
{
	return token == END_T ||
	       token == END_IF_T ||
	       token == END_CASE_T ||
	       token == END_LOOP_T ||
	       token == END_REPEAT_T ||
	       token == END_WHILE_T;
}

static int compound_control_tokens_match(int start_token, int end_token)
{
	return (start_token == BEGIN_T && end_token == END_T) ||
	       (start_token == IF_T && end_token == END_IF_T) ||
	       (start_token == CASE_T && end_token == END_CASE_T) ||
	       (start_token == LOOP_T && end_token == END_LOOP_T) ||
	       (start_token == REPEAT_T && end_token == END_REPEAT_T) ||
	       (start_token == WHILE_T && end_token == END_WHILE_T);
}

static void merge_compound_control_statement_spans(mylite_parser *parser)
{
	size_t statement_index = 0;

	while (statement_index < parser->statement_count) {
		mylite_statement *statement = &parser->statements[statement_index];
		size_t end_token;

		if (!statement_starts_with_matched_compound_control(parser, statement, &end_token)) {
			statement_index++;
			continue;
		}

		end_token = last_compound_control_statement_token(parser, end_token);
		set_statement_end_from_token(parser, statement, end_token);
		remove_statements_covered_by_previous(parser, statement_index);
		statement_index++;
	}
}

static int statement_starts_with_matched_compound_control(const mylite_parser *parser,
                                                          const mylite_statement *statement,
                                                          size_t *end_token)
{
	const mylite_token *first_token;
	size_t first_token_index;
	size_t matching_token;

	if (statement->first_token == 0 ||
	    statement->first_token > parser->token_count ||
	    statement->last_token < statement->first_token) {
		return 0;
	}

	first_token_index = statement->first_token - 1;
	if (first_token_index + 2 < parser->token_count &&
	    token_can_start_label_name(&parser->tokens[first_token_index]) &&
	    token_text_equals(parser, first_token_index + 1, ":")) {
		first_token_index += 2;
	}

	first_token = &parser->tokens[first_token_index];
	matching_token = first_token->matching_token;
	if (matching_token == 0 ||
	    matching_token <= statement->last_token ||
	    matching_token > parser->token_count ||
	    !compound_control_tokens_match(first_token->parser_token,
	                                   parser->tokens[matching_token - 1].parser_token)) {
		return 0;
	}

	*end_token = matching_token - 1;
	return 1;
}

static size_t last_compound_control_statement_token(const mylite_parser *parser, size_t end_token)
{
	size_t label_token = end_token + 1;

	if (compound_control_end_allows_label(parser->tokens[end_token].parser_token) &&
	    label_token < parser->token_count &&
	    token_can_start_label_name(&parser->tokens[label_token])) {
		return label_token;
	}
	return end_token;
}

static int compound_control_end_allows_label(int token)
{
	return token == END_T ||
	       token == END_LOOP_T ||
	       token == END_REPEAT_T ||
	       token == END_WHILE_T;
}

static void set_statement_end_from_token(const mylite_parser *parser,
                                         mylite_statement *statement,
                                         size_t token_index)
{
	const mylite_token *token = &parser->tokens[token_index];

	statement->last_token = token_index + 1;
	statement->end_offset = token->end_offset;
	statement->end_line = token->end_line;
	statement->end_column = token->end_column;
}

static void remove_statements_covered_by_previous(mylite_parser *parser, size_t statement_index)
{
	size_t first_covered = statement_index + 1;
	size_t first_uncovered = first_covered;
	size_t covered_count;

	while (first_uncovered < parser->statement_count &&
	       parser->statements[first_uncovered].first_token <= parser->statements[statement_index].last_token) {
		first_uncovered++;
	}

	covered_count = first_uncovered - first_covered;
	if (covered_count == 0) {
		return;
	}

	memmove(&parser->statements[first_covered],
	        &parser->statements[first_uncovered],
	        (parser->statement_count - first_uncovered) * sizeof(parser->statements[0]));
	parser->statement_count -= covered_count;
}

void mylite_parse_result_free(mylite_parse_result *result)
{
	free(result->tokens);
	free(result->statements);
	memset(result, 0, sizeof(*result));
}

const char *mylite_statement_kind_name(mylite_statement_kind kind)
{
	switch (kind) {
	case MYLITE_STATEMENT_SELECT: return "select";
	case MYLITE_STATEMENT_INSERT: return "insert";
	case MYLITE_STATEMENT_REPLACE: return "replace";
	case MYLITE_STATEMENT_UPDATE: return "update";
	case MYLITE_STATEMENT_DELETE: return "delete";
	case MYLITE_STATEMENT_CREATE: return "create";
	case MYLITE_STATEMENT_ALTER: return "alter";
	case MYLITE_STATEMENT_DROP: return "drop";
	case MYLITE_STATEMENT_TRUNCATE: return "truncate";
	case MYLITE_STATEMENT_RENAME: return "rename";
	case MYLITE_STATEMENT_CALL: return "call";
	case MYLITE_STATEMENT_DO: return "do";
	case MYLITE_STATEMENT_HANDLER: return "handler";
	case MYLITE_STATEMENT_IMPORT: return "import";
	case MYLITE_STATEMENT_LOAD: return "load";
	case MYLITE_STATEMENT_TABLE: return "table";
	case MYLITE_STATEMENT_VALUES: return "values";
	case MYLITE_STATEMENT_SET: return "set";
	case MYLITE_STATEMENT_SHOW: return "show";
	case MYLITE_STATEMENT_USE: return "use";
	case MYLITE_STATEMENT_DESCRIBE: return "describe";
	case MYLITE_STATEMENT_EXPLAIN: return "explain";
	case MYLITE_STATEMENT_HELP: return "help";
	case MYLITE_STATEMENT_START: return "start";
	case MYLITE_STATEMENT_STOP: return "stop";
	case MYLITE_STATEMENT_BEGIN: return "begin";
	case MYLITE_STATEMENT_COMMIT: return "commit";
	case MYLITE_STATEMENT_ROLLBACK: return "rollback";
	case MYLITE_STATEMENT_SAVEPOINT: return "savepoint";
	case MYLITE_STATEMENT_RELEASE: return "release";
	case MYLITE_STATEMENT_LOCK: return "lock";
	case MYLITE_STATEMENT_UNLOCK: return "unlock";
	case MYLITE_STATEMENT_XA: return "xa";
	case MYLITE_STATEMENT_PREPARE: return "prepare";
	case MYLITE_STATEMENT_EXECUTE: return "execute";
	case MYLITE_STATEMENT_DEALLOCATE: return "deallocate";
	case MYLITE_STATEMENT_ANALYZE: return "analyze";
	case MYLITE_STATEMENT_CHECK: return "check";
	case MYLITE_STATEMENT_CHECKSUM: return "checksum";
	case MYLITE_STATEMENT_OPTIMIZE: return "optimize";
	case MYLITE_STATEMENT_REPAIR: return "repair";
	case MYLITE_STATEMENT_FLUSH: return "flush";
	case MYLITE_STATEMENT_KILL: return "kill";
	case MYLITE_STATEMENT_RESET: return "reset";
	case MYLITE_STATEMENT_RESTART: return "restart";
	case MYLITE_STATEMENT_SHUTDOWN: return "shutdown";
	case MYLITE_STATEMENT_GRANT: return "grant";
	case MYLITE_STATEMENT_REVOKE: return "revoke";
	case MYLITE_STATEMENT_INSTALL: return "install";
	case MYLITE_STATEMENT_UNINSTALL: return "uninstall";
	case MYLITE_STATEMENT_CACHE: return "cache";
	case MYLITE_STATEMENT_CLONE: return "clone";
	case MYLITE_STATEMENT_CHANGE: return "change";
	case MYLITE_STATEMENT_BINLOG: return "binlog";
	case MYLITE_STATEMENT_PURGE: return "purge";
	case MYLITE_STATEMENT_SIGNAL: return "signal";
	case MYLITE_STATEMENT_RESIGNAL: return "resignal";
	case MYLITE_STATEMENT_GET: return "get";
	case MYLITE_STATEMENT_DECLARE: return "declare";
	case MYLITE_STATEMENT_OPEN: return "open";
	case MYLITE_STATEMENT_FETCH: return "fetch";
	case MYLITE_STATEMENT_CLOSE: return "close";
	case MYLITE_STATEMENT_IF: return "if";
	case MYLITE_STATEMENT_CASE: return "case";
	case MYLITE_STATEMENT_LOOP: return "loop";
	case MYLITE_STATEMENT_REPEAT: return "repeat";
	case MYLITE_STATEMENT_WHILE: return "while";
	case MYLITE_STATEMENT_LEAVE: return "leave";
	case MYLITE_STATEMENT_ITERATE: return "iterate";
	case MYLITE_STATEMENT_RETURN: return "return";
	case MYLITE_STATEMENT_UNKNOWN:
	default:
		return "unknown";
	}
}

const char *mylite_statement_object_kind_name(mylite_statement_object_kind kind)
{
	switch (kind) {
	case MYLITE_STATEMENT_OBJECT_BINARY_LOG: return "binary_log";
	case MYLITE_STATEMENT_OBJECT_BINARY_LOG_EVENT: return "binary_log_event";
	case MYLITE_STATEMENT_OBJECT_CHARACTER_SET: return "character_set";
	case MYLITE_STATEMENT_OBJECT_COLLATION: return "collation";
	case MYLITE_STATEMENT_OBJECT_COMPONENT: return "component";
	case MYLITE_STATEMENT_OBJECT_CONDITION: return "condition";
	case MYLITE_STATEMENT_OBJECT_CONNECTION: return "connection";
	case MYLITE_STATEMENT_OBJECT_CURSOR: return "cursor";
	case MYLITE_STATEMENT_OBJECT_DATABASE: return "database";
	case MYLITE_STATEMENT_OBJECT_DIAGNOSTICS_CONDITION: return "diagnostics_condition";
	case MYLITE_STATEMENT_OBJECT_DIAGNOSTICS_AREA: return "diagnostics_area";
	case MYLITE_STATEMENT_OBJECT_DIRECTORY: return "directory";
	case MYLITE_STATEMENT_OBJECT_DUMPFILE: return "dumpfile";
	case MYLITE_STATEMENT_OBJECT_ENGINE: return "engine";
	case MYLITE_STATEMENT_OBJECT_ENGINE_LOG: return "engine_log";
	case MYLITE_STATEMENT_OBJECT_ERROR_LOG: return "error_log";
	case MYLITE_STATEMENT_OBJECT_EVENT: return "event";
	case MYLITE_STATEMENT_OBJECT_FUNCTION: return "function";
	case MYLITE_STATEMENT_OBJECT_GENERAL_LOG: return "general_log";
	case MYLITE_STATEMENT_OBJECT_GROUP_REPLICATION: return "group_replication";
	case MYLITE_STATEMENT_OBJECT_HELP_TOPIC: return "help_topic";
	case MYLITE_STATEMENT_OBJECT_HOST_CACHE: return "host_cache";
	case MYLITE_STATEMENT_OBJECT_INDEX: return "index";
	case MYLITE_STATEMENT_OBJECT_INSTANCE: return "instance";
	case MYLITE_STATEMENT_OBJECT_LABEL: return "label";
	case MYLITE_STATEMENT_OBJECT_LOG: return "log";
	case MYLITE_STATEMENT_OBJECT_LOGFILE_GROUP: return "logfile_group";
	case MYLITE_STATEMENT_OBJECT_LOCAL_VARIABLE: return "local_variable";
	case MYLITE_STATEMENT_OBJECT_OPTIMIZER_COST: return "optimizer_cost";
	case MYLITE_STATEMENT_OBJECT_OUTFILE: return "outfile";
	case MYLITE_STATEMENT_OBJECT_PLUGIN: return "plugin";
	case MYLITE_STATEMENT_OBJECT_PREPARED_STATEMENT: return "prepared_statement";
	case MYLITE_STATEMENT_OBJECT_PRIVILEGE: return "privilege";
	case MYLITE_STATEMENT_OBJECT_PROCEDURE: return "procedure";
	case MYLITE_STATEMENT_OBJECT_QUERY: return "query";
	case MYLITE_STATEMENT_OBJECT_RELAY_LOG: return "relay_log";
	case MYLITE_STATEMENT_OBJECT_REPLICATION_CHANNEL: return "replication_channel";
	case MYLITE_STATEMENT_OBJECT_RESOURCE_GROUP: return "resource_group";
	case MYLITE_STATEMENT_OBJECT_ROLE: return "role";
	case MYLITE_STATEMENT_OBJECT_SAVEPOINT: return "savepoint";
	case MYLITE_STATEMENT_OBJECT_SCHEMA: return "schema";
	case MYLITE_STATEMENT_OBJECT_SDI_FILE: return "sdi_file";
	case MYLITE_STATEMENT_OBJECT_SERVER: return "server";
	case MYLITE_STATEMENT_OBJECT_SPATIAL_REFERENCE_SYSTEM: return "spatial_reference_system";
	case MYLITE_STATEMENT_OBJECT_SQLSTATE: return "sqlstate";
	case MYLITE_STATEMENT_OBJECT_SLOW_LOG: return "slow_log";
	case MYLITE_STATEMENT_OBJECT_STATUS_VARIABLE: return "status_variable";
	case MYLITE_STATEMENT_OBJECT_SYSTEM_VARIABLE: return "system_variable";
	case MYLITE_STATEMENT_OBJECT_TABLE: return "table";
	case MYLITE_STATEMENT_OBJECT_TABLESPACE: return "tablespace";
	case MYLITE_STATEMENT_OBJECT_TRANSACTION: return "transaction";
	case MYLITE_STATEMENT_OBJECT_TRIGGER: return "trigger";
	case MYLITE_STATEMENT_OBJECT_UNDO_TABLESPACE: return "undo_tablespace";
	case MYLITE_STATEMENT_OBJECT_USER: return "user";
	case MYLITE_STATEMENT_OBJECT_USER_VARIABLE: return "user_variable";
	case MYLITE_STATEMENT_OBJECT_USER_RESOURCE: return "user_resource";
	case MYLITE_STATEMENT_OBJECT_VIEW: return "view";
	case MYLITE_STATEMENT_OBJECT_XA_TRANSACTION: return "xa_transaction";
	case MYLITE_STATEMENT_OBJECT_NONE:
	default:
		return "none";
	}
}

const char *mylite_token_kind_name(mylite_token_kind kind)
{
	switch (kind) {
	case MYLITE_TOKEN_IDENTIFIER: return "identifier";
	case MYLITE_TOKEN_QUOTED_IDENTIFIER: return "quoted_identifier";
	case MYLITE_TOKEN_STRING: return "string";
	case MYLITE_TOKEN_NUMBER: return "number";
	case MYLITE_TOKEN_PARAMETER: return "parameter";
	case MYLITE_TOKEN_USER_VARIABLE: return "user_variable";
	case MYLITE_TOKEN_SYSTEM_VARIABLE: return "system_variable";
	case MYLITE_TOKEN_OPERATOR: return "operator";
	case MYLITE_TOKEN_PUNCTUATION: return "punctuation";
	case MYLITE_TOKEN_KEYWORD: return "keyword";
	case MYLITE_TOKEN_UNKNOWN:
	default:
		return "unknown";
	}
}

static void validate_statement_syntax(mylite_parser *parser)
{
	size_t i;

	for (i = 0; i < parser->statement_count; i++) {
		const mylite_statement *statement = &parser->statements[i];

		switch (statement->kind) {
		case MYLITE_STATEMENT_INSERT:
		case MYLITE_STATEMENT_REPLACE:
			if (!validate_insert_or_replace_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid INSERT or REPLACE statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_UPDATE:
			if (!validate_update_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid UPDATE statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_DELETE:
			if (!validate_delete_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid DELETE statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_CREATE:
			if (!validate_create_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid CREATE statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_ALTER:
			if (!validate_alter_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid ALTER statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_USE:
			if (!validate_use_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid USE statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_TRUNCATE:
			if (!validate_truncate_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid TRUNCATE statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_RENAME:
			if (!validate_rename_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid RENAME statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_CALL:
			if (!validate_call_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid CALL statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_DO:
			if (!validate_do_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid DO statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_VALUES:
			if (!validate_values_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid VALUES statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_IMPORT:
			if (!validate_import_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid IMPORT statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_BINLOG:
			if (!validate_binlog_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid BINLOG statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_INSTALL:
			if (!validate_install_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid INSTALL statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_UNINSTALL:
			if (!validate_install_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid UNINSTALL statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_KILL:
			if (!validate_kill_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid KILL statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_HELP:
			if (!validate_help_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid HELP statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_CLONE:
			if (!validate_clone_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid CLONE statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_LOCK:
			if (!validate_lock_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid LOCK statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_UNLOCK:
			if (!validate_unlock_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid UNLOCK statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_OPEN:
		case MYLITE_STATEMENT_FETCH:
		case MYLITE_STATEMENT_CLOSE:
			if (!validate_cursor_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid cursor statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_HANDLER:
			if (!validate_handler_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid HANDLER statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_CACHE:
			if (!validate_cache_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid CACHE statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_LOAD:
			if (!validate_load_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid LOAD statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_PURGE:
			if (!validate_purge_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid PURGE statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_SET:
			if (!validate_set_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid SET statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_RESET:
			if (!validate_reset_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid RESET statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_FLUSH:
			if (!validate_flush_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid FLUSH statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_ANALYZE:
		case MYLITE_STATEMENT_CHECK:
		case MYLITE_STATEMENT_CHECKSUM:
		case MYLITE_STATEMENT_OPTIMIZE:
		case MYLITE_STATEMENT_REPAIR:
			if (!validate_maintenance_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid table maintenance statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_GRANT:
		case MYLITE_STATEMENT_REVOKE:
			if (!validate_principal_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid principal statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_SAVEPOINT:
			if (!validate_savepoint_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid SAVEPOINT statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_RELEASE:
			if (!validate_release_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid RELEASE statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_COMMIT:
			if (!validate_commit_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid COMMIT statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_ROLLBACK:
			if (!validate_rollback_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid ROLLBACK statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_START:
			if (!validate_start_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid START statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_STOP:
			if (!validate_stop_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid STOP statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_PREPARE:
			if (!validate_prepare_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid PREPARE statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_EXECUTE:
			if (!validate_execute_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid EXECUTE statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_DEALLOCATE:
			if (!validate_deallocate_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid DEALLOCATE statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_XA:
			if (!validate_xa_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid XA statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_DROP:
			if (!validate_drop_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid DROP statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_RESTART:
			if (!validate_single_token_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid RESTART statement");
				return;
			}
			break;
		case MYLITE_STATEMENT_SHUTDOWN:
			if (!validate_single_token_statement_syntax(parser, statement)) {
				mylite_parser_set_error(parser, "invalid SHUTDOWN statement");
				return;
			}
			break;
		default:
			break;
		}
	}
}

static int validate_create_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == ROLE_T) {
		return validate_create_role_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_starts_create_loadable_function_statement(parser, token_index, last_token_index)) {
		return validate_create_loadable_function_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_starts_create_routine_statement(parser, token_index, last_token_index)) {
		return validate_create_routine_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_starts_create_index_statement(parser, token_index, last_token_index)) {
		return validate_create_index_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_starts_view_statement(parser, token_index, last_token_index)) {
		return validate_create_view_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_starts_event_statement(parser, token_index, last_token_index)) {
		return validate_create_event_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_starts_trigger_statement(parser, token_index, last_token_index)) {
		return validate_create_trigger_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_starts_create_table_compact_statement(parser, token_index, last_token_index)) {
		return validate_create_table_compact_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_starts_create_table_definition_statement(parser, token_index, last_token_index)) {
		return validate_create_table_definition_statement_syntax(parser, token_index, last_token_index);
	}
	if (parser->tokens[token_index].parser_token == DATABASE_T ||
	    parser->tokens[token_index].parser_token == SCHEMA_T) {
		return validate_create_database_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_is_resource_group_sequence(parser, token_index, last_token_index)) {
		return validate_create_resource_group_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_is_logfile_group_sequence(parser, token_index, last_token_index)) {
		return validate_create_logfile_group_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_is_undo_tablespace_sequence(parser, token_index, last_token_index)) {
		return validate_create_tablespace_statement_syntax(parser, token_index, last_token_index, 1);
	}
	if (token_is_tablespace_token(parser, token_index)) {
		return validate_create_tablespace_statement_syntax(parser, token_index, last_token_index, 0);
	}
	if (token_is_create_spatial_reference_system_start(parser, token_index, last_token_index) ||
	    (token_text_equals(parser, token_index, "OR") &&
	     token_index + 1 <= last_token_index &&
	     token_text_equals(parser, token_index + 1, "SPATIAL"))) {
		return validate_create_spatial_reference_system_statement_syntax(parser,
		                                                                 token_index,
		                                                                 last_token_index);
	}
	if (token_text_equals(parser, token_index, "OR")) {
		return 0;
	}
	if (token_text_equals(parser, token_index, "SERVER")) {
		return validate_create_server_statement_syntax(parser, token_index, last_token_index);
	}
	if (parser->tokens[token_index].parser_token == USER_T) {
		return validate_create_user_statement_syntax(parser, token_index, last_token_index);
	}
	return 1;
}

static int validate_create_role_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index)
{
	if (token_index >= parser->token_count ||
	    parser->tokens[token_index].parser_token != ROLE_T) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "IF")) {
		if (token_index + 2 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "NOT") ||
		    !token_text_equals(parser, token_index + 2, "EXISTS")) {
			return 0;
		}
		token_index += 3;
	}

	return validate_principal_name_list_syntax(parser,
	                                           token_index,
	                                           last_token_index,
	                                           0);
}

static int validate_create_loadable_function_statement_syntax(const mylite_parser *parser,
                                                              size_t token_index,
                                                              size_t last_token_index)
{
	if (token_text_equals(parser, token_index, "AGGREGATE")) {
		token_index++;
	}
	if (token_index > last_token_index ||
	    parser->tokens[token_index].parser_token != FUNCTION_T) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "IF")) {
		if (token_index + 2 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "NOT") ||
		    !token_text_equals(parser, token_index + 2, "EXISTS")) {
			return 0;
		}
		token_index += 3;
	}

	if (token_index > last_token_index || !token_can_continue_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index = last_qualified_name_token(parser, token_index, last_token_index) + 1;

	if (token_index + 3 > last_token_index ||
	    !token_text_equals(parser, token_index, "RETURNS") ||
	    !token_is_loadable_function_return_type(parser, token_index + 1) ||
	    !token_text_equals(parser, token_index + 2, "SONAME") ||
	    parser->tokens[token_index + 3].kind != MYLITE_TOKEN_STRING) {
		return 0;
	}

	return token_index + 3 == last_token_index;
}

static int token_starts_create_loadable_function_statement(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}
	if (token_text_equals(parser, token_index, "AGGREGATE")) {
		return token_index + 1 <= last_token_index &&
		       parser->tokens[token_index + 1].parser_token == FUNCTION_T;
	}
	if (parser->tokens[token_index].parser_token != FUNCTION_T) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "IF")) {
		if (token_index + 2 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "NOT") ||
		    !token_text_equals(parser, token_index + 2, "EXISTS")) {
			while (token_index <= last_token_index) {
				if (token_text_equals(parser, token_index, "SONAME")) {
					return 1;
				}
				token_index++;
			}
			return 0;
		}
		token_index += 3;
	}

	if (token_index > last_token_index || !token_can_continue_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index = last_qualified_name_token(parser, token_index, last_token_index) + 1;
	if (token_index <= last_token_index && parser->tokens[token_index].parser_token == '(') {
		return 0;
	}

	return token_index <= last_token_index &&
	       (token_text_equals(parser, token_index, "RETURNS") ||
	        token_text_equals(parser, token_index, "SONAME"));
}

static int token_is_loadable_function_return_type(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "STRING") ||
	       token_text_equals(parser, token_index, "INTEGER") ||
	       token_text_equals(parser, token_index, "REAL") ||
	       token_text_equals(parser, token_index, "DECIMAL");
}

static int validate_create_routine_statement_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index)
{
	int is_function;

	if (token_text_equals(parser, token_index, "DEFINER")) {
		if (!validate_definer_clause_syntax(parser, token_index, last_token_index, &token_index)) {
			return 0;
		}
	}
	if (token_index > last_token_index ||
	    (parser->tokens[token_index].parser_token != FUNCTION_T &&
	     parser->tokens[token_index].parser_token != PROCEDURE_T)) {
		return 0;
	}
	is_function = parser->tokens[token_index].parser_token == FUNCTION_T;

	token_index++;
	if (token_index + 2 <= last_token_index &&
	    token_text_equals(parser, token_index, "IF") &&
	    token_text_equals(parser, token_index + 1, "NOT") &&
	    token_text_equals(parser, token_index + 2, "EXISTS")) {
		token_index += 3;
	}
	if (token_index + 1 > last_token_index || !token_can_continue_object_name(&parser->tokens[token_index])) {
		return 0;
	}

	token_index = last_qualified_name_token(parser, token_index, last_token_index) + 1;
	if (token_index > last_token_index ||
	    parser->tokens[token_index].parser_token != '(' ||
	    !validate_routine_parameter_list_syntax(parser, token_index, is_function)) {
		return 0;
	}
	token_index = parser->tokens[token_index].matching_token;

	if (is_function) {
		int saw_return_type = 0;

		if (token_index + 1 > last_token_index ||
		    !token_text_equals(parser, token_index, "RETURNS")) {
			return 0;
		}
		token_index++;
		while (token_index <= last_token_index &&
		       !token_starts_create_routine_characteristic(parser, token_index, last_token_index) &&
		       !token_can_start_stored_function_body(parser, token_index)) {
			size_t matching_token = parser->tokens[token_index].matching_token;

			saw_return_type = 1;
			if (matching_token > token_index + 1) {
				token_index = matching_token;
			} else {
				token_index++;
			}
		}
		if (!saw_return_type) {
			return 0;
		}
	}

	while (token_index <= last_token_index &&
	       token_starts_create_routine_characteristic(parser, token_index, last_token_index)) {
		if (!validate_create_routine_characteristic_syntax(parser,
		                                                   token_index,
		                                                   last_token_index,
		                                                   &token_index)) {
			return 0;
		}
	}

	return token_index <= last_token_index;
}

static int validate_routine_parameter_list_syntax(const mylite_parser *parser,
                                                  size_t open_token_index,
                                                  int is_function)
{
	size_t token_index = open_token_index + 1;
	size_t close_token_index;

	if (open_token_index >= parser->token_count ||
	    parser->tokens[open_token_index].parser_token != '(' ||
	    parser->tokens[open_token_index].matching_token <= open_token_index) {
		return 0;
	}

	close_token_index = parser->tokens[open_token_index].matching_token - 1;
	if (token_index >= close_token_index) {
		return 1;
	}

	while (token_index < close_token_index) {
		int saw_type_token = 0;

		if (token_is_routine_parameter_mode(parser, token_index)) {
			if (is_function) {
				return 0;
			}
			token_index++;
		}
		if (token_index >= close_token_index ||
		    !token_can_start_local_variable_name(&parser->tokens[token_index])) {
			return 0;
		}
		token_index++;

		while (token_index < close_token_index && parser->tokens[token_index].parser_token != ',') {
			size_t matching_token = parser->tokens[token_index].matching_token;

			saw_type_token = 1;
			if (matching_token > token_index + 1) {
				token_index = matching_token;
			} else {
				token_index++;
			}
		}
		if (!saw_type_token) {
			return 0;
		}
		if (token_index >= close_token_index) {
			return 1;
		}
		token_index++;
		if (token_index >= close_token_index) {
			return 0;
		}
	}

	return 1;
}

static int validate_create_routine_characteristic_syntax(const mylite_parser *parser,
                                                         size_t token_index,
                                                         size_t last_token_index,
                                                         size_t *next_token_index)
{
	if (token_text_equals(parser, token_index, "COMMENT")) {
		if (token_index + 1 > last_token_index ||
		    parser->tokens[token_index + 1].kind != MYLITE_TOKEN_STRING) {
			return 0;
		}
		*next_token_index = token_index + 2;
		return 1;
	}
	if (token_text_equals(parser, token_index, "LANGUAGE")) {
		if (token_index + 1 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "SQL")) {
			return 0;
		}
		*next_token_index = token_index + 2;
		return 1;
	}
	if (token_text_equals(parser, token_index, "DETERMINISTIC")) {
		*next_token_index = token_index + 1;
		return 1;
	}
	if (token_text_equals(parser, token_index, "NOT")) {
		if (token_index + 1 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "DETERMINISTIC")) {
			return 0;
		}
		*next_token_index = token_index + 2;
		return 1;
	}
	if (token_text_equals(parser, token_index, "CONTAINS") ||
	    parser->tokens[token_index].parser_token == NO_T) {
		if (token_index + 1 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "SQL")) {
			return 0;
		}
		*next_token_index = token_index + 2;
		return 1;
	}
	if (token_text_equals(parser, token_index, "READS") ||
	    token_text_equals(parser, token_index, "MODIFIES")) {
		if (token_index + 2 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "SQL") ||
		    !token_text_equals(parser, token_index + 2, "DATA")) {
			return 0;
		}
		*next_token_index = token_index + 3;
		return 1;
	}
	if (token_text_equals(parser, token_index, "SQL")) {
		if (token_index + 2 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "SECURITY") ||
		    (!token_text_equals(parser, token_index + 2, "DEFINER") &&
		     !token_text_equals(parser, token_index + 2, "INVOKER"))) {
			return 0;
		}
		*next_token_index = token_index + 3;
		return 1;
	}

	return 0;
}

static int token_starts_create_routine_statement(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}
	if (parser->tokens[token_index].parser_token == PROCEDURE_T) {
		return 1;
	}
	if (parser->tokens[token_index].parser_token == FUNCTION_T) {
		return 1;
	}
	if (!token_text_equals(parser, token_index, "DEFINER")) {
		return 0;
	}

	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;
		size_t definer_clause_last_token = last_definer_clause_token(parser, token_index, last_token_index);

		if (definer_clause_last_token > token_index) {
			token_index = definer_clause_last_token + 1;
			continue;
		}
		if (parser->tokens[token_index].parser_token == FUNCTION_T ||
		    parser->tokens[token_index].parser_token == PROCEDURE_T) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token == DATABASE_T ||
		    parser->tokens[token_index].parser_token == EVENT_T ||
		    parser->tokens[token_index].parser_token == INDEX_T ||
		    parser->tokens[token_index].parser_token == ROLE_T ||
		    parser->tokens[token_index].parser_token == SCHEMA_T ||
		    parser->tokens[token_index].parser_token == TABLE_T ||
		    parser->tokens[token_index].parser_token == TRIGGER_T ||
		    parser->tokens[token_index].parser_token == VIEW_T ||
		    token_text_equals(parser, token_index, "SERVER") ||
		    token_text_equals(parser, token_index, "TABLESPACE")) {
			return 0;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}
	return 0;
}

static int token_is_routine_parameter_mode(const mylite_parser *parser, size_t token_index)
{
	return parser->tokens[token_index].parser_token == IN_T ||
	       token_text_equals(parser, token_index, "OUT") ||
	       token_text_equals(parser, token_index, "INOUT");
}

static int token_starts_create_routine_characteristic(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}
	if (token_text_equals(parser, token_index, "NOT")) {
		return token_index + 1 <= last_token_index &&
		       token_text_equals(parser, token_index + 1, "DETERMINISTIC");
	}
	return token_text_equals(parser, token_index, "COMMENT") ||
	       token_text_equals(parser, token_index, "LANGUAGE") ||
	       token_text_equals(parser, token_index, "DETERMINISTIC") ||
	       token_text_equals(parser, token_index, "CONTAINS") ||
	       parser->tokens[token_index].parser_token == NO_T ||
	       token_text_equals(parser, token_index, "READS") ||
	       token_text_equals(parser, token_index, "MODIFIES") ||
	       token_text_equals(parser, token_index, "SQL");
}

static int token_can_start_stored_function_body(const mylite_parser *parser, size_t token_index)
{
	return parser->tokens[token_index].parser_token == BEGIN_T ||
	       parser->tokens[token_index].parser_token == RETURN_T ||
	       parser->tokens[token_index].parser_token == SELECT_T ||
	       parser->tokens[token_index].parser_token == INSERT_T ||
	       parser->tokens[token_index].parser_token == UPDATE_T ||
	       parser->tokens[token_index].parser_token == DELETE_T ||
	       parser->tokens[token_index].parser_token == WITH_T ||
	       parser->tokens[token_index].parser_token == DO_T ||
	       parser->tokens[token_index].parser_token == CALL_T ||
	       parser->tokens[token_index].parser_token == CASE_T ||
	       parser->tokens[token_index].parser_token == IF_T ||
	       parser->tokens[token_index].parser_token == LOOP_T ||
	       parser->tokens[token_index].parser_token == REPEAT_T ||
	       parser->tokens[token_index].parser_token == WHILE_T;
}

static int validate_create_index_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	size_t last_name_token;

	if (token_is_create_index_modifier(parser, token_index)) {
		token_index++;
	}
	if (token_index > last_token_index ||
	    parser->tokens[token_index].parser_token != INDEX_T ||
	    token_index + 1 > last_token_index ||
	    !token_can_continue_object_name(&parser->tokens[token_index + 1])) {
		return 0;
	}

	token_index += 2;
	if (token_index <= last_token_index &&
	    (token_text_equals(parser, token_index, "USING") ||
	     token_text_equals(parser, token_index, "TYPE"))) {
		if (token_index + 1 > last_token_index || !token_is_index_type_value(parser, token_index + 1)) {
			return 0;
		}
		token_index += 2;
	}

	if (token_index > last_token_index ||
	    !token_text_equals(parser, token_index, "ON") ||
	    token_index + 1 > last_token_index ||
	    !token_can_continue_object_name(&parser->tokens[token_index + 1])) {
		return 0;
	}

	token_index += 1;
	last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
	token_index = last_name_token + 1;
	if (token_index > last_token_index ||
	    parser->tokens[token_index].parser_token != '(' ||
	    !validate_create_index_key_part_list_syntax(parser, token_index)) {
		return 0;
	}

	token_index = parser->tokens[token_index].matching_token;
	while (token_index <= last_token_index) {
		if (!validate_create_index_option_syntax(parser, token_index, last_token_index, &token_index)) {
			return 0;
		}
	}
	return 1;
}

static int validate_create_index_key_part_list_syntax(const mylite_parser *parser, size_t open_token_index)
{
	size_t token_index = open_token_index + 1;
	size_t close_token_index;

	if (open_token_index >= parser->token_count ||
	    parser->tokens[open_token_index].parser_token != '(' ||
	    parser->tokens[open_token_index].matching_token <= open_token_index + 1) {
		return 0;
	}

	close_token_index = parser->tokens[open_token_index].matching_token - 1;
	if (token_index >= close_token_index) {
		return 0;
	}

	while (token_index < close_token_index) {
		int saw_key_part_token = 0;

		while (token_index < close_token_index && parser->tokens[token_index].parser_token != ',') {
			size_t matching_token = parser->tokens[token_index].matching_token;

			saw_key_part_token = 1;
			if (matching_token > token_index + 1) {
				token_index = matching_token;
			} else {
				token_index++;
			}
		}
		if (!saw_key_part_token) {
			return 0;
		}
		if (token_index >= close_token_index) {
			return 1;
		}
		token_index++;
		if (token_index >= close_token_index) {
			return 0;
		}
	}

	return 1;
}

static int validate_create_index_option_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index,
                                               size_t *next_token_index)
{
	if (token_text_equals(parser, token_index, "USING") ||
	    token_text_equals(parser, token_index, "TYPE")) {
		if (token_index + 1 > last_token_index || !token_is_index_type_value(parser, token_index + 1)) {
			return 0;
		}
		*next_token_index = token_index + 2;
		return 1;
	}
	if (token_text_equals(parser, token_index, "KEY_BLOCK_SIZE")) {
		token_index++;
		if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
			token_index++;
		}
		if (token_index > last_token_index || parser->tokens[token_index].kind != MYLITE_TOKEN_NUMBER) {
			return 0;
		}
		*next_token_index = token_index + 1;
		return 1;
	}
	if (token_text_equals(parser, token_index, "WITH")) {
		if (token_index + 2 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "PARSER") ||
		    !token_can_continue_object_name(&parser->tokens[token_index + 2])) {
			return 0;
		}
		*next_token_index = token_index + 3;
		return 1;
	}
	if (token_text_equals(parser, token_index, "COMMENT")) {
		if (token_index + 1 > last_token_index ||
		    parser->tokens[token_index + 1].kind != MYLITE_TOKEN_STRING) {
			return 0;
		}
		*next_token_index = token_index + 2;
		return 1;
	}
	if (token_text_equals(parser, token_index, "VISIBLE") ||
	    token_text_equals(parser, token_index, "INVISIBLE")) {
		*next_token_index = token_index + 1;
		return 1;
	}
	if (token_text_equals(parser, token_index, "ENGINE_ATTRIBUTE") ||
	    token_text_equals(parser, token_index, "SECONDARY_ENGINE_ATTRIBUTE")) {
		token_index++;
		if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
			token_index++;
		}
		if (token_index > last_token_index || parser->tokens[token_index].kind != MYLITE_TOKEN_STRING) {
			return 0;
		}
		*next_token_index = token_index + 1;
		return 1;
	}
	if (token_text_equals(parser, token_index, "ALGORITHM")) {
		token_index++;
		if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
			token_index++;
		}
		if (token_index > last_token_index || !token_is_drop_index_algorithm_value(parser, token_index)) {
			return 0;
		}
		*next_token_index = token_index + 1;
		return 1;
	}
	if (token_text_equals(parser, token_index, "LOCK")) {
		token_index++;
		if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
			token_index++;
		}
		if (token_index > last_token_index || !token_is_drop_index_lock_value(parser, token_index)) {
			return 0;
		}
		*next_token_index = token_index + 1;
		return 1;
	}

	return 0;
}

static int token_starts_create_index_statement(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}
	if (token_is_create_index_modifier(parser, token_index)) {
		token_index++;
	}
	return token_index <= last_token_index &&
	       parser->tokens[token_index].parser_token == INDEX_T;
}

static int token_is_create_index_modifier(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (parser->tokens[token_index].parser_token == UNIQUE_T ||
	        parser->tokens[token_index].parser_token == SPATIAL_T ||
	        token_text_equals(parser, token_index, "FULLTEXT"));
}

static int token_is_index_type_value(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "BTREE") ||
	       token_text_equals(parser, token_index, "HASH") ||
	       token_text_equals(parser, token_index, "RTREE");
}

static int validate_create_view_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index)
{
	if (token_text_equals(parser, token_index, "OR")) {
		if (token_index + 1 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "REPLACE")) {
			return 0;
		}
		token_index += 2;
	}

	return validate_view_statement_syntax(parser, token_index, last_token_index);
}

static int token_starts_view_statement(const mylite_parser *parser,
                                       size_t token_index,
                                       size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}
	if (token_text_equals(parser, token_index, "OR")) {
		if (token_index + 1 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "REPLACE")) {
			return 0;
		}
		token_index += 2;
	}
	if (token_index > last_token_index ||
	    (parser->tokens[token_index].parser_token != VIEW_T &&
	     !token_text_equals(parser, token_index, "ALGORITHM") &&
	     !token_text_equals(parser, token_index, "DEFINER") &&
	     !token_text_equals(parser, token_index, "SQL"))) {
		return 0;
	}

	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (parser->tokens[token_index].parser_token == VIEW_T) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token == DATABASE_T ||
		    parser->tokens[token_index].parser_token == EVENT_T ||
		    parser->tokens[token_index].parser_token == FUNCTION_T ||
		    parser->tokens[token_index].parser_token == INDEX_T ||
		    parser->tokens[token_index].parser_token == PROCEDURE_T ||
		    parser->tokens[token_index].parser_token == ROLE_T ||
		    parser->tokens[token_index].parser_token == SCHEMA_T ||
		    parser->tokens[token_index].parser_token == TABLE_T ||
		    parser->tokens[token_index].parser_token == TRIGGER_T ||
		    token_text_equals(parser, token_index, "SERVER") ||
		    token_text_equals(parser, token_index, "TABLESPACE")) {
			return 0;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}
	return 0;
}

static int validate_view_statement_syntax(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index)
{
	int saw_algorithm = 0;
	int saw_definer = 0;
	int saw_sql_security = 0;
	size_t query_start_token;
	size_t check_option_token;
	size_t query_last_token;

	while (token_index <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == VIEW_T) {
			break;
		}
		if (token_text_equals(parser, token_index, "ALGORITHM")) {
			if (saw_algorithm ||
			    !validate_view_algorithm_clause_syntax(parser, token_index, last_token_index, &token_index)) {
				return 0;
			}
			saw_algorithm = 1;
			continue;
		}
		if (token_text_equals(parser, token_index, "DEFINER")) {
			if (saw_definer ||
			    !validate_definer_clause_syntax(parser, token_index, last_token_index, &token_index)) {
				return 0;
			}
			saw_definer = 1;
			continue;
		}
		if (token_text_equals(parser, token_index, "SQL")) {
			if (saw_sql_security ||
			    !validate_view_sql_security_clause_syntax(parser, token_index, last_token_index, &token_index)) {
				return 0;
			}
			saw_sql_security = 1;
			continue;
		}
		return 0;
	}

	if (token_index > last_token_index ||
	    parser->tokens[token_index].parser_token != VIEW_T ||
	    token_index + 1 > last_token_index ||
	    !token_can_continue_object_name(&parser->tokens[token_index + 1])) {
		return 0;
	}

	token_index = last_qualified_name_token(parser, token_index + 1, last_token_index) + 1;
	if (token_index <= last_token_index && parser->tokens[token_index].parser_token == '(') {
		if (!validate_name_list_group_syntax(parser, token_index, 0, 0)) {
			return 0;
		}
		token_index = parser->tokens[token_index].matching_token;
	}

	if (token_index > last_token_index || parser->tokens[token_index].parser_token != AS_T) {
		return 0;
	}

	query_start_token = token_index + 1;
	if (query_start_token > last_token_index || !token_can_start_view_query(parser, query_start_token)) {
		return 0;
	}

	check_option_token = find_view_check_option_clause_token(parser, query_start_token, last_token_index);
	if (check_option_token < parser->token_count) {
		query_last_token = check_option_token - 1;
	} else {
		if (last_token_index >= query_start_token + 2 &&
		    token_text_equals(parser, last_token_index, "OPTION") &&
		    token_text_equals(parser, last_token_index - 1, "CHECK")) {
			return 0;
		}
		query_last_token = last_token_index;
	}

	return query_start_token <= query_last_token;
}

static int validate_view_algorithm_clause_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 size_t *next_token_index)
{
	if (!token_text_equals(parser, token_index, "ALGORITHM")) {
		return 0;
	}
	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
		token_index++;
	}
	if (token_index > last_token_index || !token_is_view_algorithm_value(parser, token_index)) {
		return 0;
	}

	*next_token_index = token_index + 1;
	return 1;
}

static int validate_definer_clause_syntax(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index,
                                          size_t *next_token_index)
{
	size_t first_account_token;
	size_t last_account_token;

	if (!token_text_equals(parser, token_index, "DEFINER") ||
	    token_index + 2 > last_token_index ||
	    !token_text_equals(parser, token_index + 1, "=")) {
		return 0;
	}

	first_account_token = token_index + 2;
	if (!token_can_start_definer_account_name(&parser->tokens[first_account_token])) {
		return 0;
	}

	last_account_token = last_account_name_token(parser, first_account_token, last_token_index);
	if (last_account_token + 1 <= last_token_index &&
	    token_is_account_at_marker(parser, last_account_token + 1)) {
		last_account_token++;
	}

	*next_token_index = last_account_token + 1;
	return 1;
}

static int validate_view_sql_security_clause_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    size_t *next_token_index)
{
	if (token_index + 2 > last_token_index ||
	    !token_text_equals(parser, token_index, "SQL") ||
	    !token_text_equals(parser, token_index + 1, "SECURITY") ||
	    (!token_text_equals(parser, token_index + 2, "DEFINER") &&
	     !token_text_equals(parser, token_index + 2, "INVOKER"))) {
		return 0;
	}

	*next_token_index = token_index + 3;
	return 1;
}

static size_t find_view_check_option_clause_token(const mylite_parser *parser,
                                                  size_t first_query_token,
                                                  size_t last_token_index)
{
	size_t check_token_index;
	size_t with_token_index;

	if (last_token_index < first_query_token + 2 ||
	    !token_text_equals(parser, last_token_index, "OPTION")) {
		return parser->token_count;
	}

	check_token_index = last_token_index - 1;
	if (!token_text_equals(parser, check_token_index, "CHECK")) {
		return parser->token_count;
	}

	with_token_index = check_token_index - 1;
	if (token_text_equals(parser, with_token_index, "CASCADED") ||
	    token_text_equals(parser, with_token_index, "LOCAL")) {
		if (with_token_index == 0) {
			return parser->token_count;
		}
		with_token_index--;
	}
	if (with_token_index < first_query_token ||
	    !token_text_equals(parser, with_token_index, "WITH")) {
		return parser->token_count;
	}

	return with_token_index;
}

static int token_is_view_algorithm_value(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "UNDEFINED") ||
	       token_text_equals(parser, token_index, "MERGE") ||
	       token_text_equals(parser, token_index, "TEMPTABLE");
}

static int token_can_start_view_query(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (parser->tokens[token_index].parser_token == SELECT_T ||
	        parser->tokens[token_index].parser_token == WITH_T ||
	        parser->tokens[token_index].parser_token == TABLE_T ||
	        parser->tokens[token_index].parser_token == VALUES_T ||
	        parser->tokens[token_index].parser_token == '(');
}

static int validate_create_event_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	return validate_event_statement_syntax(parser, token_index, last_token_index, 1);
}

static int validate_alter_event_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index)
{
	return validate_event_statement_syntax(parser, token_index, last_token_index, 0);
}

static int token_starts_event_statement(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}
	if (parser->tokens[token_index].parser_token == EVENT_T) {
		return 1;
	}
	if (!token_text_equals(parser, token_index, "DEFINER")) {
		return 0;
	}

	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;
		size_t definer_clause_last_token = last_definer_clause_token(parser, token_index, last_token_index);

		if (definer_clause_last_token > token_index) {
			token_index = definer_clause_last_token + 1;
			continue;
		}
		if (parser->tokens[token_index].parser_token == EVENT_T) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token == DATABASE_T ||
		    parser->tokens[token_index].parser_token == FUNCTION_T ||
		    parser->tokens[token_index].parser_token == INDEX_T ||
		    parser->tokens[token_index].parser_token == PROCEDURE_T ||
		    parser->tokens[token_index].parser_token == ROLE_T ||
		    parser->tokens[token_index].parser_token == SCHEMA_T ||
		    parser->tokens[token_index].parser_token == TABLE_T ||
		    parser->tokens[token_index].parser_token == TRIGGER_T ||
		    parser->tokens[token_index].parser_token == VIEW_T ||
		    token_text_equals(parser, token_index, "SERVER") ||
		    token_text_equals(parser, token_index, "TABLESPACE")) {
			return 0;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}
	return 0;
}

static int validate_event_statement_syntax(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index,
                                           int is_create)
{
	int saw_completion = 0;
	int saw_do = 0;
	int saw_rename = 0;
	int saw_schedule = 0;
	int saw_status = 0;
	int saw_comment = 0;
	int saw_alter_option = 0;

	if (token_text_equals(parser, token_index, "DEFINER")) {
		if (!validate_definer_clause_syntax(parser, token_index, last_token_index, &token_index)) {
			return 0;
		}
		if (!is_create) {
			saw_alter_option = 1;
		}
	}
	if (token_index > last_token_index ||
	    parser->tokens[token_index].parser_token != EVENT_T) {
		return 0;
	}

	token_index++;
	if (is_create &&
	    token_index + 2 <= last_token_index &&
	    token_text_equals(parser, token_index, "IF") &&
	    token_text_equals(parser, token_index + 1, "NOT") &&
	    token_text_equals(parser, token_index + 2, "EXISTS")) {
		token_index += 3;
	}
	if (token_index > last_token_index || !token_can_continue_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index = last_qualified_name_token(parser, token_index, last_token_index) + 1;

	while (token_index <= last_token_index) {
		if (token_text_equals(parser, token_index, "DEFINER") || saw_do) {
			return 0;
		}
		if (token_text_equals(parser, token_index, "ON") &&
		    token_index + 1 <= last_token_index &&
		    token_text_equals(parser, token_index + 1, "SCHEDULE")) {
			if (saw_schedule ||
			    !validate_event_schedule_clause_syntax(parser, token_index, last_token_index, &token_index)) {
				return 0;
			}
			saw_schedule = 1;
			saw_alter_option = 1;
			continue;
		}
		if (token_text_equals(parser, token_index, "ON")) {
			if (saw_completion ||
			    !validate_event_completion_clause_syntax(parser, token_index, last_token_index, &token_index)) {
				return 0;
			}
			saw_completion = 1;
			saw_alter_option = 1;
			continue;
		}
		if (!is_create && token_text_equals(parser, token_index, "RENAME")) {
			if (saw_rename ||
			    !validate_event_rename_clause_syntax(parser, token_index, last_token_index, &token_index)) {
				return 0;
			}
			saw_rename = 1;
			saw_alter_option = 1;
			continue;
		}
		if (token_text_equals(parser, token_index, "ENABLE") ||
		    token_text_equals(parser, token_index, "DISABLE")) {
			if (saw_status ||
			    !validate_event_status_clause_syntax(parser, token_index, last_token_index, &token_index)) {
				return 0;
			}
			saw_status = 1;
			saw_alter_option = 1;
			continue;
		}
		if (token_text_equals(parser, token_index, "COMMENT")) {
			if (saw_comment ||
			    !validate_event_comment_clause_syntax(parser, token_index, last_token_index, &token_index)) {
				return 0;
			}
			saw_comment = 1;
			saw_alter_option = 1;
			continue;
		}
		if (token_text_equals(parser, token_index, "DO")) {
			if (saw_do || token_index + 1 > last_token_index) {
				return 0;
			}
			saw_do = 1;
			saw_alter_option = 1;
			token_index = last_token_index + 1;
			continue;
		}
		return 0;
	}

	if (is_create) {
		return saw_schedule && saw_do;
	}
	return saw_alter_option;
}

static int validate_event_schedule_clause_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 size_t *next_token_index)
{
	if (token_index + 2 > last_token_index ||
	    !token_text_equals(parser, token_index, "ON") ||
	    !token_text_equals(parser, token_index + 1, "SCHEDULE")) {
		return 0;
	}

	token_index += 2;
	if (token_text_equals(parser, token_index, "AT")) {
		size_t boundary_token;

		token_index++;
		boundary_token = find_event_clause_boundary_token(parser, token_index, last_token_index);
		if (boundary_token == token_index) {
			return 0;
		}
		*next_token_index = boundary_token;
		return 1;
	}

	if (token_text_equals(parser, token_index, "EVERY")) {
		int saw_ends = 0;
		int saw_starts = 0;

		token_index++;
		if (!validate_event_interval_syntax(parser, token_index, last_token_index, &token_index)) {
			return 0;
		}

		while (token_index <= last_token_index &&
		       !token_starts_event_clause_boundary(parser, token_index, last_token_index)) {
			size_t expression_start_token;
			size_t expression_end_token;

			if (token_text_equals(parser, token_index, "STARTS")) {
				if (saw_starts || saw_ends) {
					return 0;
				}
				saw_starts = 1;
			} else if (token_text_equals(parser, token_index, "ENDS")) {
				if (saw_ends) {
					return 0;
				}
				saw_ends = 1;
			} else {
				return 0;
			}

			expression_start_token = token_index + 1;
			expression_end_token = find_event_schedule_subclause_boundary_token(parser,
			                                                                    expression_start_token,
			                                                                    last_token_index);
			if (expression_end_token == expression_start_token) {
				return 0;
			}
			token_index = expression_end_token;
		}

		*next_token_index = token_index;
		return 1;
	}

	return 0;
}

static int validate_event_interval_syntax(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index,
                                          size_t *next_token_index)
{
	size_t quantity_token = token_index;

	while (token_index <= last_token_index &&
	       !token_starts_event_clause_boundary(parser, token_index, last_token_index) &&
	       !token_starts_event_schedule_subclause_boundary(parser, token_index, last_token_index) &&
	       !token_is_event_interval_unit(parser, token_index)) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}

	if (token_index <= quantity_token ||
	    token_index > last_token_index ||
	    !token_is_event_interval_unit(parser, token_index)) {
		return 0;
	}

	*next_token_index = token_index + 1;
	return 1;
}

static int validate_event_completion_clause_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index,
                                                   size_t *next_token_index)
{
	if (token_index + 2 > last_token_index ||
	    !token_text_equals(parser, token_index, "ON") ||
	    !token_text_equals(parser, token_index + 1, "COMPLETION")) {
		return 0;
	}

	token_index += 2;
	if (token_text_equals(parser, token_index, "NOT")) {
		token_index++;
	}
	if (token_index > last_token_index || !token_text_equals(parser, token_index, "PRESERVE")) {
		return 0;
	}

	*next_token_index = token_index + 1;
	return 1;
}

static int validate_event_status_clause_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index,
                                               size_t *next_token_index)
{
	if (token_text_equals(parser, token_index, "ENABLE")) {
		*next_token_index = token_index + 1;
		return 1;
	}
	if (!token_text_equals(parser, token_index, "DISABLE")) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "ON")) {
		if (token_index + 1 > last_token_index ||
		    (!token_text_equals(parser, token_index + 1, "REPLICA") &&
		     !token_text_equals(parser, token_index + 1, "SLAVE"))) {
			return 0;
		}
		token_index += 2;
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_event_comment_clause_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index,
                                                size_t *next_token_index)
{
	if (token_index + 1 > last_token_index ||
	    !token_text_equals(parser, token_index, "COMMENT") ||
	    parser->tokens[token_index + 1].kind != MYLITE_TOKEN_STRING) {
		return 0;
	}

	*next_token_index = token_index + 2;
	return 1;
}

static int validate_event_rename_clause_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index,
                                               size_t *next_token_index)
{
	if (token_index + 2 > last_token_index ||
	    !token_text_equals(parser, token_index, "RENAME") ||
	    !token_text_equals(parser, token_index + 1, "TO") ||
	    !token_can_continue_object_name(&parser->tokens[token_index + 2])) {
		return 0;
	}

	*next_token_index = last_qualified_name_token(parser, token_index + 2, last_token_index) + 1;
	return 1;
}

static size_t find_event_clause_boundary_token(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (token_starts_event_clause_boundary(parser, token_index, last_token_index)) {
			return token_index;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}
	return token_index;
}

static size_t find_event_schedule_subclause_boundary_token(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (token_starts_event_schedule_subclause_boundary(parser, token_index, last_token_index)) {
			return token_index;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}
	return token_index;
}

static int token_starts_event_clause_boundary(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}
	if (token_text_equals(parser, token_index, "ON")) {
		return token_index + 1 <= last_token_index &&
		       (token_text_equals(parser, token_index + 1, "COMPLETION") ||
		        token_text_equals(parser, token_index + 1, "SCHEDULE"));
	}
	return token_text_equals(parser, token_index, "RENAME") ||
	       token_text_equals(parser, token_index, "ENABLE") ||
	       token_text_equals(parser, token_index, "DISABLE") ||
	       token_text_equals(parser, token_index, "COMMENT") ||
	       token_text_equals(parser, token_index, "DO");
}

static int token_starts_event_schedule_subclause_boundary(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index)
{
	return token_starts_event_clause_boundary(parser, token_index, last_token_index) ||
	       token_text_equals(parser, token_index, "STARTS") ||
	       token_text_equals(parser, token_index, "ENDS");
}

static int token_is_event_interval_unit(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "YEAR") ||
	       token_text_equals(parser, token_index, "QUARTER") ||
	       token_text_equals(parser, token_index, "MONTH") ||
	       token_text_equals(parser, token_index, "DAY") ||
	       token_text_equals(parser, token_index, "HOUR") ||
	       token_text_equals(parser, token_index, "MINUTE") ||
	       token_text_equals(parser, token_index, "WEEK") ||
	       token_text_equals(parser, token_index, "SECOND") ||
	       token_text_equals(parser, token_index, "YEAR_MONTH") ||
	       token_text_equals(parser, token_index, "DAY_HOUR") ||
	       token_text_equals(parser, token_index, "DAY_MINUTE") ||
	       token_text_equals(parser, token_index, "DAY_SECOND") ||
	       token_text_equals(parser, token_index, "HOUR_MINUTE") ||
	       token_text_equals(parser, token_index, "HOUR_SECOND") ||
	       token_text_equals(parser, token_index, "MINUTE_SECOND");
}

static int validate_create_trigger_statement_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index)
{
	if (token_text_equals(parser, token_index, "DEFINER")) {
		if (!validate_definer_clause_syntax(parser, token_index, last_token_index, &token_index)) {
			return 0;
		}
	}
	if (token_index > last_token_index ||
	    parser->tokens[token_index].parser_token != TRIGGER_T) {
		return 0;
	}

	token_index++;
	if (token_index + 2 <= last_token_index &&
	    token_text_equals(parser, token_index, "IF") &&
	    token_text_equals(parser, token_index + 1, "NOT") &&
	    token_text_equals(parser, token_index + 2, "EXISTS")) {
		token_index += 3;
	}
	if (token_index > last_token_index || !token_can_continue_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index = last_qualified_name_token(parser, token_index, last_token_index) + 1;

	if (token_index + 5 > last_token_index ||
	    !token_is_trigger_time(parser, token_index) ||
	    !token_is_trigger_event(parser, token_index + 1) ||
	    !token_text_equals(parser, token_index + 2, "ON") ||
	    !token_can_continue_object_name(&parser->tokens[token_index + 3])) {
		return 0;
	}

	token_index = last_qualified_name_token(parser, token_index + 3, last_token_index) + 1;
	if (token_index + 2 > last_token_index ||
	    !token_text_equals(parser, token_index, "FOR") ||
	    !token_text_equals(parser, token_index + 1, "EACH") ||
	    !token_text_equals(parser, token_index + 2, "ROW")) {
		return 0;
	}
	token_index += 3;

	if (token_index <= last_token_index &&
	    (token_text_equals(parser, token_index, "FOLLOWS") ||
	     token_text_equals(parser, token_index, "PRECEDES"))) {
		if (!validate_trigger_order_clause_syntax(parser, token_index, last_token_index, &token_index)) {
			return 0;
		}
	}

	return token_index <= last_token_index;
}

static int token_starts_trigger_statement(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}
	if (parser->tokens[token_index].parser_token == TRIGGER_T) {
		return 1;
	}
	if (!token_text_equals(parser, token_index, "DEFINER")) {
		return 0;
	}

	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;
		size_t definer_clause_last_token = last_definer_clause_token(parser, token_index, last_token_index);

		if (definer_clause_last_token > token_index) {
			token_index = definer_clause_last_token + 1;
			continue;
		}
		if (parser->tokens[token_index].parser_token == TRIGGER_T) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token == DATABASE_T ||
		    parser->tokens[token_index].parser_token == EVENT_T ||
		    parser->tokens[token_index].parser_token == FUNCTION_T ||
		    parser->tokens[token_index].parser_token == INDEX_T ||
		    parser->tokens[token_index].parser_token == PROCEDURE_T ||
		    parser->tokens[token_index].parser_token == ROLE_T ||
		    parser->tokens[token_index].parser_token == SCHEMA_T ||
		    parser->tokens[token_index].parser_token == TABLE_T ||
		    parser->tokens[token_index].parser_token == VIEW_T ||
		    token_text_equals(parser, token_index, "SERVER") ||
		    token_text_equals(parser, token_index, "TABLESPACE")) {
			return 0;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}
	return 0;
}

static int validate_trigger_order_clause_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index,
                                                size_t *next_token_index)
{
	if ((!token_text_equals(parser, token_index, "FOLLOWS") &&
	     !token_text_equals(parser, token_index, "PRECEDES")) ||
	    token_index + 1 > last_token_index ||
	    !token_can_continue_object_name(&parser->tokens[token_index + 1])) {
		return 0;
	}

	*next_token_index = last_qualified_name_token(parser, token_index + 1, last_token_index) + 1;
	return 1;
}

static int token_is_trigger_time(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "BEFORE") ||
	       token_text_equals(parser, token_index, "AFTER");
}

static int token_is_trigger_event(const mylite_parser *parser, size_t token_index)
{
	return parser->tokens[token_index].parser_token == INSERT_T ||
	       parser->tokens[token_index].parser_token == UPDATE_T ||
	       parser->tokens[token_index].parser_token == DELETE_T;
}

static int validate_create_table_compact_statement_syntax(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index)
{
	token_index = first_token_after_create_table_head(parser, token_index, last_token_index);
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "LIKE") ||
	    (parser->tokens[token_index].parser_token == '(' &&
	     token_index + 1 <= last_token_index &&
	     token_text_equals(parser, token_index + 1, "LIKE"))) {
		return validate_create_table_like_tail_syntax(parser, token_index, last_token_index);
	}

	return validate_create_table_select_tail_syntax(parser, token_index, last_token_index);
}

static int token_starts_create_table_compact_statement(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index)
{
	token_index = first_token_after_create_table_head(parser, token_index, last_token_index);
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}
	if (token_text_equals(parser, token_index, "LIKE") ||
	    (parser->tokens[token_index].parser_token == '(' &&
	     token_index + 1 <= last_token_index &&
	     token_text_equals(parser, token_index + 1, "LIKE"))) {
		return 1;
	}

	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (token_can_start_create_table_query(parser, token_index) ||
		    token_text_equals(parser, token_index, "AS") ||
		    token_text_equals(parser, token_index, "IGNORE") ||
		    token_text_equals(parser, token_index, "REPLACE")) {
			return 1;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}
	return 0;
}

static int validate_create_table_definition_statement_syntax(const mylite_parser *parser,
                                                             size_t token_index,
                                                             size_t last_token_index)
{
	token_index = first_token_after_create_table_head(parser, token_index, last_token_index);
	if (token_index > last_token_index ||
	    parser->tokens[token_index].parser_token != '(' ||
	    token_can_start_create_table_query(parser, token_index) ||
	    (token_index + 1 <= last_token_index && token_text_equals(parser, token_index + 1, "LIKE")) ||
	    !validate_create_table_definition_group_syntax(parser, token_index)) {
		return 0;
	}

	return 1;
}

static int token_starts_create_table_definition_statement(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index)
{
	if (token_index <= last_token_index &&
	    parser->tokens[token_index].parser_token == TEMPORARY_T) {
		token_index++;
	}
	return token_index <= last_token_index &&
	       token_index < parser->token_count &&
	       parser->tokens[token_index].parser_token == TABLE_T;
}

static size_t first_token_after_create_table_head(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	if (token_index <= last_token_index &&
	    parser->tokens[token_index].parser_token == TEMPORARY_T) {
		token_index++;
	}
	if (token_index > last_token_index ||
	    parser->tokens[token_index].parser_token != TABLE_T) {
		return parser->token_count;
	}

	token_index++;
	if (token_index + 2 <= last_token_index &&
	    token_text_equals(parser, token_index, "IF") &&
	    token_text_equals(parser, token_index + 1, "NOT") &&
	    token_text_equals(parser, token_index + 2, "EXISTS")) {
		token_index += 3;
	}
	if (token_index > last_token_index || !token_can_start_object_name(&parser->tokens[token_index])) {
		return parser->token_count;
	}
	return last_qualified_name_token(parser, token_index, last_token_index) + 1;
}

static int validate_create_table_like_tail_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	size_t close_token_index;

	if (token_text_equals(parser, token_index, "LIKE")) {
		token_index++;
		if (token_index > last_token_index || !token_can_start_object_name(&parser->tokens[token_index])) {
			return 0;
		}
		return last_qualified_name_token(parser, token_index, last_token_index) == last_token_index;
	}

	if (parser->tokens[token_index].parser_token != '(' ||
	    parser->tokens[token_index].matching_token <= token_index + 1) {
		return 0;
	}
	close_token_index = parser->tokens[token_index].matching_token - 1;
	if (close_token_index != last_token_index ||
	    token_index + 2 > close_token_index ||
	    !token_text_equals(parser, token_index + 1, "LIKE") ||
	    !token_can_start_object_name(&parser->tokens[token_index + 2])) {
		return 0;
	}

	return last_qualified_name_token(parser, token_index + 2, close_token_index) == close_token_index - 1;
}

static int validate_create_table_select_tail_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index)
{
	if (token_index <= last_token_index &&
	    parser->tokens[token_index].parser_token == '(' &&
	    !token_can_start_create_table_query(parser, token_index)) {
		if (parser->tokens[token_index].matching_token <= token_index + 2) {
			return 0;
		}
		token_index = parser->tokens[token_index].matching_token;
	}

	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (token_text_equals(parser, token_index, "IGNORE") ||
		    token_text_equals(parser, token_index, "REPLACE")) {
			token_index++;
			if (token_index <= last_token_index && token_text_equals(parser, token_index, "AS")) {
				token_index++;
			}
			return token_index <= last_token_index &&
			       token_can_start_create_table_query(parser, token_index);
		}
		if (token_text_equals(parser, token_index, "AS")) {
			token_index++;
			return token_index <= last_token_index &&
			       token_can_start_create_table_query(parser, token_index);
		}
		if (token_can_start_create_table_query(parser, token_index)) {
			return 1;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}

	return 0;
}

static int validate_create_table_definition_group_syntax(const mylite_parser *parser, size_t open_token_index)
{
	size_t token_index = open_token_index + 1;
	size_t close_token_index;

	if (open_token_index >= parser->token_count ||
	    parser->tokens[open_token_index].parser_token != '(' ||
	    parser->tokens[open_token_index].matching_token <= open_token_index + 2) {
		return 0;
	}

	close_token_index = parser->tokens[open_token_index].matching_token - 1;
	while (token_index < close_token_index) {
		int saw_definition_token = 0;

		while (token_index < close_token_index && parser->tokens[token_index].parser_token != ',') {
			size_t matching_token = parser->tokens[token_index].matching_token;

			saw_definition_token = 1;
			if (matching_token > token_index + 1) {
				token_index = matching_token;
			} else {
				token_index++;
			}
		}
		if (!saw_definition_token) {
			return 0;
		}
		if (token_index >= close_token_index) {
			return 1;
		}
		token_index++;
		if (token_index >= close_token_index) {
			return 0;
		}
	}

	return 1;
}

static int token_can_start_create_table_query(const mylite_parser *parser, size_t token_index)
{
	if (token_index >= parser->token_count) {
		return 0;
	}
	if (parser->tokens[token_index].parser_token == SELECT_T ||
	    parser->tokens[token_index].parser_token == WITH_T ||
	    parser->tokens[token_index].parser_token == TABLE_T ||
	    parser->tokens[token_index].parser_token == VALUES_T) {
		return 1;
	}
	return parser->tokens[token_index].parser_token == '(' &&
	       token_index + 1 < parser->token_count &&
	       parser->tokens[token_index].matching_token > token_index + 1 &&
	       token_can_start_create_table_query(parser, token_index + 1);
}

static int validate_create_database_statement_syntax(const mylite_parser *parser,
                                                     size_t token_index,
                                                     size_t last_token_index)
{
	if (token_index > last_token_index ||
	    (parser->tokens[token_index].parser_token != DATABASE_T &&
	     parser->tokens[token_index].parser_token != SCHEMA_T)) {
		return 0;
	}

	token_index++;
	if (token_index + 2 <= last_token_index &&
	    token_text_equals(parser, token_index, "IF") &&
	    token_text_equals(parser, token_index + 1, "NOT") &&
	    token_text_equals(parser, token_index + 2, "EXISTS")) {
		token_index += 3;
	}
	if (token_index > last_token_index ||
	    !token_can_continue_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index++;

	if (token_index > last_token_index) {
		return 1;
	}
	return validate_database_option_list_syntax(parser, token_index, last_token_index, 0);
}

static int validate_database_option_list_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index,
                                                int allow_read_only)
{
	int saw_option = 0;

	while (token_index <= last_token_index) {
		if (!validate_database_option_syntax(parser,
		                                     token_index,
		                                     last_token_index,
		                                     &token_index,
		                                     allow_read_only)) {
			return 0;
		}
		saw_option = 1;
	}
	return saw_option;
}

static int validate_database_option_syntax(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index,
                                           size_t *next_token_index,
                                           int allow_read_only)
{
	if (token_index <= last_token_index && parser->tokens[token_index].parser_token == DEFAULT_T) {
		token_index++;
	}
	if (token_index > last_token_index) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == CHARACTER_T ||
	    token_text_equals(parser, token_index, "CHARSET")) {
		if (parser->tokens[token_index].parser_token == CHARACTER_T) {
			if (token_index + 1 > last_token_index ||
			    parser->tokens[token_index + 1].parser_token != SET_T) {
				return 0;
			}
			token_index += 2;
		} else {
			token_index++;
		}
		if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
			token_index++;
		}
		if (token_index > last_token_index || !token_is_database_option_value(parser, token_index)) {
			return 0;
		}
		*next_token_index = token_index + 1;
		return 1;
	}

	if (token_text_equals(parser, token_index, "COLLATE")) {
		token_index++;
		if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
			token_index++;
		}
		if (token_index > last_token_index || !token_is_database_option_value(parser, token_index)) {
			return 0;
		}
		*next_token_index = token_index + 1;
		return 1;
	}

	if (token_text_equals(parser, token_index, "ENCRYPTION")) {
		token_index++;
		if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
			token_index++;
		}
		if (token_index > last_token_index ||
		    parser->tokens[token_index].kind != MYLITE_TOKEN_STRING) {
			return 0;
		}
		*next_token_index = token_index + 1;
		return 1;
	}

	if (allow_read_only &&
	    parser->tokens[token_index].parser_token == READ_T &&
	    token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index + 1, "ONLY")) {
		token_index += 2;
		if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
			token_index++;
		}
		if (token_index > last_token_index || !token_is_read_only_value(parser, token_index)) {
			return 0;
		}
		*next_token_index = token_index + 1;
		return 1;
	}

	return 0;
}

static int token_starts_database_option(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index,
                                        int allow_read_only)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}
	if (parser->tokens[token_index].parser_token == DEFAULT_T) {
		token_index++;
		if (token_index > last_token_index || token_index >= parser->token_count) {
			return 0;
		}
	}
	return parser->tokens[token_index].parser_token == CHARACTER_T ||
	       token_text_equals(parser, token_index, "CHARSET") ||
	       token_text_equals(parser, token_index, "COLLATE") ||
	       token_text_equals(parser, token_index, "ENCRYPTION") ||
	       (allow_read_only &&
	        parser->tokens[token_index].parser_token == READ_T &&
	        token_index + 1 <= last_token_index &&
	        token_text_equals(parser, token_index + 1, "ONLY"));
}

static int token_is_database_option_value(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (token_can_continue_object_name(&parser->tokens[token_index]) ||
	        parser->tokens[token_index].kind == MYLITE_TOKEN_STRING);
}

static int token_is_read_only_value(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (parser->tokens[token_index].parser_token == DEFAULT_T ||
	        token_text_equals(parser, token_index, "0") ||
	        token_text_equals(parser, token_index, "1"));
}

static int validate_create_resource_group_statement_syntax(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index)
{
	if (!token_is_resource_group_sequence(parser, token_index, last_token_index)) {
		return 0;
	}

	token_index += 2;
	if (token_index > last_token_index ||
	    !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index++;

	if (token_index > last_token_index || !token_text_equals(parser, token_index, "TYPE")) {
		return 0;
	}
	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
		token_index++;
	}
	if (token_index > last_token_index ||
	    (!token_text_equals(parser, token_index, "SYSTEM") &&
	     !token_text_equals(parser, token_index, "USER"))) {
		return 0;
	}
	token_index++;

	if (token_index <= last_token_index && token_text_equals(parser, token_index, "VCPU")) {
		if (!validate_resource_group_vcpu_clause_syntax(parser,
		                                                token_index,
		                                                last_token_index,
		                                                &token_index)) {
			return 0;
		}
	}
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "THREAD_PRIORITY")) {
		if (!validate_resource_group_thread_priority_clause_syntax(parser,
		                                                           token_index,
		                                                           last_token_index,
		                                                           &token_index)) {
			return 0;
		}
	}
	if (token_index <= last_token_index &&
	    (token_text_equals(parser, token_index, "ENABLE") ||
	     token_text_equals(parser, token_index, "DISABLE"))) {
		token_index++;
	}

	return token_index > last_token_index;
}

static int validate_create_logfile_group_statement_syntax(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index)
{
	if (!token_is_logfile_group_sequence(parser, token_index, last_token_index)) {
		return 0;
	}

	token_index += 2;
	if (token_index > last_token_index ||
	    !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index++;

	if (!validate_logfile_group_add_undofile_clause_syntax(parser,
	                                                       token_index,
	                                                       last_token_index,
	                                                       &token_index)) {
		return 0;
	}
	if (token_index <= last_token_index &&
	    token_text_equals(parser, token_index, "INITIAL_SIZE") &&
	    !validate_storage_size_clause_syntax(parser,
	                                         token_index,
	                                         last_token_index,
	                                         "INITIAL_SIZE",
	                                         &token_index)) {
		return 0;
	}
	if (token_index <= last_token_index &&
	    token_text_equals(parser, token_index, "UNDO_BUFFER_SIZE") &&
	    !validate_storage_size_clause_syntax(parser,
	                                         token_index,
	                                         last_token_index,
	                                         "UNDO_BUFFER_SIZE",
	                                         &token_index)) {
		return 0;
	}
	if (token_index <= last_token_index &&
	    token_text_equals(parser, token_index, "REDO_BUFFER_SIZE") &&
	    !validate_storage_size_clause_syntax(parser,
	                                         token_index,
	                                         last_token_index,
	                                         "REDO_BUFFER_SIZE",
	                                         &token_index)) {
		return 0;
	}
	if (token_index <= last_token_index &&
	    token_text_equals(parser, token_index, "NODEGROUP") &&
	    !validate_storage_nodegroup_clause_syntax(parser,
	                                              token_index,
	                                              last_token_index,
	                                              &token_index)) {
		return 0;
	}
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "WAIT")) {
		token_index++;
	}
	if (token_index <= last_token_index &&
	    token_text_equals(parser, token_index, "COMMENT") &&
	    !validate_storage_comment_clause_syntax(parser,
	                                            token_index,
	                                            last_token_index,
	                                            &token_index)) {
		return 0;
	}

	return validate_storage_engine_tail_syntax(parser, token_index, last_token_index);
}

static int validate_logfile_group_add_undofile_clause_syntax(const mylite_parser *parser,
                                                             size_t token_index,
                                                             size_t last_token_index,
                                                             size_t *next_token_index)
{
	if (token_index + 2 > last_token_index ||
	    !token_text_equals(parser, token_index, "ADD") ||
	    !token_text_equals(parser, token_index + 1, "UNDOFILE") ||
	    parser->tokens[token_index + 2].kind != MYLITE_TOKEN_STRING) {
		return 0;
	}

	*next_token_index = token_index + 3;
	return 1;
}

static int validate_storage_size_clause_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index,
                                               const char *clause_name,
                                               size_t *next_token_index)
{
	int saw_equals = 0;

	if (token_index > last_token_index || !token_text_equals(parser, token_index, clause_name)) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
		saw_equals = 1;
		token_index++;
	}
	if (strcmp(clause_name, "FILE_BLOCK_SIZE") == 0 && !saw_equals) {
		return 0;
	}
	if (token_index > last_token_index || !token_is_storage_size_literal(parser, token_index)) {
		return 0;
	}

	*next_token_index = token_index + 1;
	return 1;
}

static int validate_storage_nodegroup_clause_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    size_t *next_token_index)
{
	if (token_index > last_token_index || !token_text_equals(parser, token_index, "NODEGROUP")) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
		token_index++;
	}
	if (token_index > last_token_index ||
	    parser->tokens[token_index].kind != MYLITE_TOKEN_NUMBER) {
		return 0;
	}

	*next_token_index = token_index + 1;
	return 1;
}

static int validate_storage_comment_clause_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index,
                                                  size_t *next_token_index)
{
	if (token_index > last_token_index || !token_text_equals(parser, token_index, "COMMENT")) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
		token_index++;
	}
	if (token_index > last_token_index ||
	    parser->tokens[token_index].kind != MYLITE_TOKEN_STRING) {
		return 0;
	}

	*next_token_index = token_index + 1;
	return 1;
}

static int token_is_storage_size_literal(const mylite_parser *parser, size_t token_index)
{
	const mylite_token *token;
	size_t offset;

	if (token_index >= parser->token_count) {
		return 0;
	}

	token = &parser->tokens[token_index];
	if (token->kind == MYLITE_TOKEN_NUMBER) {
		return 1;
	}
	if (token->kind == MYLITE_TOKEN_IDENTIFIER) {
		return source_span_is_storage_size_literal(parser->lexer.input,
		                                           token->start_offset,
		                                           token->end_offset);
	}
	if (token->kind == MYLITE_TOKEN_STRING) {
		for (offset = token->start_offset; offset < token->end_offset; offset++) {
			if (parser->lexer.input[offset] == '\'' || parser->lexer.input[offset] == '"') {
				break;
			}
		}
		if (offset + 1 >= token->end_offset ||
		    parser->lexer.input[token->end_offset - 1] != parser->lexer.input[offset]) {
			return 0;
		}
		return source_span_is_storage_size_literal(parser->lexer.input,
		                                           offset + 1,
		                                           token->end_offset - 1);
	}
	return 0;
}

static int source_span_is_storage_size_literal(const char *source, size_t start_offset, size_t end_offset)
{
	size_t offset;
	size_t suffix_length;

	if (start_offset >= end_offset ||
	    source[start_offset] < '0' ||
	    source[start_offset] > '9') {
		return 0;
	}

	offset = start_offset;
	while (offset < end_offset &&
	       source[offset] >= '0' &&
	       source[offset] <= '9') {
		offset++;
	}
	suffix_length = end_offset - offset;
	if (suffix_length == 0) {
		return 1;
	}
	if (suffix_length != 1) {
		return 0;
	}
	return source[offset] == 'K' ||
	       source[offset] == 'k' ||
	       source[offset] == 'M' ||
	       source[offset] == 'm' ||
	       source[offset] == 'G' ||
	       source[offset] == 'g' ||
	       source[offset] == 'T' ||
	       source[offset] == 't';
}

static int validate_create_tablespace_statement_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       int is_undo_tablespace)
{
	int saw_datafile = 0;

	if (is_undo_tablespace) {
		if (!token_is_undo_tablespace_sequence(parser, token_index, last_token_index)) {
			return 0;
		}
		token_index += 2;
	} else {
		if (!token_is_tablespace_token(parser, token_index)) {
			return 0;
		}
		token_index++;
	}

	if (token_index > last_token_index ||
	    !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index++;

	if (token_index <= last_token_index && token_text_equals(parser, token_index, "ADD")) {
		if (!validate_tablespace_datafile_clause_syntax(parser,
		                                                token_index,
		                                                last_token_index,
		                                                "ADD",
		                                                &token_index)) {
			return 0;
		}
		saw_datafile = 1;
	}
	if (is_undo_tablespace && !saw_datafile) {
		return 0;
	}
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "AUTOEXTEND_SIZE")) {
		if (!validate_storage_size_clause_syntax(parser,
		                                         token_index,
		                                         last_token_index,
		                                         "AUTOEXTEND_SIZE",
		                                         &token_index)) {
			return 0;
		}
	}
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "FILE_BLOCK_SIZE")) {
		if (!validate_storage_size_clause_syntax(parser,
		                                         token_index,
		                                         last_token_index,
		                                         "FILE_BLOCK_SIZE",
		                                         &token_index)) {
			return 0;
		}
	}
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "ENCRYPTION")) {
		if (!validate_tablespace_encryption_clause_syntax(parser,
		                                                  token_index,
		                                                  last_token_index,
		                                                  &token_index)) {
			return 0;
		}
	}
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "USE")) {
		if (!saw_datafile) {
			return 0;
		}
		if (!validate_tablespace_use_logfile_group_clause_syntax(parser,
		                                                         token_index,
		                                                         last_token_index,
		                                                         &token_index)) {
			return 0;
		}
		if (token_index <= last_token_index && token_text_equals(parser, token_index, "EXTENT_SIZE")) {
			if (!validate_storage_size_clause_syntax(parser,
			                                         token_index,
			                                         last_token_index,
			                                         "EXTENT_SIZE",
			                                         &token_index)) {
				return 0;
			}
		}
		if (token_index <= last_token_index && token_text_equals(parser, token_index, "INITIAL_SIZE")) {
			if (!validate_storage_size_clause_syntax(parser,
			                                         token_index,
			                                         last_token_index,
			                                         "INITIAL_SIZE",
			                                         &token_index)) {
				return 0;
			}
		}
		if (token_index <= last_token_index && token_text_equals(parser, token_index, "MAX_SIZE")) {
			if (!validate_storage_size_clause_syntax(parser,
			                                         token_index,
			                                         last_token_index,
			                                         "MAX_SIZE",
			                                         &token_index)) {
				return 0;
			}
		}
		if (token_index <= last_token_index && token_text_equals(parser, token_index, "NODEGROUP")) {
			if (!validate_storage_nodegroup_clause_syntax(parser,
			                                              token_index,
			                                              last_token_index,
			                                              &token_index)) {
				return 0;
			}
		}
		if (token_index <= last_token_index && token_text_equals(parser, token_index, "WAIT")) {
			token_index++;
		}
		if (token_index <= last_token_index && token_text_equals(parser, token_index, "COMMENT")) {
			if (!validate_storage_comment_clause_syntax(parser,
			                                            token_index,
			                                            last_token_index,
			                                            &token_index)) {
				return 0;
			}
		}
	}

	return validate_tablespace_optional_storage_tail_syntax(parser, token_index, last_token_index);
}

static int validate_tablespace_datafile_clause_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index,
                                                      const char *action,
                                                      size_t *next_token_index)
{
	if (token_index + 2 > last_token_index ||
	    !token_text_equals(parser, token_index, action) ||
	    !token_text_equals(parser, token_index + 1, "DATAFILE") ||
	    parser->tokens[token_index + 2].kind != MYLITE_TOKEN_STRING) {
		return 0;
	}

	*next_token_index = token_index + 3;
	return 1;
}

static int validate_tablespace_use_logfile_group_clause_syntax(const mylite_parser *parser,
                                                               size_t token_index,
                                                               size_t last_token_index,
                                                               size_t *next_token_index)
{
	if (token_index + 3 > last_token_index ||
	    !token_text_equals(parser, token_index, "USE") ||
	    !token_text_equals(parser, token_index + 1, "LOGFILE") ||
	    parser->tokens[token_index + 2].parser_token != GROUP_T ||
	    !token_can_start_object_name(&parser->tokens[token_index + 3])) {
		return 0;
	}

	*next_token_index = token_index + 4;
	return 1;
}

static int validate_tablespace_encryption_clause_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index,
                                                        size_t *next_token_index)
{
	if (token_index > last_token_index || !token_text_equals(parser, token_index, "ENCRYPTION")) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
		token_index++;
	}
	if (token_index > last_token_index ||
	    parser->tokens[token_index].kind != MYLITE_TOKEN_STRING) {
		return 0;
	}

	*next_token_index = token_index + 1;
	return 1;
}

static int validate_tablespace_engine_attribute_clause_syntax(const mylite_parser *parser,
                                                              size_t token_index,
                                                              size_t last_token_index,
                                                              size_t *next_token_index)
{
	if (token_index > last_token_index || !token_text_equals(parser, token_index, "ENGINE_ATTRIBUTE")) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
		token_index++;
	}
	if (token_index > last_token_index ||
	    parser->tokens[token_index].kind != MYLITE_TOKEN_STRING) {
		return 0;
	}

	*next_token_index = token_index + 1;
	return 1;
}

static int validate_tablespace_optional_storage_tail_syntax(const mylite_parser *parser,
                                                            size_t token_index,
                                                            size_t last_token_index)
{
	if (token_index <= last_token_index && parser->tokens[token_index].parser_token == ENGINE_T) {
		if (!validate_storage_engine_clause_syntax(parser,
		                                           token_index,
		                                           last_token_index,
		                                           &token_index)) {
			return 0;
		}
	}
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "ENGINE_ATTRIBUTE")) {
		if (!validate_tablespace_engine_attribute_clause_syntax(parser,
		                                                        token_index,
		                                                        last_token_index,
		                                                        &token_index)) {
			return 0;
		}
	}
	return token_index > last_token_index;
}

static int validate_create_spatial_reference_system_statement_syntax(const mylite_parser *parser,
                                                                     size_t token_index,
                                                                     size_t last_token_index)
{
	int seen_attributes = 0;
	int saw_attribute = 0;

	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "OR") &&
	    token_text_equals(parser, token_index + 1, "REPLACE")) {
		token_index += 2;
		if (!token_is_spatial_reference_system_sequence(parser, token_index, last_token_index)) {
			return 0;
		}
		token_index += 3;
	} else {
		if (!token_is_spatial_reference_system_sequence(parser, token_index, last_token_index)) {
			return 0;
		}
		token_index += 3;
		if (token_index + 2 <= last_token_index &&
		    token_text_equals(parser, token_index, "IF") &&
		    token_text_equals(parser, token_index + 1, "NOT") &&
		    token_text_equals(parser, token_index + 2, "EXISTS")) {
			token_index += 3;
		}
	}

	if (token_index > last_token_index ||
	    parser->tokens[token_index].kind != MYLITE_TOKEN_NUMBER) {
		return 0;
	}
	token_index++;

	while (token_index <= last_token_index) {
		int attribute_kind = 0;

		if (!validate_spatial_reference_system_attribute_syntax(parser,
		                                                        token_index,
		                                                        last_token_index,
		                                                        &token_index,
		                                                        &attribute_kind)) {
			return 0;
		}
		if ((seen_attributes & attribute_kind) != 0) {
			return 0;
		}
		seen_attributes |= attribute_kind;
		saw_attribute = 1;
	}

	return saw_attribute;
}

static int validate_spatial_reference_system_attribute_syntax(const mylite_parser *parser,
                                                              size_t token_index,
                                                              size_t last_token_index,
                                                              size_t *next_token_index,
                                                              int *attribute_kind)
{
	if (token_text_equals(parser, token_index, "NAME")) {
		if (token_index + 1 > last_token_index ||
		    parser->tokens[token_index + 1].kind != MYLITE_TOKEN_STRING) {
			return 0;
		}
		*attribute_kind = 1 << 0;
		*next_token_index = token_index + 2;
		return 1;
	}
	if (token_text_equals(parser, token_index, "DEFINITION")) {
		if (token_index + 1 > last_token_index ||
		    parser->tokens[token_index + 1].kind != MYLITE_TOKEN_STRING) {
			return 0;
		}
		*attribute_kind = 1 << 1;
		*next_token_index = token_index + 2;
		return 1;
	}
	if (token_text_equals(parser, token_index, "ORGANIZATION")) {
		if (token_index + 4 > last_token_index ||
		    parser->tokens[token_index + 1].kind != MYLITE_TOKEN_STRING ||
		    !token_text_equals(parser, token_index + 2, "IDENTIFIED") ||
		    !token_text_equals(parser, token_index + 3, "BY") ||
		    parser->tokens[token_index + 4].kind != MYLITE_TOKEN_NUMBER) {
			return 0;
		}
		*attribute_kind = 1 << 2;
		*next_token_index = token_index + 5;
		return 1;
	}
	if (token_text_equals(parser, token_index, "DESCRIPTION")) {
		if (token_index + 1 > last_token_index ||
		    parser->tokens[token_index + 1].kind != MYLITE_TOKEN_STRING) {
			return 0;
		}
		*attribute_kind = 1 << 3;
		*next_token_index = token_index + 2;
		return 1;
	}

	return 0;
}

static int token_is_create_spatial_reference_system_start(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index)
{
	if (token_is_spatial_reference_system_sequence(parser, token_index, last_token_index)) {
		return 1;
	}
	return token_index + 4 <= last_token_index &&
	       token_text_equals(parser, token_index, "OR") &&
	       token_text_equals(parser, token_index + 1, "REPLACE") &&
	       token_is_spatial_reference_system_sequence(parser, token_index + 2, last_token_index);
}

static int validate_create_server_statement_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index)
{
	if (!token_text_equals(parser, token_index, "SERVER")) {
		return 0;
	}

	token_index++;
	if (token_index + 4 > last_token_index ||
	    !token_can_start_object_name(&parser->tokens[token_index]) ||
	    !token_text_equals(parser, token_index + 1, "FOREIGN") ||
	    !token_text_equals(parser, token_index + 2, "DATA") ||
	    !token_text_equals(parser, token_index + 3, "WRAPPER") ||
	    !token_can_start_object_name(&parser->tokens[token_index + 4])) {
		return 0;
	}

	token_index += 5;
	return validate_server_options_clause_syntax(parser,
	                                             token_index,
	                                             last_token_index,
	                                             &token_index) &&
	       token_index > last_token_index;
}

static int validate_server_options_clause_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 size_t *next_token_index)
{
	int saw_option = 0;

	if (token_index + 2 > last_token_index ||
	    !token_text_equals(parser, token_index, "OPTIONS") ||
	    parser->tokens[token_index + 1].parser_token != '(') {
		return 0;
	}

	token_index += 2;
	while (token_index <= last_token_index) {
		if (parser->tokens[token_index].parser_token == ')') {
			*next_token_index = token_index + 1;
			return saw_option;
		}
		if (!validate_server_option_syntax(parser,
		                                   token_index,
		                                   last_token_index,
		                                   &token_index)) {
			return 0;
		}
		saw_option = 1;

		if (token_index > last_token_index) {
			return 0;
		}
		if (parser->tokens[token_index].parser_token == ',') {
			token_index++;
			if (token_index > last_token_index ||
			    parser->tokens[token_index].parser_token == ')') {
				return 0;
			}
			continue;
		}
		if (parser->tokens[token_index].parser_token != ')') {
			return 0;
		}
	}

	return 0;
}

static int validate_server_option_syntax(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index,
                                         size_t *next_token_index)
{
	if (token_index + 1 > last_token_index) {
		return 0;
	}

	if (token_is_server_character_option(parser, token_index)) {
		if (parser->tokens[token_index + 1].kind != MYLITE_TOKEN_STRING) {
			return 0;
		}
		*next_token_index = token_index + 2;
		return 1;
	}

	if (token_is_server_numeric_option(parser, token_index)) {
		if (parser->tokens[token_index + 1].kind != MYLITE_TOKEN_NUMBER) {
			return 0;
		}
		*next_token_index = token_index + 2;
		return 1;
	}

	return 0;
}

static int token_is_server_character_option(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "HOST") ||
	       token_text_equals(parser, token_index, "DATABASE") ||
	       parser->tokens[token_index].parser_token == USER_T ||
	       token_text_equals(parser, token_index, "PASSWORD") ||
	       token_text_equals(parser, token_index, "SOCKET") ||
	       token_text_equals(parser, token_index, "OWNER");
}

static int token_is_server_numeric_option(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "PORT");
}

static int validate_create_user_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index)
{
	int saw_user = 0;

	if (token_index >= parser->token_count ||
	    parser->tokens[token_index].parser_token != USER_T) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "IF")) {
		if (token_index + 2 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "NOT") ||
		    !token_text_equals(parser, token_index + 2, "EXISTS")) {
			return 0;
		}
		token_index += 3;
	}

	while (token_index <= last_token_index) {
		if (!validate_create_user_entry_syntax(parser, token_index, last_token_index, &token_index)) {
			return 0;
		}
		saw_user = 1;

		if (token_index > last_token_index) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token == ',') {
			token_index++;
			if (token_index > last_token_index) {
				return 0;
			}
			continue;
		}
		break;
	}

	return saw_user && validate_create_user_tail_syntax(parser, token_index, last_token_index);
}

static int validate_create_user_entry_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index,
                                             size_t *next_token_index)
{
	if (!validate_principal_name_syntax(parser, token_index, last_token_index, 0, &token_index)) {
		return 0;
	}

	if (token_index <= last_token_index && token_text_equals(parser, token_index, "IDENTIFIED")) {
		return validate_create_user_auth_option_syntax(parser,
		                                               token_index,
		                                               last_token_index,
		                                               next_token_index,
		                                               1);
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_create_user_auth_option_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index,
                                                   size_t *next_token_index,
                                                   int allow_initial_auth)
{
	if (token_index > last_token_index || !token_text_equals(parser, token_index, "IDENTIFIED")) {
		return 0;
	}

	if (!validate_create_user_identified_tail_syntax(parser,
	                                                token_index + 1,
	                                                last_token_index,
	                                                &token_index,
	                                                allow_initial_auth)) {
		return 0;
	}

	return validate_create_user_auth_mfa_tail_syntax(parser,
	                                                 token_index,
	                                                 last_token_index,
	                                                 next_token_index);
}

static int validate_create_user_identified_tail_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index,
                                                       int allow_initial_auth)
{
	if (token_index > last_token_index) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "BY")) {
		token_index++;
		if (token_index + 1 <= last_token_index &&
		    token_text_equals(parser, token_index, "RANDOM") &&
		    token_text_equals(parser, token_index + 1, "PASSWORD")) {
			*next_token_index = token_index + 2;
			return 1;
		}
		if (token_is_create_user_auth_string(parser, token_index)) {
			*next_token_index = token_index + 1;
			return 1;
		}
		return 0;
	}

	if (!token_text_equals(parser, token_index, "WITH")) {
		return 0;
	}
	token_index++;
	if (token_index > last_token_index ||
	    !token_can_be_create_user_auth_plugin(parser, token_index)) {
		return 0;
	}
	token_index++;

	if (token_index > last_token_index) {
		*next_token_index = token_index;
		return 1;
	}
	if (token_text_equals(parser, token_index, "BY")) {
		token_index++;
		if (token_index + 1 <= last_token_index &&
		    token_text_equals(parser, token_index, "RANDOM") &&
		    token_text_equals(parser, token_index + 1, "PASSWORD")) {
			*next_token_index = token_index + 2;
			return 1;
		}
		if (token_is_create_user_auth_string(parser, token_index)) {
			*next_token_index = token_index + 1;
			return 1;
		}
		return 0;
	}
	if (token_text_equals(parser, token_index, "AS")) {
		token_index++;
		if (!token_is_create_user_auth_hash(parser, token_index)) {
			return 0;
		}
		*next_token_index = token_index + 1;
		return 1;
	}
	if (allow_initial_auth && token_text_equals(parser, token_index, "INITIAL")) {
		return validate_create_user_initial_auth_option_syntax(parser,
		                                                       token_index,
		                                                       last_token_index,
		                                                       next_token_index);
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_create_user_initial_auth_option_syntax(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index,
                                                           size_t *next_token_index)
{
	if (token_index + 2 > last_token_index ||
	    !token_text_equals(parser, token_index, "INITIAL") ||
	    !token_text_equals(parser, token_index + 1, "AUTHENTICATION") ||
	    !token_text_equals(parser, token_index + 2, "IDENTIFIED")) {
		return 0;
	}

	token_index += 3;
	if (token_index > last_token_index) {
		return 0;
	}
	if (token_text_equals(parser, token_index, "BY")) {
		token_index++;
		if (token_index + 1 <= last_token_index &&
		    token_text_equals(parser, token_index, "RANDOM") &&
		    token_text_equals(parser, token_index + 1, "PASSWORD")) {
			*next_token_index = token_index + 2;
			return 1;
		}
		if (token_is_create_user_auth_string(parser, token_index)) {
			*next_token_index = token_index + 1;
			return 1;
		}
		return 0;
	}
	if (token_index + 2 <= last_token_index &&
	    token_text_equals(parser, token_index, "WITH") &&
	    token_can_be_create_user_auth_plugin(parser, token_index + 1) &&
	    token_text_equals(parser, token_index + 2, "AS")) {
		token_index += 3;
		if (!token_is_create_user_auth_hash(parser, token_index)) {
			return 0;
		}
		*next_token_index = token_index + 1;
		return 1;
	}

	return 0;
}

static int validate_create_user_auth_mfa_tail_syntax(const mylite_parser *parser,
                                                     size_t token_index,
                                                     size_t last_token_index,
                                                     size_t *next_token_index)
{
	int factor_count = 0;

	while (token_index <= last_token_index && token_text_equals(parser, token_index, "AND")) {
		factor_count++;
		if (factor_count > 2) {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index || !token_text_equals(parser, token_index, "IDENTIFIED")) {
			return 0;
		}
		if (!validate_create_user_identified_tail_syntax(parser,
		                                                token_index + 1,
		                                                last_token_index,
		                                                &token_index,
		                                                0)) {
			return 0;
		}
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_create_user_tail_syntax(const mylite_parser *parser,
                                            size_t token_index,
                                            size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 1;
	}

	if (token_text_equals(parser, token_index, "DEFAULT")) {
		if (!validate_create_user_default_role_clause_syntax(parser,
		                                                     token_index,
		                                                     last_token_index,
		                                                     &token_index)) {
			return 0;
		}
	}
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "REQUIRE")) {
		if (!validate_create_user_tls_clause_syntax(parser,
		                                            token_index,
		                                            last_token_index,
		                                            &token_index)) {
			return 0;
		}
	}
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "WITH")) {
		if (!validate_create_user_resource_clause_syntax(parser,
		                                                 token_index,
		                                                 last_token_index,
		                                                 &token_index)) {
			return 0;
		}
	}

	while (token_index <= last_token_index) {
		if (!validate_create_user_password_or_lock_option_syntax(parser,
		                                                         token_index,
		                                                         last_token_index,
		                                                         &token_index)) {
			break;
		}
	}

	if (token_index > last_token_index) {
		return 1;
	}
	if ((token_text_equals(parser, token_index, "COMMENT") ||
	     token_text_equals(parser, token_index, "ATTRIBUTE")) &&
	    token_index + 1 == last_token_index &&
	    token_is_create_user_auth_string(parser, token_index + 1)) {
		return 1;
	}

	return 0;
}

static int validate_create_user_default_role_clause_syntax(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index,
                                                           size_t *next_token_index)
{
	int saw_role = 0;

	if (token_index + 1 > last_token_index ||
	    !token_text_equals(parser, token_index, "DEFAULT") ||
	    parser->tokens[token_index + 1].parser_token != ROLE_T) {
		return 0;
	}

	token_index += 2;
	while (token_index <= last_token_index) {
		if (token_is_create_user_tail_boundary(parser, token_index)) {
			break;
		}
		if (!validate_principal_name_syntax(parser, token_index, last_token_index, 0, &token_index)) {
			return 0;
		}
		saw_role = 1;

		if (token_index > last_token_index || token_is_create_user_tail_boundary(parser, token_index)) {
			break;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	if (!saw_role) {
		return 0;
	}
	*next_token_index = token_index;
	return 1;
}

static int validate_create_user_tls_clause_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index,
                                                  size_t *next_token_index)
{
	int saw_option = 0;

	if (token_index > last_token_index || !token_text_equals(parser, token_index, "REQUIRE")) {
		return 0;
	}
	token_index++;
	if (token_index > last_token_index) {
		return 0;
	}
	if (token_text_equals(parser, token_index, "NONE")) {
		*next_token_index = token_index + 1;
		return 1;
	}

	while (token_index <= last_token_index) {
		if (token_is_create_user_tail_boundary(parser, token_index)) {
			break;
		}
		if (token_text_equals(parser, token_index, "AND")) {
			token_index++;
			if (token_index > last_token_index) {
				return 0;
			}
		}
		if (!validate_create_user_tls_option_syntax(parser,
		                                            token_index,
		                                            last_token_index,
		                                            &token_index)) {
			return 0;
		}
		saw_option = 1;
	}

	if (!saw_option) {
		return 0;
	}
	*next_token_index = token_index;
	return 1;
}

static int validate_create_user_tls_option_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index,
                                                  size_t *next_token_index)
{
	if (token_text_equals(parser, token_index, "SSL") ||
	    token_text_equals(parser, token_index, "X509")) {
		*next_token_index = token_index + 1;
		return 1;
	}

	if ((token_text_equals(parser, token_index, "CIPHER") ||
	     token_text_equals(parser, token_index, "ISSUER") ||
	     token_text_equals(parser, token_index, "SUBJECT")) &&
	    token_index + 1 <= last_token_index &&
	    token_is_create_user_auth_string(parser, token_index + 1)) {
		*next_token_index = token_index + 2;
		return 1;
	}

	return 0;
}

static int validate_create_user_resource_clause_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index)
{
	int saw_option = 0;

	if (token_index > last_token_index || !token_text_equals(parser, token_index, "WITH")) {
		return 0;
	}
	token_index++;

	while (token_index <= last_token_index && !token_is_create_user_tail_boundary(parser, token_index)) {
		if (token_index + 1 > last_token_index ||
		    !token_is_create_user_resource_option(parser, token_index) ||
		    parser->tokens[token_index + 1].kind != MYLITE_TOKEN_NUMBER) {
			return 0;
		}
		token_index += 2;
		saw_option = 1;
	}

	if (!saw_option) {
		return 0;
	}
	*next_token_index = token_index;
	return 1;
}

static int validate_create_user_password_or_lock_option_syntax(const mylite_parser *parser,
                                                               size_t token_index,
                                                               size_t last_token_index,
                                                               size_t *next_token_index)
{
	if (token_text_equals(parser, token_index, "ACCOUNT")) {
		if (token_index + 1 > last_token_index ||
		    (!token_text_equals(parser, token_index + 1, "LOCK") &&
		     !token_text_equals(parser, token_index + 1, "UNLOCK"))) {
			return 0;
		}
		*next_token_index = token_index + 2;
		return 1;
	}

	if (token_text_equals(parser, token_index, "FAILED_LOGIN_ATTEMPTS")) {
		if (token_index + 1 > last_token_index ||
		    parser->tokens[token_index + 1].kind != MYLITE_TOKEN_NUMBER) {
			return 0;
		}
		*next_token_index = token_index + 2;
		return 1;
	}

	if (token_text_equals(parser, token_index, "PASSWORD_LOCK_TIME")) {
		if (token_index + 1 > last_token_index ||
		    (parser->tokens[token_index + 1].kind != MYLITE_TOKEN_NUMBER &&
		     !token_text_equals(parser, token_index + 1, "UNBOUNDED"))) {
			return 0;
		}
		*next_token_index = token_index + 2;
		return 1;
	}

	if (!token_text_equals(parser, token_index, "PASSWORD") ||
	    token_index + 1 > last_token_index) {
		return 0;
	}

	token_index++;
	if (token_text_equals(parser, token_index, "EXPIRE")) {
		token_index++;
		if (token_index > last_token_index ||
		    token_is_create_user_tail_boundary(parser, token_index)) {
			*next_token_index = token_index;
			return 1;
		}
		if (token_text_equals(parser, token_index, "DEFAULT") ||
		    token_text_equals(parser, token_index, "NEVER")) {
			*next_token_index = token_index + 1;
			return 1;
		}
		if (token_index + 2 <= last_token_index &&
		    token_text_equals(parser, token_index, "INTERVAL") &&
		    parser->tokens[token_index + 1].kind == MYLITE_TOKEN_NUMBER &&
		    token_text_equals(parser, token_index + 2, "DAY")) {
			*next_token_index = token_index + 3;
			return 1;
		}
		return 0;
	}

	if (token_text_equals(parser, token_index, "HISTORY")) {
		if (token_index + 1 > last_token_index ||
		    (!token_text_equals(parser, token_index + 1, "DEFAULT") &&
		     parser->tokens[token_index + 1].kind != MYLITE_TOKEN_NUMBER)) {
			return 0;
		}
		*next_token_index = token_index + 2;
		return 1;
	}

	if (token_text_equals(parser, token_index, "REUSE")) {
		if (token_index + 2 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "INTERVAL")) {
			return 0;
		}
		token_index += 2;
		if (token_text_equals(parser, token_index, "DEFAULT")) {
			*next_token_index = token_index + 1;
			return 1;
		}
		if (token_index + 1 <= last_token_index &&
		    parser->tokens[token_index].kind == MYLITE_TOKEN_NUMBER &&
		    token_text_equals(parser, token_index + 1, "DAY")) {
			*next_token_index = token_index + 2;
			return 1;
		}
		return 0;
	}

	if (token_text_equals(parser, token_index, "REQUIRE")) {
		if (token_index + 1 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "CURRENT")) {
			return 0;
		}
		token_index += 2;
		if (token_index <= last_token_index &&
		    (token_text_equals(parser, token_index, "DEFAULT") ||
		     token_text_equals(parser, token_index, "OPTIONAL"))) {
			token_index++;
		}
		*next_token_index = token_index;
		return 1;
	}

	return 0;
}

static int token_is_create_user_tail_boundary(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "REQUIRE") ||
	       token_text_equals(parser, token_index, "WITH") ||
	       token_text_equals(parser, token_index, "PASSWORD") ||
	       token_text_equals(parser, token_index, "FAILED_LOGIN_ATTEMPTS") ||
	       token_text_equals(parser, token_index, "PASSWORD_LOCK_TIME") ||
	       token_text_equals(parser, token_index, "ACCOUNT") ||
	       token_text_equals(parser, token_index, "COMMENT") ||
	       token_text_equals(parser, token_index, "ATTRIBUTE");
}

static int token_is_create_user_auth_string(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       parser->tokens[token_index].kind == MYLITE_TOKEN_STRING;
}

static int token_is_create_user_auth_hash(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (parser->tokens[token_index].kind == MYLITE_TOKEN_STRING ||
	        parser->tokens[token_index].kind == MYLITE_TOKEN_NUMBER);
}

static int token_can_be_create_user_auth_plugin(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (token_can_continue_object_name(&parser->tokens[token_index]) ||
	        parser->tokens[token_index].kind == MYLITE_TOKEN_STRING);
}

static int token_is_create_user_resource_option(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "MAX_QUERIES_PER_HOUR") ||
	       token_text_equals(parser, token_index, "MAX_UPDATES_PER_HOUR") ||
	       token_text_equals(parser, token_index, "MAX_CONNECTIONS_PER_HOUR") ||
	       token_text_equals(parser, token_index, "MAX_USER_CONNECTIONS");
}

static int validate_alter_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (token_starts_view_statement(parser, token_index, last_token_index)) {
		return validate_view_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_starts_event_statement(parser, token_index, last_token_index)) {
		return validate_alter_event_statement_syntax(parser, token_index, last_token_index);
	}
	if (parser->tokens[token_index].parser_token == USER_T) {
		return validate_alter_user_statement_syntax(parser, token_index, last_token_index);
	}
	if (parser->tokens[token_index].parser_token == DATABASE_T ||
	    parser->tokens[token_index].parser_token == SCHEMA_T) {
		return validate_alter_database_statement_syntax(parser, token_index, last_token_index);
	}
	if (parser->tokens[token_index].parser_token == FUNCTION_T ||
	    parser->tokens[token_index].parser_token == PROCEDURE_T) {
		return validate_alter_routine_statement_syntax(parser, token_index, last_token_index);
	}
	if (parser->tokens[token_index].parser_token == TABLE_T) {
		return validate_alter_table_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_text_equals(parser, token_index, "INSTANCE")) {
		return validate_alter_instance_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_is_logfile_group_sequence(parser, token_index, last_token_index)) {
		return validate_alter_logfile_group_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_is_undo_tablespace_sequence(parser, token_index, last_token_index)) {
		return validate_alter_tablespace_statement_syntax(parser, token_index, last_token_index, 1);
	}
	if (token_is_tablespace_token(parser, token_index)) {
		return validate_alter_tablespace_statement_syntax(parser, token_index, last_token_index, 0);
	}
	if (token_text_equals(parser, token_index, "SERVER")) {
		return validate_alter_server_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_is_resource_group_sequence(parser, token_index, last_token_index)) {
		return validate_alter_resource_group_statement_syntax(parser, token_index, last_token_index);
	}
	return 1;
}

static int validate_alter_database_statement_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index)
{
	if (token_index > last_token_index ||
	    (parser->tokens[token_index].parser_token != DATABASE_T &&
	     parser->tokens[token_index].parser_token != SCHEMA_T)) {
		return 0;
	}

	token_index++;
	if (token_index > last_token_index) {
		return 0;
	}
	if (!token_starts_database_option(parser, token_index, last_token_index, 1)) {
		if (!token_can_continue_object_name(&parser->tokens[token_index])) {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	return validate_database_option_list_syntax(parser, token_index, last_token_index, 1);
}

static int validate_alter_routine_statement_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index)
{
	size_t last_name_token;

	if (token_index > last_token_index ||
	    (parser->tokens[token_index].parser_token != FUNCTION_T &&
	     parser->tokens[token_index].parser_token != PROCEDURE_T)) {
		return 0;
	}

	token_index++;
	if (token_index > last_token_index || !token_can_continue_object_name(&parser->tokens[token_index])) {
		return 0;
	}

	last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
	token_index = last_name_token + 1;

	while (token_index <= last_token_index) {
		if (!validate_alter_routine_characteristic_syntax(parser,
		                                                  token_index,
		                                                  last_token_index,
		                                                  &token_index)) {
			return 0;
		}
	}

	return 1;
}

static int validate_alter_routine_characteristic_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index,
                                                        size_t *next_token_index)
{
	if (token_text_equals(parser, token_index, "COMMENT")) {
		if (token_index + 1 > last_token_index ||
		    parser->tokens[token_index + 1].kind != MYLITE_TOKEN_STRING) {
			return 0;
		}
		*next_token_index = token_index + 2;
		return 1;
	}
	if (token_text_equals(parser, token_index, "LANGUAGE")) {
		if (token_index + 1 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "SQL")) {
			return 0;
		}
		*next_token_index = token_index + 2;
		return 1;
	}
	if (token_text_equals(parser, token_index, "CONTAINS") ||
	    parser->tokens[token_index].parser_token == NO_T) {
		if (token_index + 1 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "SQL")) {
			return 0;
		}
		*next_token_index = token_index + 2;
		return 1;
	}
	if (token_text_equals(parser, token_index, "READS") ||
	    token_text_equals(parser, token_index, "MODIFIES")) {
		if (token_index + 2 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "SQL") ||
		    !token_text_equals(parser, token_index + 2, "DATA")) {
			return 0;
		}
		*next_token_index = token_index + 3;
		return 1;
	}
	if (token_text_equals(parser, token_index, "SQL")) {
		if (token_index + 2 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "SECURITY") ||
		    (!token_text_equals(parser, token_index + 2, "DEFINER") &&
		     !token_text_equals(parser, token_index + 2, "INVOKER"))) {
			return 0;
		}
		*next_token_index = token_index + 3;
		return 1;
	}

	return 0;
}

static int validate_alter_table_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index)
{
	size_t last_name_token;

	if (token_index > last_token_index ||
	    parser->tokens[token_index].parser_token != TABLE_T) {
		return 0;
	}

	token_index++;
	if (token_index > last_token_index || !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}

	last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
	token_index = last_name_token + 1;
	if (token_index > last_token_index) {
		return 1;
	}

	return validate_alter_table_action_list_syntax(parser, token_index, last_token_index);
}

static int validate_alter_table_action_list_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index)
{
	while (token_index <= last_token_index) {
		if (parser->tokens[token_index].parser_token == ',') {
			return 0;
		}
		if (!validate_alter_table_action_syntax(parser,
		                                        token_index,
		                                        last_token_index,
		                                        &token_index)) {
			return 0;
		}
		if (token_index > last_token_index) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	return 1;
}

static int validate_alter_table_action_syntax(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index,
                                              size_t *next_token_index)
{
	size_t boundary_token = find_alter_table_action_boundary(parser, token_index, last_token_index);
	size_t action_last_token;
	int is_valid;

	if (boundary_token <= token_index) {
		return 0;
	}

	action_last_token = boundary_token - 1;
	if (token_is_alter_table_partition_action_start(parser, token_index, action_last_token)) {
		is_valid = validate_alter_table_partition_action_syntax(parser, token_index, action_last_token);
	} else if (token_text_equals(parser, token_index, "ADD")) {
		is_valid = validate_alter_table_add_action_syntax(parser, token_index, action_last_token);
	} else if (token_text_equals(parser, token_index, "DROP")) {
		is_valid = validate_alter_table_drop_action_syntax(parser, token_index, action_last_token);
	} else if (parser->tokens[token_index].parser_token == CHANGE_T ||
	           token_text_equals(parser, token_index, "MODIFY")) {
		is_valid = validate_alter_table_change_or_modify_action_syntax(parser,
		                                                                token_index,
		                                                                action_last_token);
	} else if (parser->tokens[token_index].parser_token == ALTER_T) {
		is_valid = validate_alter_table_alter_action_syntax(parser, token_index, action_last_token);
	} else if (parser->tokens[token_index].parser_token == RENAME_T) {
		is_valid = validate_alter_table_rename_action_syntax(parser, token_index, action_last_token);
	} else if (parser->tokens[token_index].parser_token == ORDER_T) {
		is_valid = validate_alter_table_order_action_syntax(parser, token_index, action_last_token);
	} else if (token_text_equals(parser, token_index, "ALGORITHM") ||
	           parser->tokens[token_index].parser_token == LOCK_T) {
		is_valid = validate_alter_table_algorithm_or_lock_action_syntax(parser,
		                                                                token_index,
		                                                                action_last_token);
	} else if (token_text_equals(parser, token_index, "WITH") ||
	           token_text_equals(parser, token_index, "WITHOUT")) {
		is_valid = validate_alter_table_validation_action_syntax(parser,
		                                                         token_index,
		                                                         action_last_token);
	} else if (token_text_equals(parser, token_index, "ENABLE") ||
	           token_text_equals(parser, token_index, "DISABLE")) {
		is_valid = validate_alter_table_enable_or_disable_action_syntax(parser,
		                                                                token_index,
		                                                                action_last_token);
	} else if (token_text_equals(parser, token_index, "CONVERT")) {
		is_valid = validate_alter_table_convert_action_syntax(parser, token_index, action_last_token);
	} else if (token_text_equals(parser, token_index, "FORCE")) {
		is_valid = token_index == action_last_token;
	} else if (token_is_alter_table_table_option_start(parser, token_index, action_last_token)) {
		is_valid = validate_alter_table_table_option_action_syntax(parser,
		                                                           token_index,
		                                                           action_last_token);
	} else {
		is_valid = 0;
	}

	if (!is_valid) {
		return 0;
	}

	*next_token_index = boundary_token;
	return 1;
}

static int token_starts_alter_table_action(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}
	return token_is_alter_table_partition_action_start(parser, token_index, last_token_index) ||
	       token_text_equals(parser, token_index, "ADD") ||
	       token_text_equals(parser, token_index, "DROP") ||
	       parser->tokens[token_index].parser_token == CHANGE_T ||
	       token_text_equals(parser, token_index, "MODIFY") ||
	       parser->tokens[token_index].parser_token == ALTER_T ||
	       parser->tokens[token_index].parser_token == RENAME_T ||
	       parser->tokens[token_index].parser_token == ORDER_T ||
	       token_text_equals(parser, token_index, "ALGORITHM") ||
	       parser->tokens[token_index].parser_token == LOCK_T ||
	       token_text_equals(parser, token_index, "WITH") ||
	       token_text_equals(parser, token_index, "WITHOUT") ||
	       token_text_equals(parser, token_index, "ENABLE") ||
	       token_text_equals(parser, token_index, "DISABLE") ||
	       token_text_equals(parser, token_index, "CONVERT") ||
	       token_text_equals(parser, token_index, "FORCE") ||
	       token_is_alter_table_table_option_start(parser, token_index, last_token_index);
}

static size_t find_alter_table_action_boundary(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (parser->tokens[token_index].parser_token == ',' &&
		    (token_index + 1 > last_token_index ||
		     parser->tokens[token_index + 1].parser_token == ',' ||
		     token_starts_alter_table_action(parser, token_index + 1, last_token_index))) {
			return token_index;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}
	return token_index;
}

static int validate_alter_table_add_action_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	if (!token_text_equals(parser, token_index, "ADD")) {
		return 0;
	}

	token_index++;
	if (token_index > last_token_index) {
		return 0;
	}
	if (token_is_alter_table_column_keyword(parser, token_index)) {
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	if (parser->tokens[token_index].parser_token == '(') {
		return parser->tokens[token_index].matching_token == last_token_index + 1 &&
		       validate_create_table_definition_group_syntax(parser, token_index);
	}
	if (token_is_alter_table_key_action_head(parser, token_index)) {
		return validate_alter_table_key_action_syntax(parser, token_index, last_token_index);
	}
	if (!token_can_start_alter_table_name(&parser->tokens[token_index])) {
		return 0;
	}

	token_index++;
	return token_index <= last_token_index;
}

static int validate_alter_table_key_action_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	return token_index <= last_token_index &&
	       alter_table_action_has_group(parser, token_index, last_token_index);
}

static int validate_alter_table_drop_action_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index)
{
	if (!token_text_equals(parser, token_index, "DROP")) {
		return 0;
	}

	token_index++;
	if (token_index > last_token_index) {
		return 0;
	}
	if (parser->tokens[token_index].parser_token == PRIMARY_T) {
		return token_index + 1 == last_token_index &&
		       parser->tokens[token_index + 1].parser_token == KEY_T;
	}
	if (token_is_alter_table_column_keyword(parser, token_index)) {
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
		return token_can_start_alter_table_name(&parser->tokens[token_index]);
	}
	if (token_text_equals(parser, token_index, "FOREIGN")) {
		return token_index + 2 <= last_token_index &&
		       parser->tokens[token_index + 1].parser_token == KEY_T &&
		       token_can_start_alter_table_name(&parser->tokens[token_index + 2]);
	}
	if (parser->tokens[token_index].parser_token == INDEX_T ||
	    parser->tokens[token_index].parser_token == KEY_T ||
	    parser->tokens[token_index].parser_token == CHECK_T ||
	    parser->tokens[token_index].parser_token == CONSTRAINT_T) {
		return token_index + 1 <= last_token_index &&
		       token_can_start_alter_table_name(&parser->tokens[token_index + 1]);
	}

	return token_can_start_alter_table_name(&parser->tokens[token_index]);
}

static int validate_alter_table_change_or_modify_action_syntax(const mylite_parser *parser,
                                                               size_t token_index,
                                                               size_t last_token_index)
{
	int is_change = parser->tokens[token_index].parser_token == CHANGE_T;

	if (!is_change && !token_text_equals(parser, token_index, "MODIFY")) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index && token_is_alter_table_column_keyword(parser, token_index)) {
		token_index++;
	}
	if (token_index > last_token_index ||
	    !token_can_start_alter_table_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index++;

	if (is_change) {
		if (token_index > last_token_index ||
		    !token_can_start_alter_table_name(&parser->tokens[token_index])) {
			return 0;
		}
		token_index++;
	}

	return token_index <= last_token_index;
}

static int validate_alter_table_alter_action_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index)
{
	if (parser->tokens[token_index].parser_token != ALTER_T) {
		return 0;
	}

	token_index++;
	if (token_index > last_token_index) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == CHECK_T ||
	    parser->tokens[token_index].parser_token == CONSTRAINT_T) {
		token_index++;
		if (token_index > last_token_index ||
		    !token_can_start_alter_table_name(&parser->tokens[token_index])) {
			return 0;
		}
		token_index++;
		if (token_index <= last_token_index && parser->tokens[token_index].parser_token == NOT_T) {
			token_index++;
		}
		return token_index == last_token_index &&
		       token_text_equals(parser, token_index, "ENFORCED");
	}

	if (parser->tokens[token_index].parser_token == INDEX_T ||
	    parser->tokens[token_index].parser_token == KEY_T) {
		token_index++;
		if (token_index + 1 > last_token_index ||
		    !token_can_start_alter_table_name(&parser->tokens[token_index])) {
			return 0;
		}
		token_index++;
		return token_index == last_token_index &&
		       (token_text_equals(parser, token_index, "VISIBLE") ||
		        token_text_equals(parser, token_index, "INVISIBLE"));
	}

	if (token_is_alter_table_column_keyword(parser, token_index)) {
		token_index++;
	}
	if (token_index > last_token_index ||
	    !token_can_start_alter_table_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index++;

	if (token_index > last_token_index) {
		return 0;
	}
	if (parser->tokens[token_index].parser_token == SET_T) {
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
		if (parser->tokens[token_index].parser_token == DEFAULT_T) {
			return token_index + 1 <= last_token_index;
		}
		return token_index == last_token_index &&
		       (token_text_equals(parser, token_index, "VISIBLE") ||
		        token_text_equals(parser, token_index, "INVISIBLE"));
	}
	if (token_text_equals(parser, token_index, "DROP")) {
		return token_index + 1 == last_token_index &&
		       parser->tokens[token_index + 1].parser_token == DEFAULT_T;
	}

	return 0;
}

static int validate_alter_table_rename_action_syntax(const mylite_parser *parser,
                                                     size_t token_index,
                                                     size_t last_token_index)
{
	size_t last_name_token;

	if (parser->tokens[token_index].parser_token != RENAME_T) {
		return 0;
	}

	token_index++;
	if (token_index > last_token_index) {
		return 0;
	}
	if (token_is_alter_table_column_keyword(parser, token_index) ||
	    parser->tokens[token_index].parser_token == INDEX_T ||
	    parser->tokens[token_index].parser_token == KEY_T) {
		token_index++;
		if (token_index + 2 > last_token_index ||
		    !token_can_start_alter_table_name(&parser->tokens[token_index]) ||
		    parser->tokens[token_index + 1].parser_token != TO_T ||
		    !token_can_start_alter_table_name(&parser->tokens[token_index + 2])) {
			return 0;
		}
		token_index += 3;
		return token_index > last_token_index ||
		       (token_is_alter_table_partition_action_start(parser, token_index, last_token_index) &&
		        validate_alter_table_partition_action_syntax(parser, token_index, last_token_index));
	}

	if (parser->tokens[token_index].parser_token == TO_T ||
	    parser->tokens[token_index].parser_token == AS_T) {
		token_index++;
	}
	if (token_index > last_token_index || !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}

	last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
	if (last_name_token == last_token_index) {
		return 1;
	}

	token_index = last_name_token + 1;
	return token_is_alter_table_partition_action_start(parser, token_index, last_token_index) &&
	       validate_alter_table_partition_action_syntax(parser, token_index, last_token_index);
}

static int validate_alter_table_order_action_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index)
{
	if (token_index + 2 > last_token_index ||
	    parser->tokens[token_index].parser_token != ORDER_T ||
	    parser->tokens[token_index + 1].parser_token != BY_T) {
		return 0;
	}

	return validate_alter_table_nonempty_list_syntax(parser, token_index + 2, last_token_index);
}

static int validate_alter_table_algorithm_or_lock_action_syntax(const mylite_parser *parser,
                                                                size_t token_index,
                                                                size_t last_token_index)
{
	int is_algorithm_option = token_text_equals(parser, token_index, "ALGORITHM");
	int is_lock_option = parser->tokens[token_index].parser_token == LOCK_T;

	if (!is_algorithm_option && !is_lock_option) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
		token_index++;
	}
	if (token_index != last_token_index) {
		return 0;
	}

	if (is_algorithm_option) {
		return token_is_alter_table_algorithm_value(parser, token_index);
	}
	return token_is_drop_index_lock_value(parser, token_index);
}

static int validate_alter_table_validation_action_syntax(const mylite_parser *parser,
                                                         size_t token_index,
                                                         size_t last_token_index)
{
	return token_index + 1 == last_token_index &&
	       (token_text_equals(parser, token_index, "WITH") ||
	        token_text_equals(parser, token_index, "WITHOUT")) &&
	       token_text_equals(parser, token_index + 1, "VALIDATION");
}

static int validate_alter_table_enable_or_disable_action_syntax(const mylite_parser *parser,
                                                                size_t token_index,
                                                                size_t last_token_index)
{
	return token_index + 1 == last_token_index &&
	       (token_text_equals(parser, token_index, "ENABLE") ||
	        token_text_equals(parser, token_index, "DISABLE")) &&
	       token_text_equals(parser, token_index + 1, "KEYS");
}

static int validate_alter_table_convert_action_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index)
{
	if (!token_text_equals(parser, token_index, "CONVERT") ||
	    token_index + 3 > last_token_index ||
	    parser->tokens[token_index + 1].parser_token != TO_T) {
		return 0;
	}

	token_index += 2;
	if (parser->tokens[token_index].parser_token == CHARACTER_T) {
		if (token_index + 1 > last_token_index ||
		    parser->tokens[token_index + 1].parser_token != SET_T) {
			return 0;
		}
		token_index += 2;
	} else if (parser->tokens[token_index].parser_token == CHARSET_T) {
		token_index++;
	} else {
		return 0;
	}

	if (token_index > last_token_index ||
	    !token_can_start_alter_table_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index++;
	if (token_index > last_token_index) {
		return 1;
	}
	if (parser->tokens[token_index].parser_token != COLLATE_T ||
	    token_index + 1 > last_token_index ||
	    !token_can_start_alter_table_name(&parser->tokens[token_index + 1])) {
		return 0;
	}
	return token_index + 1 == last_token_index;
}

static int validate_alter_table_partition_action_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index)
{
	if (token_text_equals(parser, token_index, "REORGANIZE")) {
		return token_index + 1 <= last_token_index &&
		       token_text_equals(parser, token_index + 1, "PARTITION");
	}

	if (token_text_equals(parser, token_index, "ADD") ||
	    token_text_equals(parser, token_index, "DROP") ||
	    parser->tokens[token_index].parser_token == TRUNCATE_T ||
	    parser->tokens[token_index].parser_token == ANALYZE_T ||
	    parser->tokens[token_index].parser_token == CHECK_T ||
	    parser->tokens[token_index].parser_token == OPTIMIZE_T ||
	    parser->tokens[token_index].parser_token == REPAIR_T ||
	    token_text_equals(parser, token_index, "REBUILD") ||
	    token_text_equals(parser, token_index, "COALESCE")) {
		return token_index + 2 <= last_token_index &&
		       token_text_equals(parser, token_index + 1, "PARTITION");
	}

	if (token_text_equals(parser, token_index, "DISCARD") ||
	    parser->tokens[token_index].parser_token == IMPORT_T) {
		if (token_index + 1 == last_token_index &&
		    token_text_equals(parser, token_index + 1, "TABLESPACE")) {
			return 1;
		}
		return token_index + 3 <= last_token_index &&
		       token_text_equals(parser, token_index + 1, "PARTITION") &&
		       token_text_equals(parser, last_token_index, "TABLESPACE");
	}

	if (token_text_equals(parser, token_index, "EXCHANGE")) {
		return token_index + 5 <= last_token_index &&
		       token_text_equals(parser, token_index + 1, "PARTITION") &&
		       token_can_start_alter_table_name(&parser->tokens[token_index + 2]) &&
		       token_text_equals(parser, token_index + 3, "WITH") &&
		       parser->tokens[token_index + 4].parser_token == TABLE_T &&
		       token_can_start_object_name(&parser->tokens[token_index + 5]);
	}

	if (token_text_equals(parser, token_index, "REMOVE")) {
		return token_index + 1 == last_token_index &&
		       token_text_equals(parser, token_index + 1, "PARTITIONING");
	}

	if (token_text_equals(parser, token_index, "PARTITION")) {
		return token_index + 2 <= last_token_index &&
		       parser->tokens[token_index + 1].parser_token == BY_T;
	}

	return 0;
}

static int validate_alter_table_table_option_action_syntax(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index)
{
	if (parser->tokens[token_index].parser_token == DEFAULT_T) {
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	if (parser->tokens[token_index].parser_token == CHARACTER_T) {
		token_index++;
		if (token_index <= last_token_index && parser->tokens[token_index].parser_token == SET_T) {
			token_index++;
		}
	} else if ((parser->tokens[token_index].parser_token == DATA_T ||
	            parser->tokens[token_index].parser_token == INDEX_T) &&
	           token_index + 1 <= last_token_index &&
	           token_text_equals(parser, token_index + 1, "DIRECTORY")) {
		token_index += 2;
	} else if (token_text_equals(parser, token_index, "STORAGE")) {
		if (token_index + 1 > last_token_index ||
		    (!token_text_equals(parser, token_index + 1, "DISK") &&
		     !token_text_equals(parser, token_index + 1, "MEMORY"))) {
			return 0;
		}
		return token_index + 1 == last_token_index ||
		       token_is_alter_table_table_option_start(parser, token_index + 2, last_token_index);
	} else {
		token_index++;
	}

	if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
		token_index++;
	}
	return token_index <= last_token_index;
}

static int validate_alter_table_nonempty_list_syntax(const mylite_parser *parser,
                                                     size_t token_index,
                                                     size_t last_token_index)
{
	int expecting_item = 1;

	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (parser->tokens[token_index].parser_token == ',') {
			if (expecting_item) {
				return 0;
			}
			expecting_item = 1;
			token_index++;
			continue;
		}
		expecting_item = 0;
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}

	return !expecting_item;
}

static int alter_table_action_has_group(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (parser->tokens[token_index].parser_token == '(' &&
		    matching_token > token_index + 1 &&
		    matching_token <= last_token_index + 1) {
			return 1;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}
	return 0;
}

static int token_can_start_alter_table_name(const mylite_token *token)
{
	return token->kind == MYLITE_TOKEN_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_QUOTED_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_STRING ||
	       token->kind == MYLITE_TOKEN_NUMBER ||
	       token->kind == MYLITE_TOKEN_KEYWORD;
}

static int token_is_alter_table_column_keyword(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "COLUMN");
}

static int token_is_alter_table_key_action_head(const mylite_parser *parser, size_t token_index)
{
	return parser->tokens[token_index].parser_token == PRIMARY_T ||
	       parser->tokens[token_index].parser_token == UNIQUE_T ||
	       parser->tokens[token_index].parser_token == INDEX_T ||
	       parser->tokens[token_index].parser_token == KEY_T ||
	       parser->tokens[token_index].parser_token == FULL_T ||
	       token_text_equals(parser, token_index, "FULLTEXT") ||
	       parser->tokens[token_index].parser_token == SPATIAL_T ||
	       parser->tokens[token_index].parser_token == CONSTRAINT_T ||
	       token_text_equals(parser, token_index, "FOREIGN") ||
	       parser->tokens[token_index].parser_token == CHECK_T;
}

static int token_is_alter_table_algorithm_value(const mylite_parser *parser, size_t token_index)
{
	return token_is_drop_index_algorithm_value(parser, token_index) ||
	       token_text_equals(parser, token_index, "INSTANT");
}

static int token_is_alter_table_table_option_start(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}
	if (parser->tokens[token_index].parser_token == DEFAULT_T) {
		return token_index + 1 <= last_token_index &&
		       (parser->tokens[token_index + 1].parser_token == CHARACTER_T ||
		        parser->tokens[token_index + 1].parser_token == CHARSET_T ||
		        parser->tokens[token_index + 1].parser_token == COLLATE_T ||
		        token_text_equals(parser, token_index + 1, "ENCRYPTION"));
	}
	if ((parser->tokens[token_index].parser_token == DATA_T ||
	     parser->tokens[token_index].parser_token == INDEX_T) &&
	    token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index + 1, "DIRECTORY")) {
		return 1;
	}
	return parser->tokens[token_index].parser_token == AUTO_INCREMENT_T ||
	       parser->tokens[token_index].parser_token == CHARACTER_T ||
	       parser->tokens[token_index].parser_token == CHARSET_T ||
	       parser->tokens[token_index].parser_token == CHECKSUM_T ||
	       parser->tokens[token_index].parser_token == COLLATE_T ||
	       parser->tokens[token_index].parser_token == ENGINE_T ||
	       parser->tokens[token_index].parser_token == UNION_T ||
	       token_text_equals(parser, token_index, "AVG_ROW_LENGTH") ||
	       token_text_equals(parser, token_index, "COMMENT") ||
	       token_text_equals(parser, token_index, "COMPRESSION") ||
	       token_text_equals(parser, token_index, "CONNECTION") ||
	       token_text_equals(parser, token_index, "DELAY_KEY_WRITE") ||
	       token_text_equals(parser, token_index, "ENCRYPTION") ||
	       token_text_equals(parser, token_index, "ENGINE_ATTRIBUTE") ||
	       token_text_equals(parser, token_index, "INSERT_METHOD") ||
	       token_text_equals(parser, token_index, "KEY_BLOCK_SIZE") ||
	       token_text_equals(parser, token_index, "MAX_ROWS") ||
	       token_text_equals(parser, token_index, "MIN_ROWS") ||
	       token_text_equals(parser, token_index, "PACK_KEYS") ||
	       token_text_equals(parser, token_index, "PASSWORD") ||
	       token_text_equals(parser, token_index, "ROW_FORMAT") ||
	       token_text_equals(parser, token_index, "SECONDARY_ENGINE") ||
	       token_text_equals(parser, token_index, "SECONDARY_ENGINE_ATTRIBUTE") ||
	       token_text_equals(parser, token_index, "STATS_AUTO_RECALC") ||
	       token_text_equals(parser, token_index, "STATS_PERSISTENT") ||
	       token_text_equals(parser, token_index, "STATS_SAMPLE_PAGES") ||
	       token_text_equals(parser, token_index, "STORAGE") ||
	       token_text_equals(parser, token_index, "TABLESPACE");
}

static int token_is_alter_table_partition_action_start(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}
	if (token_text_equals(parser, token_index, "PARTITION")) {
		return token_index + 1 <= last_token_index &&
		       parser->tokens[token_index + 1].parser_token == BY_T;
	}
	if (token_text_equals(parser, token_index, "REMOVE")) {
		return token_index + 1 <= last_token_index &&
		       token_text_equals(parser, token_index + 1, "PARTITIONING");
	}
	if (token_text_equals(parser, token_index, "DISCARD") ||
	    parser->tokens[token_index].parser_token == IMPORT_T) {
		return token_index + 1 <= last_token_index &&
		       (token_text_equals(parser, token_index + 1, "TABLESPACE") ||
		        token_text_equals(parser, token_index + 1, "PARTITION"));
	}
	if (token_text_equals(parser, token_index, "EXCHANGE")) {
		return token_index + 1 <= last_token_index &&
		       token_text_equals(parser, token_index + 1, "PARTITION");
	}
	if (token_text_equals(parser, token_index, "ADD") ||
	    token_text_equals(parser, token_index, "DROP") ||
	    parser->tokens[token_index].parser_token == TRUNCATE_T ||
	    parser->tokens[token_index].parser_token == ANALYZE_T ||
	    parser->tokens[token_index].parser_token == CHECK_T ||
	    parser->tokens[token_index].parser_token == OPTIMIZE_T ||
	    parser->tokens[token_index].parser_token == REPAIR_T ||
	    token_text_equals(parser, token_index, "REBUILD") ||
	    token_text_equals(parser, token_index, "COALESCE") ||
	    token_text_equals(parser, token_index, "REORGANIZE")) {
		return token_index + 1 <= last_token_index &&
		       token_text_equals(parser, token_index + 1, "PARTITION");
	}
	return 0;
}

static int validate_alter_instance_statement_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index)
{
	if (token_index > last_token_index || !token_text_equals(parser, token_index, "INSTANCE")) {
		return 0;
	}

	token_index++;
	if (token_index > last_token_index) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "ENABLE") ||
	    token_text_equals(parser, token_index, "DISABLE")) {
		return token_index + 2 == last_token_index &&
		       token_text_equals(parser, token_index + 1, "INNODB") &&
		       token_text_equals(parser, token_index + 2, "REDO_LOG");
	}

	if (token_text_equals(parser, token_index, "ROTATE")) {
		return token_index + 3 == last_token_index &&
		       (token_text_equals(parser, token_index + 1, "INNODB") ||
		        token_text_equals(parser, token_index + 1, "BINLOG")) &&
		       token_text_equals(parser, token_index + 2, "MASTER") &&
		       parser->tokens[token_index + 3].parser_token == KEY_T;
	}

	if (!token_text_equals(parser, token_index, "RELOAD")) {
		return 0;
	}

	token_index++;
	if (token_index > last_token_index) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "KEYRING")) {
		return token_index == last_token_index;
	}
	if (!token_text_equals(parser, token_index, "TLS")) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "FOR")) {
		if (token_index + 2 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "CHANNEL") ||
		    !token_is_alter_instance_tls_channel(parser, token_index + 2)) {
			return 0;
		}
		token_index += 3;
	}

	if (token_index <= last_token_index && parser->tokens[token_index].parser_token == NO_T) {
		if (token_index + 3 > last_token_index ||
		    parser->tokens[token_index + 1].parser_token != ROLLBACK_T ||
		    parser->tokens[token_index + 2].parser_token != ON_T ||
		    !token_text_equals(parser, token_index + 3, "ERROR")) {
			return 0;
		}
		token_index += 4;
	}

	return token_index > last_token_index;
}

static int token_is_alter_instance_tls_channel(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "mysql_main") ||
	       token_text_equals(parser, token_index, "mysql_admin");
}

static int validate_alter_logfile_group_statement_syntax(const mylite_parser *parser,
                                                         size_t token_index,
                                                         size_t last_token_index)
{
	if (!token_is_logfile_group_sequence(parser, token_index, last_token_index)) {
		return 0;
	}

	token_index += 2;
	if (token_index > last_token_index ||
	    !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index++;

	if (!validate_logfile_group_add_undofile_clause_syntax(parser,
	                                                       token_index,
	                                                       last_token_index,
	                                                       &token_index)) {
		return 0;
	}
	if (token_index <= last_token_index &&
	    token_text_equals(parser, token_index, "INITIAL_SIZE") &&
	    !validate_storage_size_clause_syntax(parser,
	                                         token_index,
	                                         last_token_index,
	                                         "INITIAL_SIZE",
	                                         &token_index)) {
		return 0;
	}
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "WAIT")) {
		token_index++;
	}

	return validate_storage_engine_tail_syntax(parser, token_index, last_token_index);
}

static int validate_alter_tablespace_statement_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index,
                                                      int is_undo_tablespace)
{
	if (is_undo_tablespace) {
		if (!token_is_undo_tablespace_sequence(parser, token_index, last_token_index)) {
			return 0;
		}
		token_index += 2;
	} else {
		if (!token_is_tablespace_token(parser, token_index)) {
			return 0;
		}
		token_index++;
	}

	if (token_index > last_token_index ||
	    !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index++;

	if (is_undo_tablespace) {
		if (!validate_alter_tablespace_set_state_syntax(parser,
		                                                token_index,
		                                                last_token_index,
		                                                &token_index)) {
			return 0;
		}
		return validate_tablespace_optional_storage_tail_syntax(parser, token_index, last_token_index);
	}

	if (token_index > last_token_index) {
		return 0;
	}
	if (token_text_equals(parser, token_index, "ADD") ||
	    token_text_equals(parser, token_index, "DROP")) {
		if (!validate_alter_tablespace_file_action_syntax(parser,
		                                                  token_index,
		                                                  last_token_index,
		                                                  &token_index)) {
			return 0;
		}
		return validate_tablespace_optional_storage_tail_syntax(parser, token_index, last_token_index);
	}
	if (token_text_equals(parser, token_index, "RENAME")) {
		if (!validate_alter_tablespace_rename_syntax(parser,
		                                             token_index,
		                                             last_token_index,
		                                             &token_index)) {
			return 0;
		}
		return validate_tablespace_optional_storage_tail_syntax(parser, token_index, last_token_index);
	}
	if (token_text_equals(parser, token_index, "AUTOEXTEND_SIZE")) {
		if (!validate_storage_size_clause_syntax(parser,
		                                         token_index,
		                                         last_token_index,
		                                         "AUTOEXTEND_SIZE",
		                                         &token_index)) {
			return 0;
		}
		return validate_tablespace_optional_storage_tail_syntax(parser, token_index, last_token_index);
	}
	if (token_text_equals(parser, token_index, "ENCRYPTION")) {
		if (!validate_tablespace_encryption_clause_syntax(parser,
		                                                  token_index,
		                                                  last_token_index,
		                                                  &token_index)) {
			return 0;
		}
		return validate_tablespace_optional_storage_tail_syntax(parser, token_index, last_token_index);
	}
	if (token_text_equals(parser, token_index, "ENGINE_ATTRIBUTE")) {
		return validate_tablespace_engine_attribute_clause_syntax(parser,
		                                                          token_index,
		                                                          last_token_index,
		                                                          &token_index) &&
		       token_index > last_token_index;
	}
	return 0;
}

static int validate_alter_tablespace_file_action_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index,
                                                        size_t *next_token_index)
{
	int is_add = token_text_equals(parser, token_index, "ADD");

	if (!is_add && !token_text_equals(parser, token_index, "DROP")) {
		return 0;
	}
	if (!validate_tablespace_datafile_clause_syntax(parser,
	                                                token_index,
	                                                last_token_index,
	                                                is_add ? "ADD" : "DROP",
	                                                &token_index)) {
		return 0;
	}
	if (is_add &&
	    token_index <= last_token_index &&
	    token_text_equals(parser, token_index, "INITIAL_SIZE")) {
		if (!validate_storage_size_clause_syntax(parser,
		                                         token_index,
		                                         last_token_index,
		                                         "INITIAL_SIZE",
		                                         &token_index)) {
			return 0;
		}
	}
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "WAIT")) {
		token_index++;
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_alter_tablespace_rename_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index,
                                                   size_t *next_token_index)
{
	if (token_index + 2 > last_token_index ||
	    !token_text_equals(parser, token_index, "RENAME") ||
	    parser->tokens[token_index + 1].parser_token != TO_T ||
	    !token_can_start_object_name(&parser->tokens[token_index + 2])) {
		return 0;
	}

	*next_token_index = token_index + 3;
	return 1;
}

static int validate_alter_tablespace_set_state_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index,
                                                      size_t *next_token_index)
{
	if (token_index + 1 > last_token_index ||
	    !token_text_equals(parser, token_index, "SET") ||
	    (!token_text_equals(parser, token_index + 1, "ACTIVE") &&
	     !token_text_equals(parser, token_index + 1, "INACTIVE"))) {
		return 0;
	}

	*next_token_index = token_index + 2;
	return 1;
}

static int validate_alter_server_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	if (!token_text_equals(parser, token_index, "SERVER")) {
		return 0;
	}

	token_index++;
	if (token_index > last_token_index ||
	    !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}

	token_index++;
	return validate_server_options_clause_syntax(parser,
	                                             token_index,
	                                             last_token_index,
	                                             &token_index) &&
	       token_index > last_token_index;
}

static int validate_alter_resource_group_statement_syntax(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index)
{
	int saw_option = 0;

	if (!token_is_resource_group_sequence(parser, token_index, last_token_index)) {
		return 0;
	}

	token_index += 2;
	if (token_index > last_token_index ||
	    !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index++;

	if (token_index <= last_token_index && token_text_equals(parser, token_index, "VCPU")) {
		if (!validate_resource_group_vcpu_clause_syntax(parser,
		                                                token_index,
		                                                last_token_index,
		                                                &token_index)) {
			return 0;
		}
		saw_option = 1;
	}
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "THREAD_PRIORITY")) {
		if (!validate_resource_group_thread_priority_clause_syntax(parser,
		                                                           token_index,
		                                                           last_token_index,
		                                                           &token_index)) {
			return 0;
		}
		saw_option = 1;
	}
	if (token_index <= last_token_index &&
	    (token_text_equals(parser, token_index, "ENABLE") ||
	     token_text_equals(parser, token_index, "DISABLE"))) {
		int is_disable = token_text_equals(parser, token_index, "DISABLE");

		token_index++;
		if (is_disable &&
		    token_index <= last_token_index &&
		    token_text_equals(parser, token_index, "FORCE")) {
			token_index++;
		}
		saw_option = 1;
	}

	return saw_option && token_index > last_token_index;
}

static int validate_alter_user_statement_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index)
{
	int saw_user = 0;

	if (token_index >= parser->token_count ||
	    parser->tokens[token_index].parser_token != USER_T) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "IF")) {
		if (token_index + 1 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "EXISTS")) {
			return 0;
		}
		token_index += 2;
	}

	while (token_index <= last_token_index) {
		if (!validate_alter_user_entry_syntax(parser, token_index, last_token_index, &token_index)) {
			return 0;
		}
		saw_user = 1;

		if (token_index > last_token_index) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token == ',') {
			token_index++;
			if (token_index > last_token_index) {
				return 0;
			}
			continue;
		}
		break;
	}

	return saw_user && validate_alter_user_tail_syntax(parser, token_index, last_token_index);
}

static int validate_alter_user_entry_syntax(const mylite_parser *parser,
                                            size_t token_index,
                                            size_t last_token_index,
                                            size_t *next_token_index)
{
	int is_user_function = 0;

	if (!validate_alter_user_name_syntax(parser,
	                                     token_index,
	                                     last_token_index,
	                                     &token_index,
	                                     &is_user_function)) {
		return 0;
	}

	if (token_index <= last_token_index && token_starts_alter_user_entry_option(parser, token_index)) {
		return validate_alter_user_entry_option_syntax(parser,
		                                               token_index,
		                                               last_token_index,
		                                               next_token_index,
		                                               is_user_function);
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_alter_user_name_syntax(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index,
                                           size_t *next_token_index,
                                           int *is_user_function)
{
	if (token_index + 2 <= last_token_index &&
	    token_text_equals(parser, token_index, "USER") &&
	    token_pair_is_empty_parentheses(parser, token_index + 1)) {
		*next_token_index = token_index + 3;
		*is_user_function = 1;
		return 1;
	}

	*is_user_function = 0;
	return validate_principal_name_syntax(parser,
	                                      token_index,
	                                      last_token_index,
	                                      1,
	                                      next_token_index);
}

static int validate_alter_user_entry_option_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index,
                                                   size_t *next_token_index,
                                                   int is_user_function)
{
	if (token_text_equals(parser, token_index, "IDENTIFIED")) {
		return validate_alter_user_identified_tail_syntax(parser,
		                                                  token_index + 1,
		                                                  last_token_index,
		                                                  next_token_index,
		                                                  is_user_function);
	}

	if (token_text_equals(parser, token_index, "DISCARD")) {
		if (token_index + 2 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "OLD") ||
		    !token_text_equals(parser, token_index + 2, "PASSWORD")) {
			return 0;
		}
		*next_token_index = token_index + 3;
		return 1;
	}

	if (!is_user_function &&
	    (token_text_equals(parser, token_index, "ADD") ||
	     token_text_equals(parser, token_index, "MODIFY") ||
	     token_text_equals(parser, token_index, "DROP"))) {
		return validate_alter_user_factor_operation_syntax(parser,
		                                                   token_index,
		                                                   last_token_index,
		                                                   next_token_index);
	}

	if (token_starts_alter_user_factor(parser, token_index, last_token_index)) {
		return validate_alter_user_registration_option_syntax(parser,
		                                                      token_index,
		                                                      last_token_index,
		                                                      next_token_index);
	}

	return 0;
}

static int validate_alter_user_identified_tail_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index,
                                                      size_t *next_token_index,
                                                      int is_user_function)
{
	if (token_index > last_token_index) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "BY")) {
		token_index++;
		if (!is_user_function &&
		    token_index + 1 <= last_token_index &&
		    token_text_equals(parser, token_index, "RANDOM") &&
		    token_text_equals(parser, token_index + 1, "PASSWORD")) {
			return validate_alter_user_auth_option_tail_syntax(parser,
			                                                   token_index + 2,
			                                                   last_token_index,
			                                                   next_token_index);
		}
		if (token_is_create_user_auth_string(parser, token_index)) {
			return validate_alter_user_auth_option_tail_syntax(parser,
			                                                   token_index + 1,
			                                                   last_token_index,
			                                                   next_token_index);
		}
		return 0;
	}

	if (is_user_function || !token_text_equals(parser, token_index, "WITH")) {
		return 0;
	}

	token_index++;
	if (token_index > last_token_index ||
	    !token_can_be_create_user_auth_plugin(parser, token_index)) {
		return 0;
	}
	token_index++;

	if (token_index > last_token_index) {
		*next_token_index = token_index;
		return 1;
	}
	if (token_text_equals(parser, token_index, "BY")) {
		token_index++;
		if (token_index + 1 <= last_token_index &&
		    token_text_equals(parser, token_index, "RANDOM") &&
		    token_text_equals(parser, token_index + 1, "PASSWORD")) {
			return validate_alter_user_auth_option_tail_syntax(parser,
			                                                   token_index + 2,
			                                                   last_token_index,
			                                                   next_token_index);
		}
		if (token_is_create_user_auth_string(parser, token_index)) {
			return validate_alter_user_auth_option_tail_syntax(parser,
			                                                   token_index + 1,
			                                                   last_token_index,
			                                                   next_token_index);
		}
		return 0;
	}
	if (token_text_equals(parser, token_index, "AS")) {
		token_index++;
		if (!token_is_create_user_auth_hash(parser, token_index)) {
			return 0;
		}
		*next_token_index = token_index + 1;
		return 1;
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_alter_user_auth_option_tail_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index)
{
	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "REPLACE")) {
		token_index++;
		if (!token_is_create_user_auth_string(parser, token_index)) {
			return 0;
		}
		token_index++;
	}

	if (token_index + 2 <= last_token_index &&
	    token_text_equals(parser, token_index, "RETAIN")) {
		if (!token_text_equals(parser, token_index + 1, "CURRENT") ||
		    !token_text_equals(parser, token_index + 2, "PASSWORD")) {
			return 0;
		}
		token_index += 3;
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_alter_user_factor_operation_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index)
{
	const char *operation;
	int factor_count = 0;

	if (token_text_equals(parser, token_index, "ADD")) {
		operation = "ADD";
	} else if (token_text_equals(parser, token_index, "MODIFY")) {
		operation = "MODIFY";
	} else if (token_text_equals(parser, token_index, "DROP")) {
		operation = "DROP";
	} else {
		return 0;
	}

	while (token_index <= last_token_index && token_text_equals(parser, token_index, operation)) {
		factor_count++;
		if (factor_count > 2) {
			return 0;
		}
		token_index++;
		if (!token_starts_alter_user_factor(parser, token_index, last_token_index)) {
			return 0;
		}
		token_index += 2;
		if (strcmp(operation, "DROP") != 0) {
			if (!validate_alter_user_factor_auth_option_syntax(parser,
			                                                   token_index,
			                                                   last_token_index,
			                                                   &token_index)) {
				return 0;
			}
		}
	}

	*next_token_index = token_index;
	return factor_count > 0;
}

static int validate_alter_user_factor_auth_option_syntax(const mylite_parser *parser,
                                                         size_t token_index,
                                                         size_t last_token_index,
                                                         size_t *next_token_index)
{
	if (token_index > last_token_index || !token_text_equals(parser, token_index, "IDENTIFIED")) {
		return 0;
	}
	return validate_create_user_identified_tail_syntax(parser,
	                                                   token_index + 1,
	                                                   last_token_index,
	                                                   next_token_index,
	                                                   0);
}

static int validate_alter_user_registration_option_syntax(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index,
                                                          size_t *next_token_index)
{
	if (!token_starts_alter_user_factor(parser, token_index, last_token_index)) {
		return 0;
	}
	token_index += 2;

	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "INITIATE") &&
	    token_text_equals(parser, token_index + 1, "REGISTRATION")) {
		*next_token_index = token_index + 2;
		return 1;
	}

	if (token_index <= last_token_index && token_text_equals(parser, token_index, "UNREGISTER")) {
		*next_token_index = token_index + 1;
		return 1;
	}

	if (token_index + 5 <= last_token_index &&
	    token_text_equals(parser, token_index, "FINISH") &&
	    token_text_equals(parser, token_index + 1, "REGISTRATION") &&
	    token_text_equals(parser, token_index + 2, "SET") &&
	    token_text_equals(parser, token_index + 3, "CHALLENGE_RESPONSE") &&
	    token_text_equals(parser, token_index + 4, "AS") &&
	    token_is_create_user_auth_string(parser, token_index + 5)) {
		*next_token_index = token_index + 6;
		return 1;
	}

	return 0;
}

static int validate_alter_user_tail_syntax(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 1;
	}

	if (token_text_equals(parser, token_index, "DEFAULT")) {
		return validate_alter_user_default_role_clause_syntax(parser,
		                                                      token_index,
		                                                      last_token_index,
		                                                      &token_index) &&
		       token_index > last_token_index;
	}

	return validate_create_user_tail_syntax(parser, token_index, last_token_index);
}

static int validate_alter_user_default_role_clause_syntax(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index,
                                                          size_t *next_token_index)
{
	int saw_role = 0;

	if (token_index + 1 > last_token_index ||
	    !token_text_equals(parser, token_index, "DEFAULT") ||
	    parser->tokens[token_index + 1].parser_token != ROLE_T) {
		return 0;
	}

	token_index += 2;
	if (token_index <= last_token_index &&
	    (token_text_equals(parser, token_index, "NONE") ||
	     token_text_equals(parser, token_index, "ALL"))) {
		*next_token_index = token_index + 1;
		return 1;
	}

	while (token_index <= last_token_index) {
		if (!validate_principal_name_syntax(parser, token_index, last_token_index, 0, &token_index)) {
			return 0;
		}
		saw_role = 1;

		if (token_index > last_token_index) {
			break;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	*next_token_index = token_index;
	return saw_role;
}

static int token_starts_alter_user_entry_option(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "IDENTIFIED") ||
	       token_text_equals(parser, token_index, "DISCARD") ||
	       token_text_equals(parser, token_index, "ADD") ||
	       token_text_equals(parser, token_index, "MODIFY") ||
	       token_text_equals(parser, token_index, "DROP") ||
	       parser->tokens[token_index].kind == MYLITE_TOKEN_NUMBER;
}

static int token_starts_alter_user_factor(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index)
{
	return token_index + 1 <= last_token_index &&
	       (token_text_equals(parser, token_index, "2") ||
	        token_text_equals(parser, token_index, "3")) &&
	       token_text_equals(parser, token_index + 1, "FACTOR");
}

static int token_is_resource_group_sequence(const mylite_parser *parser,
                                            size_t token_index,
                                            size_t last_token_index)
{
	return token_index + 1 <= last_token_index &&
	       token_text_equals(parser, token_index, "RESOURCE") &&
	       parser->tokens[token_index + 1].parser_token == GROUP_T;
}

static int validate_resource_group_vcpu_clause_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index,
                                                      size_t *next_token_index)
{
	int saw_spec = 0;

	if (token_index > last_token_index || !token_text_equals(parser, token_index, "VCPU")) {
		return 0;
	}
	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
		token_index++;
	}

	while (token_index <= last_token_index) {
		if (!validate_resource_group_vcpu_spec_syntax(parser,
		                                              token_index,
		                                              last_token_index,
		                                              &token_index)) {
			return 0;
		}
		saw_spec = 1;

		if (token_index > last_token_index ||
		    parser->tokens[token_index].parser_token != ',') {
			break;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	*next_token_index = token_index;
	return saw_spec;
}

static int validate_resource_group_vcpu_spec_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    size_t *next_token_index)
{
	if (token_index > last_token_index ||
	    parser->tokens[token_index].kind != MYLITE_TOKEN_NUMBER) {
		return 0;
	}
	token_index++;

	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "-")) {
		token_index++;
		if (parser->tokens[token_index].kind != MYLITE_TOKEN_NUMBER) {
			return 0;
		}
		token_index++;
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_resource_group_thread_priority_clause_syntax(const mylite_parser *parser,
                                                                 size_t token_index,
                                                                 size_t last_token_index,
                                                                 size_t *next_token_index)
{
	if (token_index > last_token_index || !token_text_equals(parser, token_index, "THREAD_PRIORITY")) {
		return 0;
	}
	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
		token_index++;
	}
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "-")) {
		token_index++;
	}
	if (token_index > last_token_index ||
	    parser->tokens[token_index].kind != MYLITE_TOKEN_NUMBER) {
		return 0;
	}

	*next_token_index = token_index + 1;
	return 1;
}

static int validate_use_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	return token_index == last_token_index &&
	       token_can_continue_object_name(&parser->tokens[token_index]);
}

static int validate_truncate_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;
	size_t last_name_token;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    parser->tokens[token_index].parser_token == TABLE_T) {
		token_index++;
	}
	if (token_index > last_token_index ||
	    token_index >= parser->token_count ||
	    !token_can_continue_object_name(&parser->tokens[token_index])) {
		return 0;
	}

	last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
	return last_name_token == last_token_index;
}

static int validate_rename_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == TABLE_T ||
	    token_text_equals(parser, token_index, "TABLES")) {
		return validate_rename_table_statement_syntax(parser, token_index + 1, last_token_index);
	}
	if (parser->tokens[token_index].parser_token == USER_T) {
		return validate_rename_user_statement_syntax(parser, token_index + 1, last_token_index);
	}
	return 1;
}

static int validate_rename_table_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		if (!validate_rename_table_pair_syntax(parser, token_index, last_token_index, &token_index)) {
			return 0;
		}
		if (token_index > last_token_index) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}
	return 1;
}

static int validate_rename_table_pair_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index,
                                             size_t *next_token_index)
{
	size_t last_name_token;

	if (token_index > last_token_index ||
	    token_index >= parser->token_count ||
	    !token_can_continue_object_name(&parser->tokens[token_index])) {
		return 0;
	}

	last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
	token_index = last_name_token + 1;
	if (token_index + 1 > last_token_index ||
	    !token_text_equals(parser, token_index, "TO") ||
	    !token_can_continue_object_name(&parser->tokens[token_index + 1])) {
		return 0;
	}

	last_name_token = last_qualified_name_token(parser, token_index + 1, last_token_index);
	*next_token_index = last_name_token + 1;
	return 1;
}

static int validate_rename_user_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		if (!validate_rename_user_pair_syntax(parser, token_index, last_token_index, &token_index)) {
			return 0;
		}
		if (token_index > last_token_index) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}
	return 1;
}

static int validate_rename_user_pair_syntax(const mylite_parser *parser,
                                            size_t token_index,
                                            size_t last_token_index,
                                            size_t *next_token_index)
{
	if (!validate_principal_name_syntax(parser, token_index, last_token_index, 1, &token_index)) {
		return 0;
	}
	if (token_index + 1 > last_token_index ||
	    !token_text_equals(parser, token_index, "TO")) {
		return 0;
	}
	token_index++;

	return validate_principal_name_syntax(parser,
	                                      token_index,
	                                      last_token_index,
	                                      1,
	                                      next_token_index);
}

static int validate_call_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;
	size_t last_name_token;
	size_t next_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index ||
	    token_index >= parser->token_count ||
	    !token_can_continue_object_name(&parser->tokens[token_index])) {
		return 0;
	}

	last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
	next_token_index = last_name_token + 1;
	if (next_token_index > last_token_index) {
		return 1;
	}
	if (next_token_index < parser->token_count &&
	    parser->tokens[next_token_index].parser_token == '(' &&
	    parser->tokens[next_token_index].matching_token == statement->last_token) {
		return validate_call_argument_list_syntax(parser, next_token_index);
	}
	return 0;
}

static int validate_call_argument_list_syntax(const mylite_parser *parser, size_t open_token_index)
{
	size_t close_token_index;
	size_t token_index;
	int expecting_argument = 1;

	if (open_token_index >= parser->token_count ||
	    parser->tokens[open_token_index].matching_token <= open_token_index + 1) {
		return 0;
	}

	close_token_index = parser->tokens[open_token_index].matching_token - 1;
	if (open_token_index + 1 == close_token_index) {
		return 1;
	}

	for (token_index = open_token_index + 1; token_index < close_token_index; token_index++) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (matching_token > token_index + 1) {
			expecting_argument = 0;
			token_index = matching_token - 1;
			continue;
		}
		if (parser->tokens[token_index].parser_token == ',') {
			if (expecting_argument) {
				return 0;
			}
			expecting_argument = 1;
			continue;
		}
		expecting_argument = 0;
	}
	return !expecting_argument;
}

static int validate_do_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	return validate_expression_list_syntax(parser, token_index, last_token_index);
}

static int validate_expression_list_syntax(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index)
{
	int expecting_expression = 1;

	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (matching_token > token_index + 1) {
			expecting_expression = 0;
			token_index = matching_token;
			continue;
		}
		if (parser->tokens[token_index].parser_token == ',') {
			if (expecting_expression) {
				return 0;
			}
			expecting_expression = 1;
			token_index++;
			continue;
		}
		expecting_expression = 0;
		token_index++;
	}
	return !expecting_expression;
}

static int validate_values_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_values_statement_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}
	if (values_statement_is_explained_query(parser, statement, token_index)) {
		return 1;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (!validate_values_row_constructor_list_syntax(parser,
	                                                 token_index,
	                                                 last_token_index,
	                                                 &token_index)) {
		return 0;
	}

	while (token_index <= last_token_index &&
	       token_index < parser->token_count &&
	       parser->tokens[token_index].parser_token == ')') {
		token_index++;
	}
	return validate_values_tail_syntax(parser, token_index, last_token_index);
}

static int validate_values_row_constructor_list_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		size_t open_token_index;
		size_t after_close_token_index;

		if (token_index + 2 > last_token_index ||
		    !token_text_equals(parser, token_index, "ROW") ||
		    parser->tokens[token_index + 1].parser_token != '(') {
			return 0;
		}

		open_token_index = token_index + 1;
		after_close_token_index = parser->tokens[open_token_index].matching_token;
		if (after_close_token_index <= open_token_index + 1 ||
		    after_close_token_index - 1 > last_token_index ||
		    !validate_expression_list_syntax(parser, open_token_index + 1, after_close_token_index - 2)) {
			return 0;
		}

		token_index = after_close_token_index;
		if (token_index > last_token_index ||
		    token_is_values_tail_boundary(parser, token_index) ||
		    parser->tokens[token_index].parser_token == ')') {
			break;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index || token_is_values_tail_boundary(parser, token_index)) {
			return 0;
		}
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_values_tail_syntax(const mylite_parser *parser,
                                       size_t token_index,
                                       size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 1;
	}
	if (token_is_values_tail_boundary(parser, token_index) &&
	    !token_text_equals(parser, token_index, "ORDER") &&
	    !token_text_equals(parser, token_index, "LIMIT")) {
		return validate_nonempty_expression_tail_syntax(parser, token_index + 1, last_token_index);
	}
	if (token_text_equals(parser, token_index, "ORDER")) {
		if (!validate_values_order_by_tail_syntax(parser, token_index, last_token_index, &token_index)) {
			return 0;
		}
		if (token_index > last_token_index) {
			return 1;
		}
	}
	if (token_text_equals(parser, token_index, "LIMIT")) {
		return token_index + 1 == last_token_index &&
		       parser->tokens[token_index + 1].kind == MYLITE_TOKEN_NUMBER;
	}
	return 0;
}

static int validate_values_order_by_tail_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index,
                                                size_t *next_token_index)
{
	size_t order_expression_token_index;

	if (token_index + 2 > last_token_index ||
	    !token_text_equals(parser, token_index, "ORDER") ||
	    !token_text_equals(parser, token_index + 1, "BY")) {
		return 0;
	}

	order_expression_token_index = token_index + 2;
	token_index = order_expression_token_index;
	while (token_index <= last_token_index &&
	       !token_text_equals(parser, token_index, "LIMIT")) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (matching_token > token_index + 1) {
			token_index = matching_token;
			continue;
		}
		token_index++;
	}
	if (token_index == order_expression_token_index ||
	    parser->tokens[token_index - 1].parser_token == ',') {
		return 0;
	}
	*next_token_index = token_index;
	return 1;
}

static size_t find_values_statement_token(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index;
	size_t last_token_index;

	if (statement->first_token == 0 || statement->last_token < statement->first_token) {
		return parser->token_count;
	}

	token_index = statement->first_token - 1;
	last_token_index = statement->last_token - 1;
	while (token_index <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == VALUES_T) {
			return token_index;
		}
		token_index++;
	}
	return parser->token_count;
}

static int values_statement_is_explained_query(const mylite_parser *parser,
                                               const mylite_statement *statement,
                                               size_t values_token_index)
{
	size_t token_index;

	if (statement->first_token == 0) {
		return 0;
	}

	token_index = statement->first_token - 1;
	while (token_index < values_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == EXPLAIN_T) {
			return 1;
		}
		token_index++;
	}
	return 0;
}

static int token_is_values_tail_boundary(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (token_text_equals(parser, token_index, "ORDER") ||
	        token_text_equals(parser, token_index, "LIMIT") ||
	        token_text_equals(parser, token_index, "UNION") ||
	        token_text_equals(parser, token_index, "INTERSECT") ||
	        token_text_equals(parser, token_index, "EXCEPT"));
}

static int validate_insert_or_replace_statement_syntax(const mylite_parser *parser,
                                                       const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;
	int is_insert = statement->kind == MYLITE_STATEMENT_INSERT;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (!skip_insert_or_replace_modifiers(parser,
	                                      token_index,
	                                      last_token_index,
	                                      is_insert,
	                                      &token_index)) {
		return 0;
	}
	if (token_index <= last_token_index && parser->tokens[token_index].parser_token == INTO_T) {
		token_index++;
	}
	if (token_index > last_token_index || !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}

	token_index = last_qualified_name_token(parser, token_index, last_token_index) + 1;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "PARTITION")) {
		if (token_index + 1 > last_token_index ||
		    !validate_dml_name_list_group_syntax(parser, token_index + 1, 0)) {
			return 0;
		}
		token_index = parser->tokens[token_index + 1].matching_token;
	}
	if (token_index > last_token_index) {
		return 0;
	}
	if (parser->tokens[token_index].parser_token == '(' && group_starts_query_expression(parser, token_index)) {
		return validate_insert_or_replace_query_source_syntax(parser, token_index, last_token_index);
	}
	if (parser->tokens[token_index].parser_token == '(') {
		if (!validate_dml_name_list_group_syntax(parser, token_index, 1)) {
			return 0;
		}
		token_index = parser->tokens[token_index].matching_token;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	return validate_insert_or_replace_source_syntax(parser, token_index, last_token_index, is_insert);
}

static int skip_insert_or_replace_modifiers(const mylite_parser *parser,
                                            size_t token_index,
                                            size_t last_token_index,
                                            int is_insert,
                                            size_t *next_token_index)
{
	int seen_priority = 0;
	int seen_ignore = 0;

	while (token_index <= last_token_index && token_index < parser->token_count) {
		if (token_is_insert_priority_modifier(parser, token_index, is_insert)) {
			if (seen_priority || seen_ignore) {
				return 0;
			}
			seen_priority = 1;
			token_index++;
			continue;
		}
		if (is_insert && parser->tokens[token_index].parser_token == IGNORE_T) {
			if (seen_ignore) {
				return 0;
			}
			seen_ignore = 1;
			token_index++;
			continue;
		}
		break;
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_insert_or_replace_source_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    int is_insert)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}
	if (parser->tokens[token_index].parser_token == VALUES_T ||
	    parser->tokens[token_index].parser_token == VALUE_T) {
		return validate_insert_or_replace_values_syntax(parser, token_index, last_token_index, is_insert);
	}
	if (parser->tokens[token_index].parser_token == SET_T) {
		return validate_insert_or_replace_set_syntax(parser, token_index, last_token_index, is_insert);
	}
	if (token_starts_insert_or_replace_query_source(parser, token_index)) {
		return validate_insert_or_replace_query_source_syntax(parser, token_index, last_token_index);
	}
	return 0;
}

static int validate_insert_or_replace_values_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    int is_insert)
{
	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "ROW")) {
		if (!validate_insert_row_constructor_list_syntax(parser,
		                                                 token_index,
		                                                 last_token_index,
		                                                 &token_index)) {
			return 0;
		}
	} else if (!validate_insert_parenthesized_value_list_syntax(parser,
	                                                            token_index,
	                                                            last_token_index,
	                                                            &token_index)) {
		return 0;
	}

	return validate_insert_or_replace_tail_syntax(parser, token_index, last_token_index, is_insert);
}

static int validate_insert_parenthesized_value_list_syntax(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index,
                                                           size_t *next_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		size_t after_close_token_index;
		size_t close_token_index;

		if (parser->tokens[token_index].parser_token != '(') {
			return 0;
		}

		after_close_token_index = parser->tokens[token_index].matching_token;
		if (after_close_token_index == 0 ||
		    after_close_token_index <= token_index + 1 ||
		    after_close_token_index - 1 > last_token_index) {
			return 0;
		}

		close_token_index = after_close_token_index - 1;
		if (close_token_index > token_index + 1 &&
		    !validate_expression_list_syntax(parser, token_index + 1, close_token_index - 1)) {
			return 0;
		}

		token_index = after_close_token_index;
		if (token_index > last_token_index || parser->tokens[token_index].parser_token != ',') {
			break;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_insert_row_constructor_list_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		size_t open_token_index;
		size_t after_close_token_index;

		if (!token_text_equals(parser, token_index, "ROW") ||
		    token_index + 1 > last_token_index ||
		    parser->tokens[token_index + 1].parser_token != '(') {
			return 0;
		}

		open_token_index = token_index + 1;
		after_close_token_index = parser->tokens[open_token_index].matching_token;
		if (after_close_token_index == 0 ||
		    after_close_token_index <= open_token_index + 1 ||
		    after_close_token_index - 1 > last_token_index) {
			return 0;
		}
		if (after_close_token_index > open_token_index + 2 &&
		    !validate_expression_list_syntax(parser, open_token_index + 1, after_close_token_index - 2)) {
			return 0;
		}

		token_index = after_close_token_index;
		if (token_index > last_token_index || parser->tokens[token_index].parser_token != ',') {
			break;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_insert_or_replace_set_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 int is_insert)
{
	size_t assignment_first_token;
	size_t assignment_tail_token;
	size_t assignment_last_token;

	if (token_index > last_token_index || parser->tokens[token_index].parser_token != SET_T) {
		return 0;
	}

	assignment_first_token = token_index + 1;
	assignment_tail_token = find_insert_assignment_tail_token(parser,
	                                                          assignment_first_token,
	                                                          last_token_index,
	                                                          is_insert);
	assignment_last_token = assignment_tail_token < parser->token_count ?
		assignment_tail_token - 1 :
		last_token_index;
	if (!validate_dml_assignment_list_syntax(parser, assignment_first_token, assignment_last_token)) {
		return 0;
	}
	return validate_insert_or_replace_tail_syntax(parser, assignment_tail_token, last_token_index, is_insert);
}

static int validate_insert_or_replace_query_source_syntax(const mylite_parser *parser,
                                                          size_t token_index,
                                                          size_t last_token_index)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}
	if (parser->tokens[token_index].parser_token == TABLE_T && token_index + 1 > last_token_index) {
		return 0;
	}
	if (parser->tokens[token_index].parser_token == '(' &&
	    (!group_starts_query_expression(parser, token_index) ||
	     parser->tokens[token_index].matching_token <= token_index + 1 ||
	     parser->tokens[token_index].matching_token - 1 > last_token_index)) {
		return 0;
	}
	return validate_nonempty_expression_tail_syntax(parser, token_index, last_token_index);
}

static int validate_insert_or_replace_tail_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index,
                                                  int is_insert)
{
	if (token_index > last_token_index) {
		return 1;
	}
	if (!is_insert) {
		return 0;
	}
	if (token_text_equals(parser, token_index, "AS")) {
		if (!validate_insert_row_alias_clause_syntax(parser,
		                                             token_index,
		                                             last_token_index,
		                                             &token_index)) {
			return 0;
		}
		if (token_index > last_token_index) {
			return 1;
		}
	}
	if (parser->tokens[token_index].parser_token == ON_T) {
		return validate_insert_on_duplicate_key_update_tail_syntax(parser,
		                                                           token_index,
		                                                           last_token_index);
	}
	return 0;
}

static int validate_insert_row_alias_clause_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index,
                                                   size_t *next_token_index)
{
	if (token_index + 1 > last_token_index ||
	    !token_text_equals(parser, token_index, "AS") ||
	    !token_can_continue_object_name(&parser->tokens[token_index + 1])) {
		return 0;
	}

	token_index += 2;
	if (token_index <= last_token_index && parser->tokens[token_index].parser_token == '(') {
		if (!validate_dml_name_list_group_syntax(parser, token_index, 0)) {
			return 0;
		}
		token_index = parser->tokens[token_index].matching_token;
	}
	*next_token_index = token_index;
	return 1;
}

static int validate_insert_on_duplicate_key_update_tail_syntax(const mylite_parser *parser,
                                                               size_t token_index,
                                                               size_t last_token_index)
{
	if (token_index + 4 > last_token_index ||
	    parser->tokens[token_index].parser_token != ON_T ||
	    !token_text_equals(parser, token_index + 1, "DUPLICATE") ||
	    parser->tokens[token_index + 2].parser_token != KEY_T ||
	    parser->tokens[token_index + 3].parser_token != UPDATE_T) {
		return 0;
	}
	return validate_dml_assignment_list_syntax(parser, token_index + 4, last_token_index);
}

static int validate_dml_assignment_list_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		size_t last_name_token;
		int has_expression = 0;

		if (!token_can_continue_object_name(&parser->tokens[token_index])) {
			return 0;
		}

		last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
		if (last_name_token + 1 > last_token_index ||
		    !token_is_assignment_operator(parser, last_name_token + 1)) {
			return 0;
		}

		token_index = last_name_token + 2;
		if (token_index > last_token_index || parser->tokens[token_index].parser_token == ',') {
			return 0;
		}

		while (token_index <= last_token_index) {
			size_t matching_token = parser->tokens[token_index].matching_token;

			if (matching_token > token_index + 1) {
				has_expression = 1;
				token_index = matching_token;
				continue;
			}
			if (parser->tokens[token_index].parser_token == ',') {
				break;
			}
			has_expression = 1;
			token_index++;
		}
		if (!has_expression) {
			return 0;
		}
		if (token_index > last_token_index) {
			return 1;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}
	return 1;
}

static int validate_dml_name_list_group_syntax(const mylite_parser *parser,
                                               size_t open_token_index,
                                               int allow_empty)
{
	size_t token_index;
	size_t close_token_index;
	int expecting_name = 1;

	if (open_token_index >= parser->token_count ||
	    parser->tokens[open_token_index].parser_token != '(' ||
	    parser->tokens[open_token_index].matching_token == 0 ||
	    parser->tokens[open_token_index].matching_token <= open_token_index + 1) {
		return 0;
	}

	close_token_index = parser->tokens[open_token_index].matching_token - 1;
	if (close_token_index == open_token_index + 1) {
		return allow_empty;
	}

	for (token_index = open_token_index + 1; token_index < close_token_index; token_index++) {
		if (expecting_name) {
			if (!token_can_continue_object_name(&parser->tokens[token_index])) {
				return 0;
			}
			expecting_name = 0;
			continue;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		expecting_name = 1;
	}

	return !expecting_name;
}

static size_t find_insert_assignment_tail_token(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index,
                                                int is_insert)
{
	while (is_insert && token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (token_text_equals(parser, token_index, "AS") ||
		    parser->tokens[token_index].parser_token == ON_T) {
			return token_index;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}
	return parser->token_count;
}

static int token_starts_insert_or_replace_query_source(const mylite_parser *parser, size_t token_index)
{
	if (token_index >= parser->token_count) {
		return 0;
	}
	return parser->tokens[token_index].parser_token == SELECT_T ||
	       parser->tokens[token_index].parser_token == WITH_T ||
	       parser->tokens[token_index].parser_token == TABLE_T ||
	       (parser->tokens[token_index].parser_token == '(' &&
	        group_starts_query_expression(parser, token_index));
}

static int token_is_insert_priority_modifier(const mylite_parser *parser,
                                             size_t token_index,
                                             int is_insert)
{
	if (token_index >= parser->token_count) {
		return 0;
	}
	return parser->tokens[token_index].parser_token == LOW_PRIORITY_T ||
	       parser->tokens[token_index].parser_token == DELAYED_T ||
	       (is_insert && parser->tokens[token_index].parser_token == HIGH_PRIORITY_T);
}

static int validate_update_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;
	size_t set_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (!skip_update_modifiers(parser, token_index, last_token_index, &token_index)) {
		return 0;
	}

	set_token_index = find_update_set_token(parser, token_index, last_token_index);
	if (set_token_index >= parser->token_count || set_token_index == token_index) {
		return 0;
	}
	if (!validate_update_table_reference_span_syntax(parser, token_index, set_token_index - 1)) {
		return 0;
	}
	return validate_update_assignment_and_tail_syntax(parser, set_token_index + 1, last_token_index);
}

static int skip_update_modifiers(const mylite_parser *parser,
                                 size_t token_index,
                                 size_t last_token_index,
                                 size_t *next_token_index)
{
	int seen_low_priority = 0;
	int seen_ignore = 0;

	while (token_index <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == LOW_PRIORITY_T) {
			if (seen_low_priority || seen_ignore) {
				return 0;
			}
			seen_low_priority = 1;
			token_index++;
			continue;
		}
		if (parser->tokens[token_index].parser_token == IGNORE_T) {
			if (seen_ignore) {
				return 0;
			}
			seen_ignore = 1;
			token_index++;
			continue;
		}
		break;
	}

	*next_token_index = token_index;
	return 1;
}

static size_t find_update_set_token(const mylite_parser *parser,
                                    size_t token_index,
                                    size_t last_token_index)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (parser->tokens[token_index].parser_token == SET_T) {
			return token_index;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}
	return parser->token_count;
}

static int validate_update_table_reference_span_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index)
{
	int expecting_reference = 1;
	int saw_reference = 0;

	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (matching_token > token_index + 1) {
			saw_reference = 1;
			expecting_reference = 0;
			token_index = matching_token;
			continue;
		}
		if (parser->tokens[token_index].parser_token == ',') {
			if (expecting_reference) {
				return 0;
			}
			expecting_reference = 1;
			token_index++;
			continue;
		}
		saw_reference = 1;
		expecting_reference = 0;
		token_index++;
	}

	return saw_reference && !expecting_reference;
}

static int validate_update_assignment_and_tail_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index)
{
	size_t tail_token_index = find_update_tail_token(parser, token_index, last_token_index);
	size_t assignment_last_token = tail_token_index < parser->token_count ?
		tail_token_index - 1 :
		last_token_index;

	if (!validate_dml_assignment_list_syntax(parser, token_index, assignment_last_token)) {
		return 0;
	}
	return validate_update_tail_syntax(parser, tail_token_index, last_token_index);
}

static size_t find_update_tail_token(const mylite_parser *parser,
                                     size_t token_index,
                                     size_t last_token_index)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (token_starts_update_tail(parser, token_index)) {
			return token_index;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}
	return parser->token_count;
}

static int validate_update_tail_syntax(const mylite_parser *parser,
                                       size_t token_index,
                                       size_t last_token_index)
{
	int seen_where = 0;
	int seen_order = 0;
	int seen_limit = 0;

	if (token_index > last_token_index) {
		return 1;
	}

	while (token_index <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == WHERE_T) {
			if (seen_where || seen_order || seen_limit ||
			    !validate_update_where_tail_syntax(parser, token_index, last_token_index, &token_index)) {
				return 0;
			}
			seen_where = 1;
			continue;
		}
		if (token_text_equals(parser, token_index, "ORDER")) {
			if (seen_order || seen_limit ||
			    !validate_values_order_by_tail_syntax(parser, token_index, last_token_index, &token_index)) {
				return 0;
			}
			seen_order = 1;
			continue;
		}
		if (token_text_equals(parser, token_index, "LIMIT")) {
			if (seen_limit) {
				return 0;
			}
			seen_limit = 1;
			return validate_update_limit_tail_syntax(parser, token_index, last_token_index);
		}
		return 0;
	}

	return 1;
}

static int validate_update_where_tail_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index,
                                             size_t *next_token_index)
{
	size_t expression_first_token;
	size_t expression_last_token;

	if (token_index > last_token_index || parser->tokens[token_index].parser_token != WHERE_T) {
		return 0;
	}

	expression_first_token = token_index + 1;
	token_index = expression_first_token;
	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (token_text_equals(parser, token_index, "ORDER") ||
		    token_text_equals(parser, token_index, "LIMIT")) {
			break;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}

	if (token_index == expression_first_token) {
		return 0;
	}
	expression_last_token = token_index - 1;
	if (!validate_nonempty_expression_tail_syntax(parser, expression_first_token, expression_last_token)) {
		return 0;
	}
	*next_token_index = token_index;
	return 1;
}

static int validate_update_limit_tail_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index)
{
	size_t expression_token_index;

	if (token_index + 1 > last_token_index || !token_text_equals(parser, token_index, "LIMIT")) {
		return 0;
	}

	for (expression_token_index = token_index + 1;
	     expression_token_index <= last_token_index && expression_token_index < parser->token_count;
	     expression_token_index++) {
		size_t matching_token = parser->tokens[expression_token_index].matching_token;

		if (matching_token > expression_token_index + 1) {
			expression_token_index = matching_token - 1;
			continue;
		}
		if (parser->tokens[expression_token_index].parser_token == ',') {
			return 0;
		}
	}
	return validate_nonempty_expression_tail_syntax(parser, token_index + 1, last_token_index);
}

static int token_starts_update_tail(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (parser->tokens[token_index].parser_token == WHERE_T ||
	        token_text_equals(parser, token_index, "ORDER") ||
	        token_text_equals(parser, token_index, "LIMIT"));
}

static int validate_delete_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (!skip_delete_modifiers(parser, token_index, last_token_index, &token_index)) {
		return 0;
	}
	if (token_index > last_token_index) {
		return 0;
	}
	if (parser->tokens[token_index].parser_token == FROM_T) {
		return validate_delete_from_statement_syntax(parser, token_index, last_token_index);
	}
	return validate_delete_multi_from_statement_syntax(parser, token_index, last_token_index);
}

static int skip_delete_modifiers(const mylite_parser *parser,
                                 size_t token_index,
                                 size_t last_token_index,
                                 size_t *next_token_index)
{
	int seen_low_priority = 0;
	int seen_quick = 0;
	int seen_ignore = 0;

	while (token_index <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == LOW_PRIORITY_T) {
			if (seen_low_priority || seen_quick || seen_ignore) {
				return 0;
			}
			seen_low_priority = 1;
			token_index++;
			continue;
		}
		if (parser->tokens[token_index].parser_token == QUICK_T) {
			if (seen_quick || seen_ignore) {
				return 0;
			}
			seen_quick = 1;
			token_index++;
			continue;
		}
		if (parser->tokens[token_index].parser_token == IGNORE_T) {
			if (seen_ignore) {
				return 0;
			}
			seen_ignore = 1;
			token_index++;
			continue;
		}
		break;
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_delete_from_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index)
{
	size_t target_first_token = token_index + 1;
	size_t using_token_index;

	if (target_first_token > last_token_index) {
		return 0;
	}

	using_token_index = find_delete_clause_token(parser, target_first_token, last_token_index, USING_T);
	if (using_token_index < parser->token_count) {
		size_t tail_token_index;
		size_t references_last_token;

		if (using_token_index == target_first_token ||
		    !validate_delete_target_list_syntax(parser, target_first_token, using_token_index - 1)) {
			return 0;
		}

		tail_token_index = find_delete_tail_token(parser, using_token_index + 1, last_token_index);
		references_last_token = tail_token_index < parser->token_count ?
			tail_token_index - 1 :
			last_token_index;
		if (!validate_delete_table_reference_span_syntax(parser, using_token_index + 1, references_last_token)) {
			return 0;
		}
		return validate_delete_multi_table_tail_syntax(parser, tail_token_index, last_token_index);
	}

	return validate_delete_single_table_statement_syntax(parser, target_first_token, last_token_index);
}

static int validate_delete_multi_from_statement_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index)
{
	size_t from_token_index = find_delete_clause_token(parser, token_index, last_token_index, FROM_T);
	size_t tail_token_index;
	size_t references_last_token;

	if (from_token_index >= parser->token_count ||
	    from_token_index == token_index ||
	    !validate_delete_target_list_syntax(parser, token_index, from_token_index - 1)) {
		return 0;
	}

	tail_token_index = find_delete_tail_token(parser, from_token_index + 1, last_token_index);
	references_last_token = tail_token_index < parser->token_count ?
		tail_token_index - 1 :
		last_token_index;
	if (!validate_delete_table_reference_span_syntax(parser, from_token_index + 1, references_last_token)) {
		return 0;
	}
	return validate_delete_multi_table_tail_syntax(parser, tail_token_index, last_token_index);
}

static int validate_delete_single_table_statement_syntax(const mylite_parser *parser,
                                                         size_t token_index,
                                                         size_t last_token_index)
{
	int seen_alias = 0;
	int seen_partition = 0;

	if (token_index > last_token_index || !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}

	token_index = last_qualified_name_token(parser, token_index, last_token_index) + 1;
	while (token_index <= last_token_index && token_index < parser->token_count) {
		if (!seen_partition && token_text_equals(parser, token_index, "PARTITION")) {
			if (token_index + 1 > last_token_index ||
			    !validate_dml_name_list_group_syntax(parser, token_index + 1, 0)) {
				return 0;
			}
			token_index = parser->tokens[token_index + 1].matching_token;
			seen_partition = 1;
			continue;
		}
		if (!seen_alias && parser->tokens[token_index].parser_token == AS_T) {
			if (token_index + 1 > last_token_index ||
			    !token_can_continue_object_name(&parser->tokens[token_index + 1])) {
				return 0;
			}
			token_index += 2;
			seen_alias = 1;
			continue;
		}
		if (!seen_alias &&
		    !token_starts_delete_tail(parser, token_index) &&
		    token_can_start_object_name(&parser->tokens[token_index])) {
			token_index++;
			seen_alias = 1;
			continue;
		}
		break;
	}

	return validate_delete_single_table_tail_syntax(parser, token_index, last_token_index);
}

static int validate_delete_target_list_syntax(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		if (!validate_delete_target_syntax(parser, token_index, last_token_index, &token_index)) {
			return 0;
		}
		if (token_index > last_token_index) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}
	return 1;
}

static int validate_delete_target_syntax(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index,
                                         size_t *next_token_index)
{
	if (token_index > last_token_index || !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}

	token_index = last_qualified_name_token(parser, token_index, last_token_index) + 1;
	if (token_index + 1 <= last_token_index &&
	    parser->tokens[token_index].parser_token == '.' &&
	    token_text_equals(parser, token_index + 1, "*")) {
		token_index += 2;
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_delete_table_reference_span_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index)
{
	int expecting_reference = 1;
	int saw_reference = 0;

	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (matching_token > token_index + 1) {
			saw_reference = 1;
			expecting_reference = 0;
			token_index = matching_token;
			continue;
		}
		if (parser->tokens[token_index].parser_token == ',') {
			if (expecting_reference) {
				return 0;
			}
			expecting_reference = 1;
			token_index++;
			continue;
		}
		saw_reference = 1;
		expecting_reference = 0;
		token_index++;
	}

	return saw_reference && !expecting_reference;
}

static size_t find_delete_clause_token(const mylite_parser *parser,
                                       size_t token_index,
                                       size_t last_token_index,
                                       int clause_token)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (token_starts_delete_tail(parser, token_index)) {
			return parser->token_count;
		}
		if (parser->tokens[token_index].parser_token == clause_token) {
			return token_index;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}
	return parser->token_count;
}

static size_t find_delete_tail_token(const mylite_parser *parser,
                                     size_t token_index,
                                     size_t last_token_index)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (token_starts_delete_tail(parser, token_index)) {
			return token_index;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}
	return parser->token_count;
}

static int validate_delete_single_table_tail_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index)
{
	return validate_update_tail_syntax(parser, token_index, last_token_index);
}

static int validate_delete_multi_table_tail_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 1;
	}
	if (token_index >= parser->token_count || parser->tokens[token_index].parser_token != WHERE_T) {
		return 0;
	}
	if (!validate_update_where_tail_syntax(parser, token_index, last_token_index, &token_index)) {
		return 0;
	}
	return token_index > last_token_index;
}

static int token_starts_delete_tail(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (parser->tokens[token_index].parser_token == WHERE_T ||
	        token_text_equals(parser, token_index, "ORDER") ||
	        token_text_equals(parser, token_index, "LIMIT"));
}

static int validate_import_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;
	int expecting_file = 1;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	last_token_index = statement->last_token - 1;
	if (token_index + 3 > last_token_index ||
	    parser->tokens[token_index + 1].parser_token != TABLE_T ||
	    parser->tokens[token_index + 2].parser_token != FROM_T) {
		return 0;
	}

	for (token_index += 3; token_index <= last_token_index; token_index++) {
		if (expecting_file) {
			if (parser->tokens[token_index].kind != MYLITE_TOKEN_STRING) {
				return 0;
			}
			expecting_file = 0;
			continue;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		expecting_file = 1;
	}
	return !expecting_file;
}

static int validate_binlog_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	return token_index == last_token_index &&
	       parser->tokens[token_index].kind == MYLITE_TOKEN_STRING;
}

static int validate_install_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "COMPONENT")) {
		return validate_component_uri_list_syntax(parser,
		                                          token_index + 1,
		                                          last_token_index,
		                                          statement->kind == MYLITE_STATEMENT_INSTALL);
	}
	if (!token_text_equals(parser, token_index, "PLUGIN")) {
		return 0;
	}

	if (statement->kind == MYLITE_STATEMENT_INSTALL) {
		return token_index + 3 == last_token_index &&
		       token_can_continue_object_name(&parser->tokens[token_index + 1]) &&
		       token_text_equals(parser, token_index + 2, "SONAME") &&
		       parser->tokens[token_index + 3].kind == MYLITE_TOKEN_STRING;
	}

	return token_index + 1 == last_token_index &&
	       token_can_continue_object_name(&parser->tokens[token_index + 1]);
}

static int validate_component_uri_list_syntax(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index,
                                              int allow_set_clause)
{
	int expecting_uri = 1;

	if (token_index > last_token_index) {
		return 0;
	}

	for (; token_index <= last_token_index; token_index++) {
		if (expecting_uri) {
			if (parser->tokens[token_index].kind != MYLITE_TOKEN_STRING) {
				return 0;
			}
			expecting_uri = 0;
			continue;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			break;
		}
		expecting_uri = 1;
	}
	if (expecting_uri) {
		return 0;
	}
	if (token_index > last_token_index) {
		return 1;
	}
	if (allow_set_clause && token_text_equals(parser, token_index, "SET")) {
		return validate_install_component_set_clause_syntax(parser, token_index + 1, last_token_index);
	}
	return 0;
}

static int validate_install_component_set_clause_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		size_t last_name_token;
		int has_expression_token = 0;

		if (token_is_install_component_set_scope(parser, token_index)) {
			token_index++;
		}
		if (token_index > last_token_index ||
		    !token_can_start_set_system_variable_name(parser, token_index, last_token_index)) {
			return 0;
		}

		last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
		token_index = last_name_token + 1;
		if (token_index > last_token_index ||
		    !token_is_assignment_operator(parser, token_index)) {
			return 0;
		}

		token_index++;
		while (token_index <= last_token_index) {
			size_t matching_token = parser->tokens[token_index].matching_token;

			if (matching_token > token_index + 1) {
				has_expression_token = 1;
				token_index = matching_token;
				continue;
			}
			if (parser->tokens[token_index].parser_token == ',') {
				break;
			}
			has_expression_token = 1;
			token_index++;
		}
		if (!has_expression_token) {
			return 0;
		}
		if (token_index > last_token_index) {
			return 1;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}
	return 1;
}

static int token_is_install_component_set_scope(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "GLOBAL") ||
	       token_text_equals(parser, token_index, "LOCAL") ||
	       token_text_equals(parser, token_index, "SESSION") ||
	       token_text_equals(parser, token_index, "PERSIST") ||
	       token_text_equals(parser, token_index, "PERSIST_ONLY");
}

static int validate_lock_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "INSTANCE")) {
		return token_index + 2 == last_token_index &&
		       token_text_equals(parser, token_index + 1, "FOR") &&
		       token_text_equals(parser, token_index + 2, "BACKUP");
	}
	if (parser->tokens[token_index].parser_token != TABLE_T &&
	    !token_text_equals(parser, token_index, "TABLES")) {
		return 0;
	}

	return validate_lock_table_list_syntax(parser, token_index + 1, last_token_index);
}

static int validate_lock_table_list_syntax(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		size_t last_name_token;

		if (!token_can_start_lock_table_name(parser, token_index)) {
			return 0;
		}
		last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
		token_index = last_name_token + 1;

		if (token_index <= last_token_index && parser->tokens[token_index].parser_token == AS_T) {
			token_index++;
			if (token_index > last_token_index || !token_can_start_lock_table_alias(parser, token_index)) {
				return 0;
			}
			token_index++;
		} else if (token_index <= last_token_index && token_can_start_lock_table_alias(parser, token_index)) {
			token_index++;
		}

		if (token_index > last_token_index) {
			return 0;
		}
		if (token_text_equals(parser, token_index, "READ")) {
			token_index++;
			if (token_index <= last_token_index && token_text_equals(parser, token_index, "LOCAL")) {
				token_index++;
			}
		} else if (token_text_equals(parser, token_index, "LOW_PRIORITY") &&
		           token_index + 1 <= last_token_index &&
		           token_text_equals(parser, token_index + 1, "WRITE")) {
			token_index += 2;
		} else if (token_text_equals(parser, token_index, "WRITE")) {
			token_index++;
		} else {
			return 0;
		}

		if (token_index > last_token_index) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	return 1;
}

static int token_can_start_lock_table_name(const mylite_parser *parser, size_t token_index)
{
	if (token_index >= parser->token_count ||
	    token_text_equals(parser, token_index, "READ") ||
	    token_text_equals(parser, token_index, "WRITE")) {
		return 0;
	}
	return token_can_continue_object_name(&parser->tokens[token_index]);
}

static int token_can_start_lock_table_alias(const mylite_parser *parser, size_t token_index)
{
	if (token_index >= parser->token_count || token_is_lock_table_mode(parser, token_index)) {
		return 0;
	}
	return token_can_continue_object_name(&parser->tokens[token_index]);
}

static int token_is_lock_table_mode(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "READ") ||
	       token_text_equals(parser, token_index, "WRITE");
}

static int validate_unlock_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	return token_index == last_token_index &&
	       (token_text_equals(parser, token_index, "INSTANCE") ||
	        parser->tokens[token_index].parser_token == TABLE_T ||
	        token_text_equals(parser, token_index, "TABLES"));
}

static int validate_cursor_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	last_token_index = statement->last_token - 1;
	if (cursor_statement_is_compound_label_token(parser, token_index, last_token_index)) {
		return 1;
	}

	token_index++;
	if (statement->kind == MYLITE_STATEMENT_FETCH) {
		return validate_fetch_cursor_statement_syntax(parser, token_index, last_token_index);
	}
	return validate_open_close_cursor_statement_syntax(parser, token_index, last_token_index);
}

static int cursor_statement_is_compound_label_token(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index)
{
	if (token_index + 1 <= last_token_index &&
	    token_index + 1 < parser->token_count) {
		size_t offset;

		if (token_text_equals(parser, token_index + 1, ":")) {
			return 1;
		}
		for (offset = parser->tokens[token_index].end_offset;
		     offset < parser->tokens[token_index + 1].start_offset;
		     offset++) {
			if (parser->lexer.input[offset] == ':') {
				return 1;
			}
		}
	}
	if (token_index > 0) {
		int previous_token = parser->tokens[token_index - 1].parser_token;

		return previous_token == END_LOOP_T ||
		       previous_token == END_REPEAT_T ||
		       previous_token == END_WHILE_T ||
		       previous_token == LEAVE_T ||
		       previous_token == ITERATE_T;
	}
	return 0;
}

static int validate_open_close_cursor_statement_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index)
{
	return token_index == last_token_index &&
	       token_index < parser->token_count &&
	       token_can_continue_object_name(&parser->tokens[token_index]);
}

static int validate_fetch_cursor_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "NEXT")) {
		if (token_index + 1 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "FROM")) {
			return 0;
		}
		token_index += 2;
	} else if (token_text_equals(parser, token_index, "FROM")) {
		token_index++;
	}

	if (token_index + 2 > last_token_index ||
	    !token_can_continue_object_name(&parser->tokens[token_index]) ||
	    parser->tokens[token_index + 1].parser_token != INTO_T) {
		return 0;
	}

	return validate_fetch_variable_list_syntax(parser, token_index + 2, last_token_index);
}

static int validate_fetch_variable_list_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		if (!token_can_start_fetch_variable(&parser->tokens[token_index])) {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}
	return 1;
}

static int token_can_start_fetch_variable(const mylite_token *token)
{
	return token->kind == MYLITE_TOKEN_USER_VARIABLE ||
	       token_can_start_local_variable_name(token);
}

static int validate_handler_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;
	size_t last_name_token;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index ||
	    token_index >= parser->token_count ||
	    !token_can_continue_object_name(&parser->tokens[token_index])) {
		return 0;
	}

	last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
	token_index = last_name_token + 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == OPEN_T) {
		return validate_handler_open_statement_syntax(parser, token_index + 1, last_token_index);
	}
	if (parser->tokens[token_index].parser_token == READ_T) {
		return validate_handler_read_statement_syntax(parser, token_index + 1, last_token_index);
	}
	if (parser->tokens[token_index].parser_token == CLOSE_T) {
		return token_index == last_token_index;
	}
	return 0;
}

static int validate_handler_open_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 1;
	}
	if (token_index >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == AS_T) {
		token_index++;
	}
	return token_index == last_token_index &&
	       token_can_continue_object_name(&parser->tokens[token_index]);
}

static int validate_handler_read_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (token_is_handler_read_direction(parser, token_index)) {
		return validate_handler_read_tail_syntax(parser, token_index + 1, last_token_index);
	}

	if (!token_can_start_handler_index_name(parser, token_index)) {
		return 0;
	}
	token_index++;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (token_is_handler_indexed_read_direction(parser, token_index)) {
		return validate_handler_read_tail_syntax(parser, token_index + 1, last_token_index);
	}
	if (token_is_handler_read_comparison_operator(parser, token_index)) {
		return validate_handler_read_key_comparison_syntax(parser, token_index, last_token_index);
	}
	return 0;
}

static int validate_handler_read_key_comparison_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index)
{
	size_t value_group_token_index;
	size_t close_token_index;

	if (!token_is_handler_read_comparison_operator(parser, token_index) ||
	    token_index + 1 > last_token_index) {
		return 0;
	}

	value_group_token_index = token_index + 1;
	if (parser->tokens[value_group_token_index].parser_token != '(' ||
	    parser->tokens[value_group_token_index].matching_token <= value_group_token_index + 1) {
		return 0;
	}
	close_token_index = parser->tokens[value_group_token_index].matching_token - 1;
	if (close_token_index > last_token_index ||
	    !validate_expression_list_syntax(parser, value_group_token_index + 1, close_token_index - 1)) {
		return 0;
	}
	return validate_handler_read_tail_syntax(parser,
	                                         parser->tokens[value_group_token_index].matching_token,
	                                         last_token_index);
}

static int validate_handler_read_tail_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 1;
	}
	if (token_index >= parser->token_count) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "WHERE")) {
		if (!validate_handler_where_clause_syntax(parser, token_index + 1, last_token_index, &token_index)) {
			return 0;
		}
	}
	if (token_index > last_token_index) {
		return 1;
	}
	if (!token_text_equals(parser, token_index, "LIMIT")) {
		return 0;
	}
	return validate_nonempty_expression_tail_syntax(parser, token_index + 1, last_token_index);
}

static int validate_handler_where_clause_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index,
                                                size_t *next_token_index)
{
	size_t expression_first_token = token_index;
	size_t expression_last_token;

	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (token_text_equals(parser, token_index, "LIMIT")) {
			break;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
			continue;
		}
		token_index++;
	}

	if (token_index == expression_first_token) {
		return 0;
	}
	expression_last_token = token_index - 1;
	if (!validate_nonempty_expression_tail_syntax(parser, expression_first_token, expression_last_token)) {
		return 0;
	}

	*next_token_index = token_index;
	return 1;
}

static int token_can_start_handler_index_name(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (token_can_continue_object_name(&parser->tokens[token_index]) ||
	        parser->tokens[token_index].parser_token == PRIMARY_T);
}

static int token_is_handler_read_direction(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (token_text_equals(parser, token_index, "FIRST") ||
	        token_text_equals(parser, token_index, "NEXT"));
}

static int token_is_handler_indexed_read_direction(const mylite_parser *parser, size_t token_index)
{
	return token_is_handler_read_direction(parser, token_index) ||
	       (token_index < parser->token_count &&
	        (token_text_equals(parser, token_index, "PREV") ||
	         token_text_equals(parser, token_index, "LAST")));
}

static int token_is_handler_read_comparison_operator(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (token_text_equals(parser, token_index, "=") ||
	        token_text_equals(parser, token_index, "<=") ||
	        token_text_equals(parser, token_index, ">=") ||
	        token_text_equals(parser, token_index, "<") ||
	        token_text_equals(parser, token_index, ">"));
}

static int validate_cache_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t next_token_index;
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index ||
	    token_index >= parser->token_count ||
	    parser->tokens[token_index].parser_token != INDEX_T) {
		return 0;
	}

	if (!validate_table_index_list_syntax(parser, token_index + 1, last_token_index, &next_token_index, 0)) {
		return 0;
	}
	if (next_token_index > last_token_index ||
	    parser->tokens[next_token_index].parser_token != IN_T ||
	    next_token_index + 1 != last_token_index) {
		return 0;
	}
	return token_can_start_key_cache_name(parser, next_token_index + 1);
}

static int validate_load_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == INDEX_T) {
		return validate_load_index_statement_syntax(parser, statement);
	}
	if (parser->tokens[token_index].parser_token == DATA_T) {
		return validate_load_data_or_xml_statement_syntax(parser, token_index, last_token_index, 0);
	}
	if (token_text_equals(parser, token_index, "XML")) {
		return validate_load_data_or_xml_statement_syntax(parser, token_index, last_token_index, 1);
	}

	return 0;
}

static int validate_load_data_or_xml_statement_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index,
                                                      int is_xml)
{
	if (token_index > last_token_index ||
	    (is_xml && !token_text_equals(parser, token_index, "XML")) ||
	    (!is_xml && parser->tokens[token_index].parser_token != DATA_T)) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index &&
	    (parser->tokens[token_index].parser_token == LOW_PRIORITY_T ||
	     token_text_equals(parser, token_index, "CONCURRENT"))) {
		token_index++;
	}
	if (token_index <= last_token_index && parser->tokens[token_index].parser_token == LOCAL_T) {
		token_index++;
	}
	if (token_index + 1 > last_token_index ||
	    parser->tokens[token_index].parser_token != INFILE_T ||
	    parser->tokens[token_index + 1].kind != MYLITE_TOKEN_STRING) {
		return 0;
	}
	token_index += 2;

	if (token_index <= last_token_index &&
	    (parser->tokens[token_index].parser_token == REPLACE_T ||
	     parser->tokens[token_index].parser_token == IGNORE_T)) {
		token_index++;
	}
	if (token_index + 2 > last_token_index ||
	    parser->tokens[token_index].parser_token != INTO_T ||
	    parser->tokens[token_index + 1].parser_token != TABLE_T ||
	    !token_can_start_object_name(&parser->tokens[token_index + 2])) {
		return 0;
	}
	token_index = last_qualified_name_token(parser, token_index + 2, last_token_index) + 1;

	while (token_index <= last_token_index) {
		if (!is_xml && token_text_equals(parser, token_index, "PARTITION")) {
			if (!validate_load_partition_clause_syntax(parser,
			                                           token_index,
			                                           last_token_index,
			                                           &token_index)) {
				return 0;
			}
			continue;
		}
		if (parser->tokens[token_index].parser_token == CHARACTER_T) {
			if (!validate_load_character_set_clause_syntax(parser,
			                                               token_index,
			                                               last_token_index,
			                                               &token_index)) {
				return 0;
			}
			continue;
		}
		if (!is_xml &&
		    (parser->tokens[token_index].parser_token == FIELDS_T ||
		     parser->tokens[token_index].parser_token == COLUMNS_T ||
		     token_text_equals(parser, token_index, "LINES"))) {
			if (!validate_load_fields_or_lines_clause_syntax(parser,
			                                                 token_index,
			                                                 last_token_index,
			                                                 &token_index)) {
				return 0;
			}
			continue;
		}
		if (is_xml && token_text_equals(parser, token_index, "ROWS")) {
			if (!validate_load_rows_identified_clause_syntax(parser,
			                                                 token_index,
			                                                 last_token_index,
			                                                 &token_index)) {
				return 0;
			}
			continue;
		}
		if (parser->tokens[token_index].parser_token == IGNORE_T) {
			if (!validate_load_ignore_rows_clause_syntax(parser,
			                                             token_index,
			                                             last_token_index,
			                                             &token_index)) {
				return 0;
			}
			continue;
		}
		if (parser->tokens[token_index].parser_token == '(') {
			if (!validate_load_column_list_syntax(parser, token_index)) {
				return 0;
			}
			token_index = parser->tokens[token_index].matching_token;
			continue;
		}
		if (parser->tokens[token_index].parser_token == SET_T) {
			return validate_load_set_clause_syntax(parser, token_index, last_token_index);
		}
		return 0;
	}

	return 1;
}

static int validate_load_partition_clause_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 size_t *next_token_index)
{
	if (token_index + 1 > last_token_index ||
	    !token_text_equals(parser, token_index, "PARTITION") ||
	    parser->tokens[token_index + 1].parser_token != '(' ||
	    !validate_name_list_group_syntax(parser, token_index + 1, 0, 0)) {
		return 0;
	}

	*next_token_index = parser->tokens[token_index + 1].matching_token;
	return 1;
}

static int validate_load_character_set_clause_syntax(const mylite_parser *parser,
                                                     size_t token_index,
                                                     size_t last_token_index,
                                                     size_t *next_token_index)
{
	if (token_index + 2 > last_token_index ||
	    parser->tokens[token_index].parser_token != CHARACTER_T ||
	    parser->tokens[token_index + 1].parser_token != SET_T ||
	    !token_can_start_alter_table_name(&parser->tokens[token_index + 2])) {
		return 0;
	}

	*next_token_index = token_index + 3;
	return 1;
}

static int validate_load_fields_or_lines_clause_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index)
{
	int is_lines_clause = token_text_equals(parser, token_index, "LINES");
	int saw_option = 0;

	if (!is_lines_clause &&
	    parser->tokens[token_index].parser_token != FIELDS_T &&
	    parser->tokens[token_index].parser_token != COLUMNS_T) {
		return 0;
	}

	token_index++;
	while (token_index <= last_token_index) {
		if (!is_lines_clause && token_text_equals(parser, token_index, "TERMINATED")) {
			if (token_index + 2 > last_token_index ||
			    parser->tokens[token_index + 1].parser_token != BY_T ||
			    parser->tokens[token_index + 2].kind != MYLITE_TOKEN_STRING) {
				return 0;
			}
			token_index += 3;
			saw_option = 1;
			continue;
		}
		if (!is_lines_clause &&
		    (token_text_equals(parser, token_index, "ENCLOSED") ||
		     token_text_equals(parser, token_index, "OPTIONALLY"))) {
			if (token_text_equals(parser, token_index, "OPTIONALLY")) {
				token_index++;
			}
			if (token_index + 2 > last_token_index ||
			    !token_text_equals(parser, token_index, "ENCLOSED") ||
			    parser->tokens[token_index + 1].parser_token != BY_T ||
			    parser->tokens[token_index + 2].kind != MYLITE_TOKEN_STRING) {
				return 0;
			}
			token_index += 3;
			saw_option = 1;
			continue;
		}
		if (!is_lines_clause && token_text_equals(parser, token_index, "ESCAPED")) {
			if (token_index + 2 > last_token_index ||
			    parser->tokens[token_index + 1].parser_token != BY_T ||
			    parser->tokens[token_index + 2].kind != MYLITE_TOKEN_STRING) {
				return 0;
			}
			token_index += 3;
			saw_option = 1;
			continue;
		}
		if (is_lines_clause &&
		    (token_text_equals(parser, token_index, "STARTING") ||
		     token_text_equals(parser, token_index, "TERMINATED"))) {
			if (token_index + 2 > last_token_index ||
			    parser->tokens[token_index + 1].parser_token != BY_T ||
			    parser->tokens[token_index + 2].kind != MYLITE_TOKEN_STRING) {
				return 0;
			}
			token_index += 3;
			saw_option = 1;
			continue;
		}
		break;
	}

	*next_token_index = token_index;
	return saw_option || token_index <= last_token_index;
}

static int validate_load_ignore_rows_clause_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index,
                                                   size_t *next_token_index)
{
	if (token_index + 2 > last_token_index ||
	    parser->tokens[token_index].parser_token != IGNORE_T ||
	    parser->tokens[token_index + 1].kind != MYLITE_TOKEN_NUMBER ||
	    (!token_text_equals(parser, token_index + 2, "LINES") &&
	     !token_text_equals(parser, token_index + 2, "ROWS"))) {
		return 0;
	}

	*next_token_index = token_index + 3;
	return 1;
}

static int validate_load_rows_identified_clause_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index)
{
	if (token_index + 3 > last_token_index ||
	    !token_text_equals(parser, token_index, "ROWS") ||
	    !token_text_equals(parser, token_index + 1, "IDENTIFIED") ||
	    parser->tokens[token_index + 2].parser_token != BY_T ||
	    parser->tokens[token_index + 3].kind != MYLITE_TOKEN_STRING) {
		return 0;
	}

	*next_token_index = token_index + 4;
	return 1;
}

static int validate_load_column_list_syntax(const mylite_parser *parser, size_t open_token_index)
{
	size_t token_index = open_token_index + 1;
	size_t close_token_index;
	int expecting_name = 1;

	if (open_token_index >= parser->token_count ||
	    parser->tokens[open_token_index].parser_token != '(' ||
	    parser->tokens[open_token_index].matching_token <= open_token_index) {
		return 0;
	}

	close_token_index = parser->tokens[open_token_index].matching_token - 1;
	if (token_index == close_token_index) {
		return 1;
	}

	for (; token_index < close_token_index; token_index++) {
		if (expecting_name) {
			if (parser->tokens[token_index].kind != MYLITE_TOKEN_USER_VARIABLE &&
			    !token_can_start_alter_table_name(&parser->tokens[token_index])) {
				return 0;
			}
			expecting_name = 0;
			continue;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		expecting_name = 1;
	}

	return !expecting_name;
}

static int validate_load_set_clause_syntax(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index)
{
	if (token_index > last_token_index || parser->tokens[token_index].parser_token != SET_T) {
		return 0;
	}

	token_index++;
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		if (!validate_load_set_assignment_syntax(parser,
		                                         token_index,
		                                         last_token_index,
		                                         &token_index)) {
			return 0;
		}
		if (token_index > last_token_index) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	return 1;
}

static int validate_load_set_assignment_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index,
                                               size_t *next_token_index)
{
	int saw_expression_token = 0;

	if (token_index > last_token_index ||
	    !token_can_start_alter_table_name(&parser->tokens[token_index])) {
		return 0;
	}

	token_index = last_qualified_name_token(parser, token_index, last_token_index) + 1;
	if (token_index > last_token_index || !token_is_assignment_operator(parser, token_index)) {
		return 0;
	}
	token_index++;
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (parser->tokens[token_index].parser_token == ',') {
			break;
		}
		saw_expression_token = 1;
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}

	*next_token_index = token_index;
	return saw_expression_token;
}

static int validate_load_index_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t next_token_index;
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}
	if (parser->tokens[token_index].parser_token != INDEX_T) {
		return 1;
	}
	if (token_index + 2 > last_token_index ||
	    parser->tokens[token_index + 1].parser_token != INTO_T ||
	    parser->tokens[token_index + 2].parser_token != CACHE_T) {
		return 0;
	}

	return validate_table_index_list_syntax(parser, token_index + 3, last_token_index, &next_token_index, 1) &&
	       next_token_index > last_token_index;
}

static int validate_table_index_list_syntax(const mylite_parser *parser,
                                            size_t token_index,
                                            size_t last_token_index,
                                            size_t *next_token_index,
                                            int allow_ignore_leaves)
{
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		if (!validate_table_index_entry_syntax(parser,
		                                       token_index,
		                                       last_token_index,
		                                       &token_index,
		                                       allow_ignore_leaves)) {
			return 0;
		}
		if (token_index > last_token_index ||
		    parser->tokens[token_index].parser_token == IN_T) {
			break;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index || parser->tokens[token_index].parser_token == IN_T) {
			return 0;
		}
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_table_index_entry_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index,
                                             size_t *next_token_index,
                                             int allow_ignore_leaves)
{
	size_t last_name_token;

	if (!token_can_start_table_index_table_name(parser, token_index)) {
		return 0;
	}

	last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
	token_index = last_name_token + 1;

	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "PARTITION")) {
		token_index++;
		if (parser->tokens[token_index].parser_token != '(' ||
		    !validate_name_list_group_syntax(parser, token_index, 1, 0)) {
			return 0;
		}
		token_index = parser->tokens[token_index].matching_token;
	}

	if (token_index + 1 <= last_token_index &&
	    (parser->tokens[token_index].parser_token == INDEX_T ||
	     parser->tokens[token_index].parser_token == KEY_T)) {
		token_index++;
		if (parser->tokens[token_index].parser_token != '(' ||
		    !validate_name_list_group_syntax(parser, token_index, 0, 1)) {
			return 0;
		}
		token_index = parser->tokens[token_index].matching_token;
	}

	if (allow_ignore_leaves &&
	    token_index + 1 <= last_token_index &&
	    parser->tokens[token_index].parser_token == IGNORE_T) {
		if (!token_text_equals(parser, token_index + 1, "LEAVES")) {
			return 0;
		}
		token_index += 2;
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_name_list_group_syntax(const mylite_parser *parser,
                                           size_t open_token_index,
                                           int allow_all,
                                           int allow_primary)
{
	size_t token_index;
	size_t close_token_index;
	int expecting_name = 1;

	if (open_token_index >= parser->token_count ||
	    parser->tokens[open_token_index].parser_token != '(' ||
	    parser->tokens[open_token_index].matching_token <= open_token_index + 1) {
		return 0;
	}

	close_token_index = parser->tokens[open_token_index].matching_token - 1;
	token_index = open_token_index + 1;
	if (allow_all &&
	    token_index == close_token_index - 1 &&
	    parser->tokens[token_index].parser_token == ALL_T) {
		return 1;
	}

	for (; token_index < close_token_index; token_index++) {
		if (expecting_name) {
			if (!token_can_start_table_index_item_name(parser, token_index, allow_primary)) {
				return 0;
			}
			expecting_name = 0;
			continue;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		expecting_name = 1;
	}

	return !expecting_name;
}

static int token_can_start_table_index_table_name(const mylite_parser *parser, size_t token_index)
{
	if (token_index >= parser->token_count) {
		return 0;
	}
	return token_can_continue_object_name(&parser->tokens[token_index]);
}

static int token_can_start_table_index_item_name(const mylite_parser *parser,
                                                 size_t token_index,
                                                 int allow_primary)
{
	if (token_index >= parser->token_count) {
		return 0;
	}
	return token_can_continue_object_name(&parser->tokens[token_index]) ||
	       (allow_primary && parser->tokens[token_index].parser_token == PRIMARY_T);
}

static int token_can_start_key_cache_name(const mylite_parser *parser, size_t token_index)
{
	if (token_index >= parser->token_count) {
		return 0;
	}
	return token_can_continue_object_name(&parser->tokens[token_index]) ||
	       parser->tokens[token_index].parser_token == DEFAULT_T;
}

static int validate_purge_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index + 3 > last_token_index ||
	    token_index + 3 >= parser->token_count ||
	    (!token_text_equals(parser, token_index, "BINARY") &&
	     !token_text_equals(parser, token_index, "MASTER")) ||
	    !token_text_equals(parser, token_index + 1, "LOGS")) {
		return 0;
	}

	token_index += 2;
	if (parser->tokens[token_index].parser_token == TO_T) {
		return token_index + 1 == last_token_index &&
		       parser->tokens[token_index + 1].kind == MYLITE_TOKEN_STRING;
	}
	if (token_text_equals(parser, token_index, "BEFORE")) {
		return validate_nonempty_expression_tail_syntax(parser, token_index + 1, last_token_index);
	}
	return 0;
}

static int validate_nonempty_expression_tail_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index)
{
	if (token_index > last_token_index ||
	    token_index >= parser->token_count ||
	    parser->tokens[token_index].parser_token == ',' ||
	    parser->tokens[last_token_index].parser_token == ',') {
		return 0;
	}
	return 1;
}

static int validate_set_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == ROLE_T) {
		return validate_set_role_statement_syntax(parser, token_index + 1, last_token_index);
	}
	if (parser->tokens[token_index].parser_token == DEFAULT_T &&
	    token_index + 1 <= last_token_index &&
	    parser->tokens[token_index + 1].parser_token == ROLE_T) {
		return validate_set_default_role_statement_syntax(parser, token_index + 2, last_token_index);
	}
	if (token_text_equals(parser, token_index, "PASSWORD")) {
		return validate_set_password_statement_syntax(parser, token_index + 1, last_token_index);
	}
	if (token_is_resource_group_sequence(parser, token_index, last_token_index)) {
		return validate_set_resource_group_statement_syntax(parser, token_index, last_token_index);
	}
	if (token_text_equals(parser, token_index, "NAMES")) {
		return validate_set_names_statement_syntax(parser, token_index + 1, last_token_index);
	}
	if (parser->tokens[token_index].parser_token == CHARACTER_T &&
	    token_index + 1 <= last_token_index &&
	    parser->tokens[token_index + 1].parser_token == SET_T) {
		return validate_set_character_set_statement_syntax(parser, token_index + 2, last_token_index);
	}
	if (parser->tokens[token_index].parser_token == CHARSET_T) {
		return validate_set_character_set_statement_syntax(parser, token_index + 1, last_token_index);
	}

	return 1;
}

static int validate_set_role_statement_syntax(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "DEFAULT") ||
	    token_text_equals(parser, token_index, "NONE")) {
		return token_index == last_token_index;
	}

	if (token_text_equals(parser, token_index, "ALL")) {
		if (token_index == last_token_index) {
			return 1;
		}
		if (token_index + 1 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "EXCEPT")) {
			return 0;
		}
		return validate_principal_name_list_syntax(parser, token_index + 2, last_token_index, 0);
	}

	return validate_principal_name_list_syntax(parser, token_index, last_token_index, 0);
}

static int validate_set_default_role_statement_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index)
{
	int uses_role_collection;
	int saw_role = 0;

	if (token_index > last_token_index) {
		return 0;
	}

	uses_role_collection = token_text_equals(parser, token_index, "NONE") ||
	                       token_text_equals(parser, token_index, "ALL");
	if (uses_role_collection) {
		token_index++;
	} else {
		while (token_index <= last_token_index) {
			if (token_text_equals(parser, token_index, "TO")) {
				break;
			}
			if (!validate_principal_name_syntax(parser, token_index, last_token_index, 0, &token_index)) {
				return 0;
			}
			saw_role = 1;
			if (token_index > last_token_index || token_text_equals(parser, token_index, "TO")) {
				break;
			}
			if (parser->tokens[token_index].parser_token != ',') {
				return 0;
			}
			token_index++;
			if (token_index > last_token_index) {
				return 0;
			}
		}
		if (!saw_role) {
			return 0;
		}
	}

	if (token_index > last_token_index || !token_text_equals(parser, token_index, "TO")) {
		return 0;
	}
	return validate_principal_name_list_syntax(parser, token_index + 1, last_token_index, 0);
}

static int validate_set_password_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "FOR")) {
		token_index++;
		if (!validate_principal_name_syntax(parser, token_index, last_token_index, 1, &token_index)) {
			return 0;
		}
	}

	if (!validate_set_password_auth_option_syntax(parser,
	                                              token_index,
	                                              last_token_index,
	                                              &token_index)) {
		return 0;
	}
	if (token_index > last_token_index) {
		return 1;
	}
	return validate_set_password_tail_syntax(parser, token_index, last_token_index);
}

static int validate_set_password_auth_option_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    size_t *next_token_index)
{
	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "=") &&
	    token_is_set_password_string_value(parser, token_index + 1)) {
		*next_token_index = token_index + 2;
		return 1;
	}

	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "TO") &&
	    token_text_equals(parser, token_index + 1, "RANDOM")) {
		*next_token_index = token_index + 2;
		return 1;
	}

	return 0;
}

static int validate_set_password_tail_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index)
{
	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "REPLACE")) {
		token_index++;
		if (!token_is_set_password_string_value(parser, token_index)) {
			return 0;
		}
		token_index++;
	}

	if (token_index > last_token_index) {
		return 1;
	}

	if (token_index + 2 == last_token_index &&
	    token_text_equals(parser, token_index, "RETAIN") &&
	    token_text_equals(parser, token_index + 1, "CURRENT") &&
	    token_text_equals(parser, token_index + 2, "PASSWORD")) {
		return 1;
	}

	return 0;
}

static int token_is_set_password_string_value(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       parser->tokens[token_index].kind == MYLITE_TOKEN_STRING;
}

static int validate_set_resource_group_statement_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index)
{
	if (!token_is_resource_group_sequence(parser, token_index, last_token_index)) {
		return 0;
	}

	token_index += 2;
	if (token_index > last_token_index ||
	    !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index++;

	if (token_index > last_token_index) {
		return 1;
	}
	if (!token_text_equals(parser, token_index, "FOR")) {
		return 0;
	}
	token_index++;
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		if (parser->tokens[token_index].kind != MYLITE_TOKEN_NUMBER) {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}
	return 1;
}

static int validate_set_names_statement_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index)
{
	int uses_default_character_set;

	if (token_index > last_token_index || !token_can_be_character_set_value(parser, token_index)) {
		return 0;
	}

	uses_default_character_set = parser->tokens[token_index].parser_token == DEFAULT_T;
	token_index++;
	if (token_index > last_token_index) {
		return 1;
	}
	if (parser->tokens[token_index].parser_token == ',') {
		return validate_set_assignment_tail_syntax(parser, token_index + 1, last_token_index);
	}
	if (uses_default_character_set ||
	    token_index + 1 > last_token_index ||
	    !token_text_equals(parser, token_index, "COLLATE") ||
	    !token_can_be_collation_value(parser, token_index + 1)) {
		return 0;
	}
	token_index += 2;
	if (token_index > last_token_index) {
		return 1;
	}
	return parser->tokens[token_index].parser_token == ',' &&
	       validate_set_assignment_tail_syntax(parser, token_index + 1, last_token_index);
}

static int validate_set_character_set_statement_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index)
{
	if (token_index > last_token_index || !token_can_be_character_set_value(parser, token_index)) {
		return 0;
	}
	token_index++;
	if (token_index > last_token_index) {
		return 1;
	}
	return parser->tokens[token_index].parser_token == ',' &&
	       validate_set_assignment_tail_syntax(parser, token_index + 1, last_token_index);
}

static int validate_set_assignment_tail_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index)
{
	int expecting_assignment = 1;

	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (matching_token > token_index + 1) {
			expecting_assignment = 0;
			token_index = matching_token;
			continue;
		}
		if (parser->tokens[token_index].parser_token == ',') {
			if (expecting_assignment) {
				return 0;
			}
			expecting_assignment = 1;
			token_index++;
			continue;
		}
		expecting_assignment = 0;
		token_index++;
	}
	return !expecting_assignment;
}

static int token_can_be_character_set_value(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (parser->tokens[token_index].parser_token == DEFAULT_T ||
	        token_can_be_collation_value(parser, token_index));
}

static int token_can_be_collation_value(const mylite_parser *parser, size_t token_index)
{
	if (token_index >= parser->token_count) {
		return 0;
	}
	return parser->tokens[token_index].kind == MYLITE_TOKEN_IDENTIFIER ||
	       parser->tokens[token_index].kind == MYLITE_TOKEN_QUOTED_IDENTIFIER ||
	       parser->tokens[token_index].kind == MYLITE_TOKEN_STRING ||
	       token_text_equals(parser, token_index, "BINARY");
}

static int validate_reset_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "PERSIST")) {
		return validate_reset_persist_statement_syntax(parser, token_index + 1, last_token_index);
	}

	while (token_index <= last_token_index) {
		if (!validate_reset_option_syntax(parser, token_index, last_token_index, &token_index)) {
			return 0;
		}
		if (token_index > last_token_index) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	return 1;
}

static int validate_reset_persist_statement_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index)
{
	size_t last_name_token;

	if (token_index > last_token_index) {
		return 1;
	}

	if (parser->tokens[token_index].parser_token == IF_T) {
		if (token_index + 2 > last_token_index ||
		    parser->tokens[token_index + 1].parser_token != EXISTS_T) {
			return 0;
		}
		token_index += 2;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	if (!token_can_start_set_system_variable_name(parser, token_index, last_token_index)) {
		return 0;
	}
	last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
	return last_name_token == last_token_index;
}

static int validate_reset_option_syntax(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index,
                                        size_t *next_token_index)
{
	if (token_text_equals(parser, token_index, "BINARY")) {
		return validate_reset_binary_logs_option_syntax(parser,
		                                                token_index,
		                                                last_token_index,
		                                                next_token_index);
	}
	if (token_text_equals(parser, token_index, "MASTER")) {
		*next_token_index = token_index + 1;
		if (*next_token_index <= last_token_index && parser->tokens[*next_token_index].parser_token == TO_T) {
			if (*next_token_index + 1 > last_token_index ||
			    parser->tokens[*next_token_index + 1].kind != MYLITE_TOKEN_NUMBER) {
				return 0;
			}
			*next_token_index += 2;
		}
		return 1;
	}
	if (token_text_equals(parser, token_index, "REPLICA") ||
	    token_text_equals(parser, token_index, "SLAVE")) {
		return validate_reset_replica_option_syntax(parser,
		                                            token_index,
		                                            last_token_index,
		                                            next_token_index);
	}
	return 0;
}

static int validate_reset_binary_logs_option_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    size_t *next_token_index)
{
	if (token_index + 3 > last_token_index ||
	    !token_text_equals(parser, token_index + 1, "LOGS") ||
	    parser->tokens[token_index + 2].parser_token != AND_T ||
	    !token_text_equals(parser, token_index + 3, "GTIDS")) {
		return 0;
	}

	*next_token_index = token_index + 4;
	if (*next_token_index <= last_token_index && parser->tokens[*next_token_index].parser_token == TO_T) {
		if (*next_token_index + 1 > last_token_index ||
		    parser->tokens[*next_token_index + 1].kind != MYLITE_TOKEN_NUMBER) {
			return 0;
		}
		*next_token_index += 2;
	}
	return 1;
}

static int validate_reset_replica_option_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index,
                                                size_t *next_token_index)
{
	token_index++;
	if (token_index <= last_token_index && parser->tokens[token_index].parser_token == ALL_T) {
		token_index++;
	}
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "FOR")) {
		if (token_index + 2 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "CHANNEL") ||
		    parser->tokens[token_index + 2].kind != MYLITE_TOKEN_STRING) {
			return 0;
		}
		token_index += 3;
	}
	*next_token_index = token_index;
	return 1;
}

static int validate_flush_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == LOCAL_T ||
	    token_text_equals(parser, token_index, "NO_WRITE_TO_BINLOG")) {
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	if (parser->tokens[token_index].parser_token == TABLE_T ||
	    token_text_equals(parser, token_index, "TABLES")) {
		return validate_flush_table_option_syntax(parser, token_index + 1, last_token_index);
	}
	return validate_flush_option_list_syntax(parser, token_index, last_token_index);
}

static int validate_flush_table_option_syntax(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index)
{
	size_t next_token_index;

	if (token_index > last_token_index) {
		return 1;
	}

	if (token_text_equals(parser, token_index, "WITH")) {
		return token_index + 2 == last_token_index &&
		       token_text_equals(parser, token_index + 1, "READ") &&
		       token_text_equals(parser, token_index + 2, "LOCK");
	}
	if (!validate_flush_table_name_list_syntax(parser, token_index, last_token_index, &next_token_index)) {
		return 0;
	}
	if (next_token_index > last_token_index) {
		return 1;
	}
	if (token_text_equals(parser, next_token_index, "WITH")) {
		return next_token_index + 2 == last_token_index &&
		       token_text_equals(parser, next_token_index + 1, "READ") &&
		       token_text_equals(parser, next_token_index + 2, "LOCK");
	}
	if (token_text_equals(parser, next_token_index, "FOR")) {
		return next_token_index + 1 == last_token_index &&
		       token_text_equals(parser, next_token_index + 1, "EXPORT");
	}
	return 0;
}

static int validate_flush_table_name_list_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 size_t *next_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		size_t last_name_token;

		if (!token_can_start_flush_table_name(parser, token_index)) {
			return 0;
		}
		last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
		token_index = last_name_token + 1;
		if (token_index > last_token_index ||
		    token_text_equals(parser, token_index, "WITH") ||
		    token_text_equals(parser, token_index, "FOR")) {
			break;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index ||
		    token_text_equals(parser, token_index, "WITH") ||
		    token_text_equals(parser, token_index, "FOR")) {
			return 0;
		}
	}

	*next_token_index = token_index;
	return 1;
}

static int token_can_start_flush_table_name(const mylite_parser *parser, size_t token_index)
{
	if (token_index >= parser->token_count) {
		return 0;
	}
	return token_can_continue_object_name(&parser->tokens[token_index]);
}

static int validate_flush_option_list_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		if (!validate_flush_option_syntax(parser, token_index, last_token_index, &token_index)) {
			return 0;
		}
		if (token_index > last_token_index) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}
	return 1;
}

static int validate_flush_option_syntax(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index,
                                        size_t *next_token_index)
{
	if (token_text_equals(parser, token_index, "RELAY")) {
		if (token_index + 1 > last_token_index ||
		    !token_text_equals(parser, token_index + 1, "LOGS")) {
			return 0;
		}
		token_index += 2;
		if (token_index <= last_token_index && token_text_equals(parser, token_index, "FOR")) {
			if (token_index + 2 > last_token_index ||
			    !token_text_equals(parser, token_index + 1, "CHANNEL") ||
			    parser->tokens[token_index + 2].kind != MYLITE_TOKEN_STRING) {
				return 0;
			}
			token_index += 3;
		}
		*next_token_index = token_index;
		return 1;
	}

	if (token_index + 1 <= last_token_index &&
	    (token_text_equals(parser, token_index, "BINARY") ||
	     token_text_equals(parser, token_index, "ENGINE") ||
	     token_text_equals(parser, token_index, "ERROR") ||
	     token_text_equals(parser, token_index, "GENERAL") ||
	     token_text_equals(parser, token_index, "SLOW"))) {
		if (!token_text_equals(parser, token_index + 1, "LOGS")) {
			return 0;
		}
		*next_token_index = token_index + 2;
		return 1;
	}

	if (token_text_equals(parser, token_index, "LOGS") ||
	    token_text_equals(parser, token_index, "PRIVILEGES") ||
	    token_text_equals(parser, token_index, "STATUS") ||
	    token_text_equals(parser, token_index, "HOSTS") ||
	    token_text_equals(parser, token_index, "OPTIMIZER_COSTS") ||
	    token_text_equals(parser, token_index, "USER_RESOURCES")) {
		*next_token_index = token_index + 1;
		return 1;
	}

	return 0;
}

static int validate_maintenance_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == LOCAL_T ||
	    token_text_equals(parser, token_index, "NO_WRITE_TO_BINLOG")) {
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	switch (statement->kind) {
	case MYLITE_STATEMENT_ANALYZE:
		return validate_analyze_statement_syntax(parser, token_index, last_token_index);
	case MYLITE_STATEMENT_CHECK:
		return validate_check_statement_syntax(parser, token_index, last_token_index);
	case MYLITE_STATEMENT_CHECKSUM:
		return validate_checksum_statement_syntax(parser, token_index, last_token_index);
	case MYLITE_STATEMENT_OPTIMIZE:
		return validate_optimize_statement_syntax(parser, token_index, last_token_index);
	case MYLITE_STATEMENT_REPAIR:
		return validate_repair_statement_syntax(parser, token_index, last_token_index);
	default:
		return 0;
	}
}

static int validate_analyze_statement_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index)
{
	size_t next_token_index;

	if (parser->tokens[token_index].parser_token == FORMAT_T) {
		token_index++;
		if (token_index > last_token_index || !token_text_equals(parser, token_index, "=")) {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index ||
		    !token_can_start_maintenance_table_name(parser, token_index)) {
			return 0;
		}
		token_index++;
	}

	if (!token_is_maintenance_table_keyword(parser, token_index) ||
	    !validate_maintenance_table_name_list_syntax(parser,
	                                                 token_index + 1,
	                                                 last_token_index,
	                                                 &next_token_index)) {
		return 0;
	}

	if (next_token_index > last_token_index) {
		return 1;
	}
	if (next_token_index + 2 <= last_token_index &&
	    token_text_equals(parser, next_token_index, "UPDATE") &&
	    token_text_equals(parser, next_token_index + 1, "HISTOGRAM") &&
	    parser->tokens[next_token_index + 2].parser_token == ON_T) {
		next_token_index += 3;
		if (!validate_histogram_column_list_syntax(parser,
		                                           next_token_index,
		                                           last_token_index,
		                                           &next_token_index)) {
			return 0;
		}
		if (next_token_index > last_token_index) {
			return 1;
		}
		if (token_text_equals(parser, next_token_index, "WITH")) {
			return next_token_index + 2 == last_token_index &&
			       parser->tokens[next_token_index + 1].kind == MYLITE_TOKEN_NUMBER &&
			       token_text_equals(parser, next_token_index + 2, "BUCKETS");
		}
		if ((token_text_equals(parser, next_token_index, "MANUAL") ||
		     token_text_equals(parser, next_token_index, "AUTO")) &&
		    next_token_index + 1 == last_token_index &&
		    token_text_equals(parser, next_token_index + 1, "UPDATE")) {
			return 1;
		}
		if (token_text_equals(parser, next_token_index, "USING") &&
		    token_text_equals(parser, next_token_index + 1, "DATA") &&
		    next_token_index + 2 == last_token_index &&
		    parser->tokens[next_token_index + 2].kind == MYLITE_TOKEN_STRING) {
			return 1;
		}
		return 0;
	}
	if (next_token_index + 2 <= last_token_index &&
	    token_text_equals(parser, next_token_index, "DROP") &&
	    token_text_equals(parser, next_token_index + 1, "HISTOGRAM") &&
	    parser->tokens[next_token_index + 2].parser_token == ON_T) {
		next_token_index += 3;
		return validate_histogram_column_list_syntax(parser,
		                                             next_token_index,
		                                             last_token_index,
		                                             &next_token_index) &&
		       next_token_index > last_token_index;
	}
	return 0;
}

static int validate_histogram_column_list_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 size_t *next_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		size_t last_name_token;

		if (!token_can_start_histogram_column_name(parser, token_index)) {
			return 0;
		}
		last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
		token_index = last_name_token + 1;
		if (token_index > last_token_index ||
		    token_text_equals(parser, token_index, "WITH") ||
		    token_text_equals(parser, token_index, "USING") ||
		    token_text_equals(parser, token_index, "MANUAL") ||
		    token_text_equals(parser, token_index, "AUTO")) {
			break;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index ||
		    token_text_equals(parser, token_index, "WITH") ||
		    token_text_equals(parser, token_index, "USING") ||
		    token_text_equals(parser, token_index, "MANUAL") ||
		    token_text_equals(parser, token_index, "AUTO")) {
			return 0;
		}
	}

	*next_token_index = token_index;
	return 1;
}

static int token_can_start_histogram_column_name(const mylite_parser *parser, size_t token_index)
{
	if (token_index >= parser->token_count) {
		return 0;
	}
	return token_can_continue_object_name(&parser->tokens[token_index]);
}

static int validate_check_statement_syntax(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index)
{
	size_t next_token_index;

	if (!token_is_maintenance_table_keyword(parser, token_index) ||
	    !validate_maintenance_table_name_list_syntax(parser,
	                                                 token_index + 1,
	                                                 last_token_index,
	                                                 &next_token_index)) {
		return 0;
	}

	while (next_token_index <= last_token_index) {
		if (token_text_equals(parser, next_token_index, "FOR")) {
			if (next_token_index + 1 > last_token_index ||
			    !token_text_equals(parser, next_token_index + 1, "UPGRADE")) {
				return 0;
			}
			next_token_index += 2;
			continue;
		}
		if (!token_is_check_table_option(parser, next_token_index)) {
			return 0;
		}
		next_token_index++;
	}
	return 1;
}

static int validate_checksum_statement_syntax(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index)
{
	size_t next_token_index;

	if (!token_is_maintenance_table_keyword(parser, token_index) ||
	    !validate_maintenance_table_name_list_syntax(parser,
	                                                 token_index + 1,
	                                                 last_token_index,
	                                                 &next_token_index)) {
		return 0;
	}
	if (next_token_index > last_token_index) {
		return 1;
	}
	return next_token_index == last_token_index &&
	       token_is_checksum_table_option(parser, next_token_index);
}

static int validate_optimize_statement_syntax(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index)
{
	size_t next_token_index;

	return token_is_maintenance_table_keyword(parser, token_index) &&
	       validate_maintenance_table_name_list_syntax(parser,
	                                                   token_index + 1,
	                                                   last_token_index,
	                                                   &next_token_index) &&
	       next_token_index > last_token_index;
}

static int validate_repair_statement_syntax(const mylite_parser *parser,
                                            size_t token_index,
                                            size_t last_token_index)
{
	size_t next_token_index;

	if (!token_is_maintenance_table_keyword(parser, token_index) ||
	    !validate_maintenance_table_name_list_syntax(parser,
	                                                 token_index + 1,
	                                                 last_token_index,
	                                                 &next_token_index)) {
		return 0;
	}

	while (next_token_index <= last_token_index) {
		if (!token_is_repair_table_option(parser, next_token_index)) {
			return 0;
		}
		next_token_index++;
	}
	return 1;
}

static int validate_maintenance_table_name_list_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index)
{
	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		size_t last_name_token;

		if (!token_can_start_maintenance_table_name(parser, token_index)) {
			return 0;
		}
		last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
		token_index = last_name_token + 1;
		if (token_index > last_token_index ||
		    token_text_equals(parser, token_index, "UPDATE") ||
		    token_text_equals(parser, token_index, "DROP") ||
		    token_text_equals(parser, token_index, "FOR") ||
		    token_is_check_table_option(parser, token_index) ||
		    token_is_checksum_table_option(parser, token_index) ||
		    token_is_repair_table_option(parser, token_index)) {
			break;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index ||
		    token_text_equals(parser, token_index, "UPDATE") ||
		    token_text_equals(parser, token_index, "DROP") ||
		    token_text_equals(parser, token_index, "FOR") ||
		    token_is_check_table_option(parser, token_index) ||
		    token_is_checksum_table_option(parser, token_index) ||
		    token_is_repair_table_option(parser, token_index)) {
			return 0;
		}
	}

	*next_token_index = token_index;
	return 1;
}

static int token_can_start_maintenance_table_name(const mylite_parser *parser, size_t token_index)
{
	if (token_index >= parser->token_count) {
		return 0;
	}
	return token_can_continue_object_name(&parser->tokens[token_index]);
}

static int token_is_maintenance_table_keyword(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (parser->tokens[token_index].parser_token == TABLE_T ||
	        token_text_equals(parser, token_index, "TABLES"));
}

static int token_is_check_table_option(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "QUICK") ||
	       token_text_equals(parser, token_index, "FAST") ||
	       token_text_equals(parser, token_index, "MEDIUM") ||
	       token_text_equals(parser, token_index, "EXTENDED") ||
	       token_text_equals(parser, token_index, "CHANGED");
}

static int token_is_checksum_table_option(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "QUICK") ||
	       token_text_equals(parser, token_index, "EXTENDED");
}

static int token_is_repair_table_option(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "QUICK") ||
	       token_text_equals(parser, token_index, "EXTENDED") ||
	       token_text_equals(parser, token_index, "USE_FRM");
}

static int validate_savepoint_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	return token_index == last_token_index &&
	       token_can_continue_object_name(&parser->tokens[token_index]);
}

static int validate_release_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	return token_index + 1 == last_token_index &&
	       token_index < parser->token_count &&
	       parser->tokens[token_index].parser_token == SAVEPOINT_T &&
	       token_can_continue_object_name(&parser->tokens[token_index + 1]);
}

static int validate_commit_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	return validate_transaction_completion_clause(parser, token_index + 1, statement->last_token - 1);
}

static int validate_rollback_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t body_token_index;
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	body_token_index = token_index + 1;
	last_token_index = statement->last_token - 1;
	if (body_token_index <= last_token_index && token_text_equals(parser, body_token_index, "WORK")) {
		body_token_index++;
	}
	if (body_token_index <= last_token_index &&
	    body_token_index < parser->token_count &&
	    parser->tokens[body_token_index].parser_token == TO_T) {
		return validate_rollback_savepoint_clause(parser, body_token_index, last_token_index);
	}

	return validate_transaction_completion_clause(parser, token_index + 1, last_token_index);
}

static int validate_rollback_savepoint_clause(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index)
{
	if (token_index >= parser->token_count ||
	    token_index > last_token_index ||
	    parser->tokens[token_index].parser_token != TO_T) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    parser->tokens[token_index].parser_token == SAVEPOINT_T) {
		token_index++;
	}

	return token_index == last_token_index &&
	       token_can_continue_object_name(&parser->tokens[token_index]);
}

static int validate_principal_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;
	size_t marker_token_index;
	int is_grant = statement->kind == MYLITE_STATEMENT_GRANT;
	int marker_token = is_grant ? TO_T : FROM_T;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (!is_grant &&
	    token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "IF") &&
	    token_text_equals(parser, token_index + 1, "EXISTS")) {
		token_index += 2;
	}
	if (token_index > last_token_index) {
		return 0;
	}

	marker_token_index = find_principal_statement_marker(parser,
	                                                     token_index,
	                                                     last_token_index,
	                                                     marker_token);
	if (marker_token_index >= parser->token_count ||
	    marker_token_index > last_token_index ||
	    marker_token_index == token_index) {
		return 0;
	}

	token_index = marker_token_index + 1;
	if (!validate_principal_target_list_syntax(parser,
	                                           token_index,
	                                           last_token_index,
	                                           is_grant,
	                                           &token_index)) {
		return 0;
	}

	return validate_principal_statement_tail_syntax(parser, token_index, last_token_index, is_grant);
}

static size_t find_principal_statement_marker(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index,
                                              int marker_token)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (parser->tokens[token_index].parser_token == marker_token) {
			return token_index;
		}
		if (matching_token > token_index + 1) {
			token_index = matching_token;
		} else {
			token_index++;
		}
	}
	return parser->token_count;
}

static int validate_principal_target_list_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 int is_grant,
                                                 size_t *next_token_index)
{
	int saw_name = 0;

	while (token_index <= last_token_index) {
		if (token_starts_principal_statement_tail(parser, token_index, is_grant)) {
			break;
		}
		if (!validate_principal_name_syntax(parser, token_index, last_token_index, 1, &token_index)) {
			return 0;
		}
		saw_name = 1;

		if (token_index > last_token_index ||
		    token_starts_principal_statement_tail(parser, token_index, is_grant)) {
			break;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index ||
		    token_starts_principal_statement_tail(parser, token_index, is_grant)) {
			return 0;
		}
	}

	*next_token_index = token_index;
	return saw_name;
}

static int validate_principal_statement_tail_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    int is_grant)
{
	if (token_index > last_token_index) {
		return 1;
	}

	if (is_grant && token_text_equals(parser, token_index, "WITH")) {
		return token_index + 2 == last_token_index &&
		       ((parser->tokens[token_index + 1].parser_token == GRANT_T &&
		         token_text_equals(parser, token_index + 2, "OPTION")) ||
		        (token_text_equals(parser, token_index + 1, "ADMIN") &&
		         token_text_equals(parser, token_index + 2, "OPTION")));
	}

	if (is_grant && parser->tokens[token_index].parser_token == AS_T) {
		token_index++;
		if (!validate_principal_name_syntax(parser, token_index, last_token_index, 1, &token_index)) {
			return 0;
		}
		if (token_index > last_token_index) {
			return 1;
		}
		return token_index + 2 <= last_token_index &&
		       token_text_equals(parser, token_index, "WITH") &&
		       parser->tokens[token_index + 1].parser_token == ROLE_T;
	}

	if (!is_grant && parser->tokens[token_index].parser_token == IGNORE_T) {
		return token_index + 2 == last_token_index &&
		       token_text_equals(parser, token_index + 1, "UNKNOWN") &&
		       parser->tokens[token_index + 2].parser_token == USER_T;
	}

	return 0;
}

static int token_starts_principal_statement_tail(const mylite_parser *parser,
                                                 size_t token_index,
                                                 int is_grant)
{
	if (token_index >= parser->token_count) {
		return 0;
	}
	if (is_grant) {
		return token_text_equals(parser, token_index, "WITH") ||
		       parser->tokens[token_index].parser_token == AS_T;
	}
	return parser->tokens[token_index].parser_token == IGNORE_T;
}

static int validate_start_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    parser->tokens[token_index].parser_token == TRANSACTION_T) {
		return validate_start_transaction_statement_syntax(parser, token_index + 1, last_token_index);
	}
	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    (token_text_equals(parser, token_index, "REPLICA") ||
	     token_text_equals(parser, token_index, "SLAVE"))) {
		return validate_start_replica_statement_syntax(parser, token_index + 1, last_token_index);
	}
	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    token_text_equals(parser, token_index, "GROUP_REPLICATION")) {
		return validate_start_group_replication_statement_syntax(parser, token_index + 1, last_token_index);
	}
	if (token_index > last_token_index) {
		return 0;
	}

	return 1;
}

static int validate_start_transaction_statement_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index)
{
	int seen_snapshot = 0;
	int seen_read_mode = 0;

	if (token_index > last_token_index) {
		return 1;
	}

	while (token_index <= last_token_index) {
		if (!validate_start_transaction_characteristic_syntax(parser,
		                                                      token_index,
		                                                      last_token_index,
		                                                      &token_index,
		                                                      &seen_snapshot,
		                                                      &seen_read_mode)) {
			return 0;
		}
		if (token_index > last_token_index) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}
	return 1;
}

static int validate_start_transaction_characteristic_syntax(const mylite_parser *parser,
                                                            size_t token_index,
                                                            size_t last_token_index,
                                                            size_t *next_token_index,
                                                            int *seen_snapshot,
                                                            int *seen_read_mode)
{
	if (token_index + 2 <= last_token_index &&
	    token_text_equals(parser, token_index, "WITH") &&
	    token_text_equals(parser, token_index + 1, "CONSISTENT") &&
	    token_text_equals(parser, token_index + 2, "SNAPSHOT")) {
		if (*seen_snapshot) {
			return 0;
		}
		*seen_snapshot = 1;
		*next_token_index = token_index + 3;
		return 1;
	}

	if (token_index + 1 <= last_token_index &&
	    parser->tokens[token_index].parser_token == READ_T &&
	    (parser->tokens[token_index + 1].parser_token == WRITE_T ||
	     token_text_equals(parser, token_index + 1, "ONLY"))) {
		if (*seen_read_mode) {
			return 0;
		}
		*seen_read_mode = 1;
		*next_token_index = token_index + 2;
		return 1;
	}

	return 0;
}

static int validate_start_replica_statement_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index)
{
	int seen_io_thread = 0;
	int seen_sql_thread = 0;
	int seen_user = 0;
	int seen_password = 0;
	int seen_default_auth = 0;
	int seen_plugin_dir = 0;
	int seen_connection_option = 0;

	if (!validate_replication_thread_type_list_syntax(parser,
	                                                  token_index,
	                                                  last_token_index,
	                                                  &token_index,
	                                                  &seen_io_thread,
	                                                  &seen_sql_thread)) {
		return 0;
	}

	if (token_index <= last_token_index &&
	    token_text_equals(parser, token_index, "UNTIL")) {
		if (!validate_start_replica_until_clause_syntax(parser,
		                                                token_index,
		                                                last_token_index,
		                                                &token_index)) {
			return 0;
		}
	}

	while (token_index <= last_token_index &&
	       token_is_start_replica_connection_option(parser, token_index)) {
		seen_connection_option = 1;
		if (!validate_start_replica_connection_option_syntax(parser,
		                                                     token_index,
		                                                     last_token_index,
		                                                     &token_index,
		                                                     &seen_user,
		                                                     &seen_password,
		                                                     &seen_default_auth,
		                                                     &seen_plugin_dir)) {
			return 0;
		}
	}

	if (seen_password && !seen_user) {
		return 0;
	}
	if (seen_connection_option && seen_sql_thread && !seen_io_thread) {
		return 0;
	}

	if (token_index <= last_token_index &&
	    validate_replication_channel_clause_syntax(parser,
	                                               token_index,
	                                               last_token_index,
	                                               &token_index)) {
		return token_index > last_token_index;
	}

	return token_index > last_token_index;
}

static int validate_start_replica_until_clause_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index,
                                                      size_t *next_token_index)
{
	if (token_index + 1 > last_token_index ||
	    !token_text_equals(parser, token_index, "UNTIL")) {
		return 0;
	}

	token_index++;
	if (token_text_equals(parser, token_index, "SQL_AFTER_MTS_GAPS")) {
		*next_token_index = token_index + 1;
		return 1;
	}
	if (token_text_equals(parser, token_index, "SOURCE_LOG_FILE")) {
		return validate_start_replica_log_position_until_clause_syntax(parser,
		                                                               token_index,
		                                                               last_token_index,
		                                                               "SOURCE_LOG_FILE",
		                                                               "SOURCE_LOG_POS",
		                                                               next_token_index);
	}
	if (token_text_equals(parser, token_index, "MASTER_LOG_FILE")) {
		return validate_start_replica_log_position_until_clause_syntax(parser,
		                                                               token_index,
		                                                               last_token_index,
		                                                               "MASTER_LOG_FILE",
		                                                               "MASTER_LOG_POS",
		                                                               next_token_index);
	}
	if (token_text_equals(parser, token_index, "RELAY_LOG_FILE")) {
		return validate_start_replica_log_position_until_clause_syntax(parser,
		                                                               token_index,
		                                                               last_token_index,
		                                                               "RELAY_LOG_FILE",
		                                                               "RELAY_LOG_POS",
		                                                               next_token_index);
	}
	if (token_text_equals(parser, token_index, "SQL_BEFORE_GTIDS") ||
	    token_text_equals(parser, token_index, "SQL_AFTER_GTIDS")) {
		return validate_start_replica_gtid_until_clause_syntax(parser,
		                                                       token_index,
		                                                       last_token_index,
		                                                       next_token_index);
	}

	return 0;
}

static int validate_start_replica_log_position_until_clause_syntax(const mylite_parser *parser,
                                                                   size_t token_index,
                                                                   size_t last_token_index,
                                                                   const char *log_file_option,
                                                                   const char *log_pos_option,
                                                                   size_t *next_token_index)
{
	if (token_index + 6 > last_token_index ||
	    !token_text_equals(parser, token_index, log_file_option) ||
	    !token_text_equals(parser, token_index + 1, "=") ||
	    parser->tokens[token_index + 2].kind != MYLITE_TOKEN_STRING ||
	    parser->tokens[token_index + 3].parser_token != ',' ||
	    !token_text_equals(parser, token_index + 4, log_pos_option) ||
	    !token_text_equals(parser, token_index + 5, "=") ||
	    parser->tokens[token_index + 6].kind != MYLITE_TOKEN_NUMBER) {
		return 0;
	}
	*next_token_index = token_index + 7;
	return 1;
}

static int validate_start_replica_gtid_until_clause_syntax(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index,
                                                           size_t *next_token_index)
{
	size_t value_token_index;

	if (token_index + 2 > last_token_index ||
	    !token_text_equals(parser, token_index + 1, "=")) {
		return 0;
	}

	value_token_index = token_index + 2;
	if (token_is_start_replica_tail_boundary(parser, value_token_index) ||
	    parser->tokens[value_token_index].parser_token == ',') {
		return 0;
	}

	token_index = value_token_index;
	if (parser->tokens[token_index].kind == MYLITE_TOKEN_STRING) {
		token_index++;
	} else {
		while (token_index <= last_token_index &&
		       !token_is_start_replica_tail_boundary(parser, token_index)) {
			if (!token_is_start_replica_gtid_set_token(parser, token_index)) {
				return 0;
			}
			token_index++;
		}
	}
	if (parser->tokens[token_index - 1].parser_token == ',') {
		return 0;
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_start_replica_connection_option_syntax(const mylite_parser *parser,
                                                           size_t token_index,
                                                           size_t last_token_index,
                                                           size_t *next_token_index,
                                                           int *seen_user,
                                                           int *seen_password,
                                                           int *seen_default_auth,
                                                           int *seen_plugin_dir)
{
	int *seen_option;

	if (!token_is_start_replica_connection_option(parser, token_index) ||
	    token_index + 2 > last_token_index ||
	    !token_text_equals(parser, token_index + 1, "=") ||
	    parser->tokens[token_index + 2].kind != MYLITE_TOKEN_STRING) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "USER")) {
		seen_option = seen_user;
		if (token_text_equals(parser, token_index + 2, "''")) {
			return 0;
		}
	} else if (token_text_equals(parser, token_index, "PASSWORD")) {
		seen_option = seen_password;
	} else if (token_text_equals(parser, token_index, "DEFAULT_AUTH")) {
		seen_option = seen_default_auth;
	} else {
		seen_option = seen_plugin_dir;
	}

	if (*seen_option) {
		return 0;
	}
	*seen_option = 1;
	*next_token_index = token_index + 3;
	return 1;
}

static int token_is_start_replica_tail_boundary(const mylite_parser *parser, size_t token_index)
{
	if (token_index >= parser->token_count) {
		return 0;
	}
	return token_is_start_replica_connection_option(parser, token_index) ||
	       token_text_equals(parser, token_index, "FOR");
}

static int token_is_start_replica_gtid_set_token(const mylite_parser *parser, size_t token_index)
{
	const mylite_token *token;
	size_t offset;

	if (token_index >= parser->token_count) {
		return 0;
	}

	token = &parser->tokens[token_index];
	for (offset = token->start_offset; offset < token->end_offset; offset++) {
		char ch = parser->lexer.input[offset];

		if ((ch >= '0' && ch <= '9') ||
		    (ch >= 'a' && ch <= 'f') ||
		    (ch >= 'A' && ch <= 'F') ||
		    ch == '-' ||
		    ch == ':' ||
		    ch == ',') {
			continue;
		}
		return 0;
	}
	return token->start_offset < token->end_offset;
}

static int token_is_start_replica_connection_option(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (token_text_equals(parser, token_index, "USER") ||
	        token_text_equals(parser, token_index, "PASSWORD") ||
	        token_text_equals(parser, token_index, "DEFAULT_AUTH") ||
	        token_text_equals(parser, token_index, "PLUGIN_DIR"));
}

static int validate_start_group_replication_statement_syntax(const mylite_parser *parser,
                                                             size_t token_index,
                                                             size_t last_token_index)
{
	if (token_index > last_token_index) {
		return 1;
	}

	if (!validate_start_group_replication_connection_option_syntax(parser,
	                                                               token_index,
	                                                               last_token_index,
	                                                               "USER",
	                                                               &token_index)) {
		return 0;
	}

	if (token_index > last_token_index) {
		return 1;
	}
	if (parser->tokens[token_index].parser_token != ',') {
		return 0;
	}
	token_index++;
	if (token_index > last_token_index) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "PASSWORD")) {
		if (!validate_start_group_replication_connection_option_syntax(parser,
		                                                               token_index,
		                                                               last_token_index,
		                                                               "PASSWORD",
		                                                               &token_index)) {
			return 0;
		}
		if (token_index > last_token_index) {
			return 1;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}

	return validate_start_group_replication_connection_option_syntax(parser,
	                                                                 token_index,
	                                                                 last_token_index,
	                                                                 "DEFAULT_AUTH",
	                                                                 &token_index) &&
	       token_index > last_token_index;
}

static int validate_start_group_replication_connection_option_syntax(const mylite_parser *parser,
                                                                     size_t token_index,
                                                                     size_t last_token_index,
                                                                     const char *option,
                                                                     size_t *next_token_index)
{
	if (token_index + 2 > last_token_index ||
	    !token_text_equals(parser, token_index, option) ||
	    !token_text_equals(parser, token_index + 1, "=") ||
	    parser->tokens[token_index + 2].kind != MYLITE_TOKEN_STRING) {
		return 0;
	}
	if (token_text_equals(parser, token_index, "USER") &&
	    token_text_equals(parser, token_index + 2, "''")) {
		return 0;
	}
	*next_token_index = token_index + 3;
	return 1;
}

static int validate_stop_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "GROUP_REPLICATION")) {
		return token_index == last_token_index;
	}
	if (token_text_equals(parser, token_index, "REPLICA") ||
	    token_text_equals(parser, token_index, "SLAVE")) {
		return validate_stop_replica_statement_syntax(parser, token_index + 1, last_token_index);
	}

	return 0;
}

static int validate_stop_replica_statement_syntax(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	int seen_io_thread = 0;
	int seen_sql_thread = 0;

	if (!validate_replication_thread_type_list_syntax(parser,
	                                                  token_index,
	                                                  last_token_index,
	                                                  &token_index,
	                                                  &seen_io_thread,
	                                                  &seen_sql_thread)) {
		return 0;
	}

	if (token_index > last_token_index) {
		return 1;
	}
	return validate_replication_channel_clause_syntax(parser,
	                                                  token_index,
	                                                  last_token_index,
	                                                  &token_index) &&
	       token_index > last_token_index;
}

static int validate_replication_thread_type_list_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index,
                                                        size_t *next_token_index,
                                                        int *seen_io_thread,
                                                        int *seen_sql_thread)
{
	*seen_io_thread = 0;
	*seen_sql_thread = 0;

	while (token_index <= last_token_index && token_is_replication_thread_type(parser, token_index)) {
		if (token_text_equals(parser, token_index, "IO_THREAD")) {
			if (*seen_io_thread) {
				return 0;
			}
			*seen_io_thread = 1;
		} else {
			if (*seen_sql_thread) {
				return 0;
			}
			*seen_sql_thread = 1;
		}
		token_index++;
		if (token_index > last_token_index ||
		    token_text_equals(parser, token_index, "UNTIL") ||
		    token_is_start_replica_connection_option(parser, token_index) ||
		    token_text_equals(parser, token_index, "FOR")) {
			break;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index || !token_is_replication_thread_type(parser, token_index)) {
			return 0;
		}
	}

	*next_token_index = token_index;
	return 1;
}

static int validate_replication_channel_clause_syntax(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index,
                                                      size_t *next_token_index)
{
	if (token_index + 2 > last_token_index ||
	    !token_text_equals(parser, token_index, "FOR") ||
	    !token_text_equals(parser, token_index + 1, "CHANNEL") ||
	    !token_can_start_object_name(&parser->tokens[token_index + 2])) {
		return 0;
	}
	*next_token_index = token_index + 3;
	return 1;
}

static int token_is_replication_thread_type(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (token_text_equals(parser, token_index, "IO_THREAD") ||
	        token_text_equals(parser, token_index, "SQL_THREAD"));
}

static int validate_transaction_completion_clause(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	int no_chain = 0;

	if (token_index > last_token_index) {
		return 1;
	}

	if (token_index < parser->token_count && token_text_equals(parser, token_index, "WORK")) {
		token_index++;
	}
	if (token_index > last_token_index) {
		return 1;
	}

	if (token_index < parser->token_count && parser->tokens[token_index].parser_token == AND_T) {
		token_index++;
		if (token_index <= last_token_index &&
		    token_index < parser->token_count &&
		    parser->tokens[token_index].parser_token == NO_T) {
			no_chain = 1;
			token_index++;
		}
		if (token_index > last_token_index ||
		    token_index >= parser->token_count ||
		    parser->tokens[token_index].parser_token != CHAIN_T) {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 1;
		}
		if (token_index < parser->token_count &&
		    parser->tokens[token_index].parser_token == NO_T) {
			token_index++;
			if (token_index > last_token_index ||
			    token_index >= parser->token_count ||
			    parser->tokens[token_index].parser_token != RELEASE_T) {
				return 0;
			}
			token_index++;
			return token_index > last_token_index;
		}
		if (no_chain &&
		    token_index < parser->token_count &&
		    parser->tokens[token_index].parser_token == RELEASE_T) {
			token_index++;
			return token_index > last_token_index;
		}
		return 0;
	}

	if (token_index < parser->token_count && parser->tokens[token_index].parser_token == NO_T) {
		token_index++;
		if (token_index > last_token_index ||
		    token_index >= parser->token_count ||
		    parser->tokens[token_index].parser_token != RELEASE_T) {
			return 0;
		}
		token_index++;
		return token_index > last_token_index;
	}
	if (token_index < parser->token_count && parser->tokens[token_index].parser_token == RELEASE_T) {
		token_index++;
		return token_index > last_token_index;
	}

	return 0;
}

static int validate_prepare_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	last_token_index = statement->last_token - 1;
	return token_index + 3 == last_token_index &&
	       token_can_continue_object_name(&parser->tokens[token_index + 1]) &&
	       parser->tokens[token_index + 2].parser_token == FROM_T &&
	       (parser->tokens[token_index + 3].kind == MYLITE_TOKEN_STRING ||
	        parser->tokens[token_index + 3].kind == MYLITE_TOKEN_USER_VARIABLE);
}

static int validate_execute_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index ||
	    !token_can_continue_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index++;
	if (token_index > last_token_index) {
		return 1;
	}
	if (token_index >= parser->token_count || parser->tokens[token_index].parser_token != USING_T) {
		return 0;
	}

	token_index++;
	while (token_index <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].kind != MYLITE_TOKEN_USER_VARIABLE) {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 1;
		}
		if (token_index >= parser->token_count || parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
	}
	return 0;
}

static int validate_deallocate_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	last_token_index = statement->last_token - 1;
	return token_index + 2 == last_token_index &&
	       parser->tokens[token_index + 1].parser_token == PREPARE_T &&
	       token_can_continue_object_name(&parser->tokens[token_index + 2]);
}

static int validate_drop_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	last_token_index = statement->last_token - 1;
	if (token_index + 1 > last_token_index) {
		return 0;
	}
	if (parser->tokens[token_index + 1].parser_token == PREPARE_T) {
		return validate_drop_prepare_statement_syntax(parser, statement);
	}
	if (parser->tokens[token_index + 1].parser_token == DATABASE_T ||
	    parser->tokens[token_index + 1].parser_token == SCHEMA_T) {
		return validate_drop_database_statement_syntax(parser, token_index + 1, last_token_index);
	}
	if (token_is_drop_stored_object_token(parser->tokens[token_index + 1].parser_token)) {
		return validate_drop_stored_object_statement_syntax(parser, token_index + 1, last_token_index);
	}
	if (token_text_equals(parser, token_index + 1, "SERVER")) {
		return validate_drop_server_statement_syntax(parser, token_index + 1, last_token_index);
	}
	if (token_is_spatial_reference_system_sequence(parser, token_index + 1, last_token_index)) {
		return validate_drop_spatial_reference_system_statement_syntax(parser, token_index + 1, last_token_index);
	}
	if (token_is_undo_tablespace_sequence(parser, token_index + 1, last_token_index)) {
		return validate_drop_tablespace_statement_syntax(parser, token_index + 1, last_token_index, 1);
	}
	if (token_is_tablespace_token(parser, token_index + 1)) {
		return validate_drop_tablespace_statement_syntax(parser, token_index + 1, last_token_index, 0);
	}
	if (token_is_logfile_group_sequence(parser, token_index + 1, last_token_index)) {
		return validate_drop_logfile_group_statement_syntax(parser, token_index + 1, last_token_index);
	}
	if (parser->tokens[token_index + 1].parser_token == USER_T) {
		return validate_drop_principal_statement_syntax(parser, token_index + 1, last_token_index, 1);
	}
	if (parser->tokens[token_index + 1].parser_token == ROLE_T) {
		return validate_drop_principal_statement_syntax(parser, token_index + 1, last_token_index, 0);
	}
	if (token_is_drop_resource_group_token(parser, token_index + 1, last_token_index)) {
		return validate_drop_resource_group_statement_syntax(parser, token_index + 1, last_token_index);
	}
	if (parser->tokens[token_index + 1].parser_token == INDEX_T) {
		return validate_drop_index_statement_syntax(parser, token_index + 1, last_token_index);
	}
	if (parser->tokens[token_index + 1].parser_token == TEMPORARY_T) {
		if (token_index + 2 > last_token_index ||
		    !token_is_drop_table_token(parser, token_index + 2)) {
			return 0;
		}
		return validate_drop_table_or_view_statement_syntax(parser, token_index + 2, last_token_index);
	}
	if (token_is_drop_table_or_view_token(parser, token_index + 1)) {
		return validate_drop_table_or_view_statement_syntax(parser, token_index + 1, last_token_index);
	}
	return 1;
}

static int validate_drop_prepare_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	last_token_index = statement->last_token - 1;
	if (token_index + 1 > last_token_index ||
	    parser->tokens[token_index + 1].parser_token != PREPARE_T) {
		return 1;
	}
	return token_index + 2 == last_token_index &&
	       token_can_continue_object_name(&parser->tokens[token_index + 2]);
}

static int validate_drop_database_statement_syntax(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index)
{
	if (token_index >= parser->token_count ||
	    (parser->tokens[token_index].parser_token != DATABASE_T &&
	     parser->tokens[token_index].parser_token != SCHEMA_T)) {
		return 0;
	}

	token_index++;
	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "IF") &&
	    token_text_equals(parser, token_index + 1, "EXISTS")) {
		token_index += 2;
	}

	return token_index == last_token_index &&
	       token_can_continue_object_name(&parser->tokens[token_index]);
}

static int validate_drop_stored_object_statement_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index)
{
	size_t last_name_token;

	if (token_index >= parser->token_count ||
	    !token_is_drop_stored_object_token(parser->tokens[token_index].parser_token)) {
		return 0;
	}

	token_index++;
	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "IF") &&
	    token_text_equals(parser, token_index + 1, "EXISTS")) {
		token_index += 2;
	}
	if (token_index > last_token_index ||
	    !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}

	last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
	return last_name_token == last_token_index;
}

static int validate_drop_server_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index)
{
	if (token_index >= parser->token_count || !token_text_equals(parser, token_index, "SERVER")) {
		return 0;
	}

	token_index++;
	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "IF") &&
	    token_text_equals(parser, token_index + 1, "EXISTS")) {
		token_index += 2;
	}

	return token_index == last_token_index &&
	       token_can_start_object_name(&parser->tokens[token_index]);
}

static int validate_drop_spatial_reference_system_statement_syntax(const mylite_parser *parser,
                                                                   size_t token_index,
                                                                   size_t last_token_index)
{
	if (!token_is_spatial_reference_system_sequence(parser, token_index, last_token_index)) {
		return 0;
	}

	token_index += 3;
	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "IF") &&
	    token_text_equals(parser, token_index + 1, "EXISTS")) {
		token_index += 2;
	}

	return token_index == last_token_index &&
	       token_index < parser->token_count &&
	       parser->tokens[token_index].kind == MYLITE_TOKEN_NUMBER;
}

static int validate_drop_tablespace_statement_syntax(const mylite_parser *parser,
                                                     size_t token_index,
                                                     size_t last_token_index,
                                                     int is_undo_tablespace)
{
	if (is_undo_tablespace) {
		if (!token_is_undo_tablespace_sequence(parser, token_index, last_token_index)) {
			return 0;
		}
		token_index += 2;
	} else {
		if (!token_is_tablespace_token(parser, token_index)) {
			return 0;
		}
		token_index++;
	}

	if (token_index > last_token_index ||
	    !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index++;

	if (token_index > last_token_index) {
		return 1;
	}
	return validate_storage_engine_tail_syntax(parser, token_index, last_token_index);
}

static int validate_drop_logfile_group_statement_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index)
{
	if (!token_is_logfile_group_sequence(parser, token_index, last_token_index)) {
		return 0;
	}

	token_index += 2;
	if (token_index > last_token_index ||
	    !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index++;

	return validate_storage_engine_tail_syntax(parser, token_index, last_token_index);
}

static int validate_storage_engine_tail_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index)
{
	return validate_storage_engine_clause_syntax(parser,
	                                             token_index,
	                                             last_token_index,
	                                             &token_index) &&
	       token_index > last_token_index;
}

static int validate_storage_engine_clause_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 size_t *next_token_index)
{
	if (token_index > last_token_index ||
	    parser->tokens[token_index].parser_token != ENGINE_T) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
		token_index++;
	}

	if (token_index > last_token_index || !token_is_storage_engine_name(parser, token_index)) {
		return 0;
	}

	*next_token_index = token_index + 1;
	return 1;
}

static int validate_drop_principal_statement_syntax(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index,
                                                    int allow_current_user)
{
	int token;

	if (token_index >= parser->token_count) {
		return 0;
	}
	token = parser->tokens[token_index].parser_token;
	if (token != USER_T && token != ROLE_T) {
		return 0;
	}

	token_index++;
	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "IF") &&
	    token_text_equals(parser, token_index + 1, "EXISTS")) {
		token_index += 2;
	}

	return validate_principal_name_list_syntax(parser,
	                                           token_index,
	                                           last_token_index,
	                                           allow_current_user);
}

static int validate_principal_name_list_syntax(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index,
                                               int allow_current_user)
{
	int saw_name = 0;

	while (token_index <= last_token_index) {
		if (!validate_principal_name_syntax(parser,
		                                    token_index,
		                                    last_token_index,
		                                    allow_current_user,
		                                    &token_index)) {
			return 0;
		}
		saw_name = 1;

		if (token_index > last_token_index) {
			break;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index) {
			return 0;
		}
	}
	return saw_name;
}

static int validate_principal_name_syntax(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index,
                                          int allow_current_user,
                                          size_t *next_token_index)
{
	size_t last_name_token;

	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}
	if (!allow_current_user && token_is_current_user_function_name(parser, token_index)) {
		return 0;
	}
	if (!token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}

	last_name_token = last_account_name_token(parser, token_index, last_token_index);
	if (last_name_token + 1 <= last_token_index &&
	    token_is_account_at_marker(parser, last_name_token + 1)) {
		last_name_token++;
	}

	*next_token_index = last_name_token + 1;
	return 1;
}

static int validate_drop_resource_group_statement_syntax(const mylite_parser *parser,
                                                         size_t token_index,
                                                         size_t last_token_index)
{
	if (!token_is_drop_resource_group_token(parser, token_index, last_token_index)) {
		return 0;
	}

	token_index += 2;
	if (token_index > last_token_index ||
	    !token_can_start_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	token_index++;

	return token_index > last_token_index ||
	       (token_index == last_token_index && token_text_equals(parser, token_index, "FORCE"));
}

static int validate_drop_index_statement_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index)
{
	size_t last_name_token;

	if (token_index >= parser->token_count ||
	    parser->tokens[token_index].parser_token != INDEX_T ||
	    token_index + 3 > last_token_index ||
	    !token_can_continue_object_name(&parser->tokens[token_index + 1]) ||
	    !token_text_equals(parser, token_index + 2, "ON") ||
	    !token_can_continue_object_name(&parser->tokens[token_index + 3])) {
		return 0;
	}

	token_index += 3;
	last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
	token_index = last_name_token + 1;
	while (token_index <= last_token_index) {
		if (!validate_drop_index_option_syntax(parser, token_index, last_token_index, &token_index)) {
			return 0;
		}
	}
	return 1;
}

static int validate_drop_index_option_syntax(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index,
                                             size_t *next_token_index)
{
	int is_algorithm_option = token_text_equals(parser, token_index, "ALGORITHM");
	int is_lock_option = token_text_equals(parser, token_index, "LOCK");

	if (!is_algorithm_option && !is_lock_option) {
		return 0;
	}

	token_index++;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
		token_index++;
	}
	if (token_index > last_token_index) {
		return 0;
	}

	if (is_algorithm_option) {
		if (!token_is_drop_index_algorithm_value(parser, token_index)) {
			return 0;
		}
	} else if (!token_is_drop_index_lock_value(parser, token_index)) {
		return 0;
	}

	*next_token_index = token_index + 1;
	return 1;
}

static int validate_drop_table_or_view_statement_syntax(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index)
{
	if (!token_is_drop_table_or_view_token(parser, token_index)) {
		return 0;
	}

	token_index++;
	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "IF") &&
	    token_text_equals(parser, token_index + 1, "EXISTS")) {
		token_index += 2;
	}

	if (!validate_drop_object_name_list_syntax(parser, token_index, last_token_index, &token_index)) {
		return 0;
	}
	if (token_index > last_token_index) {
		return 1;
	}
	return token_index == last_token_index &&
	       token_is_drop_table_tail_option(parser, token_index);
}

static int validate_drop_object_name_list_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index,
                                                 size_t *next_token_index)
{
	int saw_name = 0;

	if (token_index > last_token_index) {
		return 0;
	}

	while (token_index <= last_token_index) {
		size_t last_name_token;

		if (token_is_drop_table_tail_option(parser, token_index)) {
			break;
		}
		if (token_index >= parser->token_count ||
		    !token_can_start_object_name(&parser->tokens[token_index])) {
			return 0;
		}

		last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
		saw_name = 1;
		token_index = last_name_token + 1;
		if (token_index > last_token_index || token_is_drop_table_tail_option(parser, token_index)) {
			break;
		}
		if (parser->tokens[token_index].parser_token != ',') {
			return 0;
		}
		token_index++;
		if (token_index > last_token_index ||
		    token_is_drop_table_tail_option(parser, token_index)) {
			return 0;
		}
	}

	*next_token_index = token_index;
	return saw_name;
}

static int token_is_drop_index_algorithm_value(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (parser->tokens[token_index].parser_token == DEFAULT_T ||
	        token_text_equals(parser, token_index, "INPLACE") ||
	        token_text_equals(parser, token_index, "COPY"));
}

static int token_is_drop_index_lock_value(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (parser->tokens[token_index].parser_token == DEFAULT_T ||
	        token_text_equals(parser, token_index, "NONE") ||
	        token_text_equals(parser, token_index, "SHARED") ||
	        token_text_equals(parser, token_index, "EXCLUSIVE"));
}

static int token_is_storage_engine_name(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       token_can_continue_qualified_object_name(&parser->tokens[token_index]);
}

static int token_is_logfile_group_sequence(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index)
{
	return token_index + 1 <= last_token_index &&
	       token_index + 1 < parser->token_count &&
	       token_text_equals(parser, token_index, "LOGFILE") &&
	       parser->tokens[token_index + 1].parser_token == GROUP_T;
}

static int token_is_spatial_reference_system_sequence(const mylite_parser *parser,
                                                      size_t token_index,
                                                      size_t last_token_index)
{
	return token_index + 2 <= last_token_index &&
	       token_index + 2 < parser->token_count &&
	       parser->tokens[token_index].parser_token == SPATIAL_T &&
	       token_text_equals(parser, token_index + 1, "REFERENCE") &&
	       token_text_equals(parser, token_index + 2, "SYSTEM");
}

static int token_is_drop_stored_object_token(int token)
{
	return token == EVENT_T ||
	       token == FUNCTION_T ||
	       token == PROCEDURE_T ||
	       token == TRIGGER_T;
}

static int token_is_drop_resource_group_token(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index)
{
	return token_index + 1 <= last_token_index &&
	       token_index + 1 < parser->token_count &&
	       token_text_equals(parser, token_index, "RESOURCE") &&
	       parser->tokens[token_index + 1].parser_token == GROUP_T;
}

static int token_is_tablespace_token(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       token_text_equals(parser, token_index, "TABLESPACE");
}

static int token_is_undo_tablespace_sequence(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index)
{
	return token_index + 1 <= last_token_index &&
	       token_index + 1 < parser->token_count &&
	       token_text_equals(parser, token_index, "UNDO") &&
	       token_is_tablespace_token(parser, token_index + 1);
}

static int token_is_drop_table_token(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (parser->tokens[token_index].parser_token == TABLE_T ||
	        token_text_equals(parser, token_index, "TABLES"));
}

static int token_is_drop_table_or_view_token(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (token_is_drop_table_token(parser, token_index) ||
	        parser->tokens[token_index].parser_token == VIEW_T);
}

static int token_is_drop_table_tail_option(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       (token_text_equals(parser, token_index, "RESTRICT") ||
	        token_text_equals(parser, token_index, "CASCADE"));
}

static int validate_single_token_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);

	return token_index < parser->token_count &&
	       statement->last_token >= statement->first_token &&
	       token_index == statement->last_token - 1;
}

static int validate_kill_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "QUERY")) {
		token_index++;
	} else if (token_index <= last_token_index && token_text_equals(parser, token_index, "CONNECTION")) {
		token_index++;
	}

	return token_can_start_processlist_expression(parser, token_index, last_token_index) &&
	       processlist_expression_is_single_target(parser, token_index, last_token_index);
}

static int validate_help_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}
	if (parser->tokens[token_index].kind == MYLITE_TOKEN_STRING) {
		return token_index == last_token_index;
	}
	while (token_index <= last_token_index) {
		if (token_index >= parser->token_count ||
		    (parser->tokens[token_index].kind != MYLITE_TOKEN_IDENTIFIER &&
		     parser->tokens[token_index].kind != MYLITE_TOKEN_QUOTED_IDENTIFIER &&
		     parser->tokens[token_index].kind != MYLITE_TOKEN_KEYWORD)) {
			return 0;
		}
		token_index++;
	}
	return 1;
}

static int validate_clone_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == LOCAL_T) {
		return validate_clone_local_statement_syntax(parser, token_index + 1, last_token_index);
	}

	if (token_text_equals(parser, token_index, "INSTANCE")) {
		return validate_clone_remote_statement_syntax(parser, token_index + 1, last_token_index);
	}

	return 0;
}

static int validate_clone_local_statement_syntax(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index)
{
	size_t next_token_index;

	return validate_clone_data_directory_clause_syntax(parser,
	                                                   token_index,
	                                                   last_token_index,
	                                                   &next_token_index) &&
	       next_token_index > last_token_index;
}

static int validate_clone_remote_statement_syntax(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index)
{
	size_t next_token_index;

	if (token_index > last_token_index ||
	    parser->tokens[token_index].parser_token != FROM_T ||
	    !validate_clone_endpoint_syntax(parser, token_index + 1, last_token_index, &next_token_index)) {
		return 0;
	}

	if (next_token_index + 2 > last_token_index ||
	    !token_text_equals(parser, next_token_index, "IDENTIFIED") ||
	    parser->tokens[next_token_index + 1].parser_token != BY_T ||
	    parser->tokens[next_token_index + 2].kind != MYLITE_TOKEN_STRING) {
		return 0;
	}
	next_token_index += 3;

	if (next_token_index <= last_token_index &&
	    parser->tokens[next_token_index].parser_token == DATA_T) {
		if (!validate_clone_data_directory_clause_syntax(parser,
		                                                 next_token_index,
		                                                 last_token_index,
		                                                 &next_token_index)) {
			return 0;
		}
	}

	if (next_token_index <= last_token_index && token_text_equals(parser, next_token_index, "REQUIRE")) {
		next_token_index++;
		if (next_token_index <= last_token_index && parser->tokens[next_token_index].parser_token == NO_T) {
			next_token_index++;
		}
		if (next_token_index > last_token_index ||
		    !token_text_equals(parser, next_token_index, "SSL")) {
			return 0;
		}
		next_token_index++;
	}

	return next_token_index > last_token_index;
}

static int validate_clone_endpoint_syntax(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index,
                                          size_t *next_token_index)
{
	if (token_index + 3 > last_token_index ||
	    !token_can_be_clone_account_name(parser, token_index) ||
	    parser->tokens[token_index + 1].kind != MYLITE_TOKEN_USER_VARIABLE) {
		return 0;
	}
	token_index += 2;

	if (token_text_equals(parser, token_index - 1, "@")) {
		if (token_index + 2 > last_token_index ||
		    !token_can_be_clone_host_name(parser, token_index)) {
			return 0;
		}
		token_index++;
	}

	if (!token_text_equals(parser, token_index, ":") ||
	    token_index + 1 > last_token_index ||
	    parser->tokens[token_index + 1].kind != MYLITE_TOKEN_NUMBER) {
		return 0;
	}

	*next_token_index = token_index + 2;
	return 1;
}

static int validate_clone_data_directory_clause_syntax(const mylite_parser *parser,
                                                       size_t token_index,
                                                       size_t last_token_index,
                                                       size_t *next_token_index)
{
	if (token_index + 2 > last_token_index ||
	    parser->tokens[token_index].parser_token != DATA_T ||
	    !token_text_equals(parser, token_index + 1, "DIRECTORY")) {
		return 0;
	}

	token_index += 2;
	if (token_index <= last_token_index && token_text_equals(parser, token_index, "=")) {
		token_index++;
	}
	if (token_index > last_token_index ||
	    parser->tokens[token_index].kind != MYLITE_TOKEN_STRING) {
		return 0;
	}

	*next_token_index = token_index + 1;
	return 1;
}

static int token_can_be_clone_account_name(const mylite_parser *parser, size_t token_index)
{
	if (token_index >= parser->token_count) {
		return 0;
	}
	return token_can_continue_object_name(&parser->tokens[token_index]) ||
	       parser->tokens[token_index].kind == MYLITE_TOKEN_STRING;
}

static int token_can_be_clone_host_name(const mylite_parser *parser, size_t token_index)
{
	if (token_index >= parser->token_count) {
		return 0;
	}
	return token_can_continue_object_name(&parser->tokens[token_index]) ||
	       parser->tokens[token_index].kind == MYLITE_TOKEN_STRING ||
	       parser->tokens[token_index].kind == MYLITE_TOKEN_NUMBER;
}

static int validate_xa_statement_syntax(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;
	size_t next_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "RECOVER")) {
		if (token_index == last_token_index) {
			return 1;
		}
		return token_index + 2 == last_token_index &&
		       token_text_equals(parser, token_index + 1, "CONVERT") &&
		       token_text_equals(parser, token_index + 2, "XID");
	}

	if ((parser->tokens[token_index].parser_token == START_T ||
	     parser->tokens[token_index].parser_token == BEGIN_T) &&
	    validate_xa_xid_syntax(parser, token_index + 1, last_token_index, &next_token_index)) {
		if (next_token_index > last_token_index) {
			return 1;
		}
		return next_token_index == last_token_index &&
		       (parser->tokens[next_token_index].parser_token == JOIN_T ||
		        token_text_equals(parser, next_token_index, "RESUME"));
	}

	if (parser->tokens[token_index].parser_token == END_T &&
	    validate_xa_xid_syntax(parser, token_index + 1, last_token_index, &next_token_index)) {
		if (next_token_index > last_token_index) {
			return 1;
		}
		if (!token_text_equals(parser, next_token_index, "SUSPEND")) {
			return 0;
		}
		next_token_index++;
		if (next_token_index > last_token_index) {
			return 1;
		}
		return next_token_index + 1 == last_token_index &&
		       token_text_equals(parser, next_token_index, "FOR") &&
		       token_text_equals(parser, next_token_index + 1, "MIGRATE");
	}

	if ((parser->tokens[token_index].parser_token == PREPARE_T ||
	     parser->tokens[token_index].parser_token == ROLLBACK_T) &&
	    validate_xa_xid_syntax(parser, token_index + 1, last_token_index, &next_token_index)) {
		return next_token_index > last_token_index;
	}

	if (parser->tokens[token_index].parser_token == COMMIT_T &&
	    validate_xa_xid_syntax(parser, token_index + 1, last_token_index, &next_token_index)) {
		if (next_token_index > last_token_index) {
			return 1;
		}
		return next_token_index + 1 == last_token_index &&
		       token_text_equals(parser, next_token_index, "ONE") &&
		       token_text_equals(parser, next_token_index + 1, "PHASE");
	}

	return 0;
}

static int validate_xa_xid_syntax(const mylite_parser *parser,
                                  size_t token_index,
                                  size_t last_token_index,
                                  size_t *next_token_index)
{
	if (token_index > last_token_index || !token_is_xa_string_value(parser, token_index)) {
		return 0;
	}
	token_index++;

	if (token_index > last_token_index || parser->tokens[token_index].parser_token != ',') {
		*next_token_index = token_index;
		return 1;
	}
	token_index++;
	if (token_index > last_token_index || !token_is_xa_string_value(parser, token_index)) {
		return 0;
	}
	token_index++;

	if (token_index > last_token_index || parser->tokens[token_index].parser_token != ',') {
		*next_token_index = token_index;
		return 1;
	}
	token_index++;
	if (token_index > last_token_index || !token_is_xa_format_id(parser, token_index)) {
		return 0;
	}

	*next_token_index = token_index + 1;
	return 1;
}

static int token_is_xa_string_value(const mylite_parser *parser, size_t token_index)
{
	if (token_index >= parser->token_count) {
		return 0;
	}
	return parser->tokens[token_index].kind == MYLITE_TOKEN_STRING ||
	       token_is_xa_prefixed_number_literal(parser, token_index);
}

static int token_is_xa_format_id(const mylite_parser *parser, size_t token_index)
{
	const mylite_token *token;
	const char *text;
	size_t length;

	if (token_index >= parser->token_count ||
	    parser->tokens[token_index].kind != MYLITE_TOKEN_NUMBER ||
	    parser->tokens[token_index].start_offset >= parser->tokens[token_index].end_offset) {
		return 0;
	}
	token = &parser->tokens[token_index];
	text = parser->lexer.input + token->start_offset;
	length = token->end_offset - token->start_offset;
	return text[0] != '-' &&
	       !(length >= 3 &&
	         (text[0] == 'x' || text[0] == 'X' || text[0] == 'b' || text[0] == 'B') &&
	         text[1] == '\'');
}

static int token_is_xa_prefixed_number_literal(const mylite_parser *parser, size_t token_index)
{
	const mylite_token *token;
	const char *text;
	size_t length;

	if (token_index >= parser->token_count ||
	    parser->tokens[token_index].kind != MYLITE_TOKEN_NUMBER) {
		return 0;
	}

	token = &parser->tokens[token_index];
	text = parser->lexer.input + token->start_offset;
	length = token->end_offset - token->start_offset;
	return (length >= 3 &&
	        (text[0] == 'x' || text[0] == 'X' || text[0] == 'b' || text[0] == 'B') &&
	        text[1] == '\'') ||
	       (length >= 3 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'));
}

static void classify_statement_metadata(mylite_parser *parser)
{
	classify_grouped_query_statement_kinds(parser);
	classify_with_statement_kinds(parser);
	classify_labeled_statement_metadata(parser);
	classify_statement_objects(parser);
}

static void classify_grouped_query_statement_kinds(mylite_parser *parser)
{
	size_t i;

	for (i = 0; i < parser->statement_count; i++) {
		parser->statements[i].kind = classify_grouped_query_statement_kind(parser, &parser->statements[i]);
	}
}

static mylite_statement_kind classify_grouped_query_statement_kind(const mylite_parser *parser,
                                                                   const mylite_statement *statement)
{
	size_t token_index;
	size_t last_token_index;
	mylite_statement_kind kind;

	if (statement->kind != MYLITE_STATEMENT_UNKNOWN ||
	    statement->first_token == 0 ||
	    statement->last_token < statement->first_token) {
		return statement->kind;
	}

	token_index = statement->first_token - 1;
	last_token_index = statement->last_token - 1;
	if (token_index >= parser->token_count || parser->tokens[token_index].parser_token != '(') {
		return statement->kind;
	}

	while (token_index <= last_token_index &&
	       token_index < parser->token_count &&
	       parser->tokens[token_index].parser_token == '(') {
		size_t matching_token = parser->tokens[token_index].matching_token;
		if (matching_token == 0 ||
		    matching_token <= token_index + 1 ||
		    matching_token > statement->last_token) {
			return statement->kind;
		}
		token_index++;
	}
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return statement->kind;
	}

	kind = query_statement_kind_from_token(parser->tokens[token_index].parser_token);
	return kind == MYLITE_STATEMENT_UNKNOWN ? statement->kind : kind;
}

static mylite_statement_kind query_statement_kind_from_token(int token)
{
	switch (token) {
	case SELECT_T:
	case WITH_T:
		return MYLITE_STATEMENT_SELECT;
	case TABLE_T:
		return MYLITE_STATEMENT_TABLE;
	case VALUES_T:
		return MYLITE_STATEMENT_VALUES;
	default:
		return MYLITE_STATEMENT_UNKNOWN;
	}
}

static void classify_with_statement_kinds(mylite_parser *parser)
{
	size_t i;

	for (i = 0; i < parser->statement_count; i++) {
		if (parser->statements[i].first_token == 0 ||
		    parser->statements[i].first_token > parser->token_count ||
		    parser->tokens[parser->statements[i].first_token - 1].parser_token != WITH_T) {
			continue;
		}
		parser->statements[i].kind = classify_with_statement_kind(parser, &parser->statements[i]);
	}
}

static mylite_statement_kind classify_with_statement_kind(const mylite_parser *parser,
                                                          const mylite_statement *statement)
{
	size_t token_index = statement->first_token;
	size_t last_token_index;

	if (statement->last_token < statement->first_token) {
		return statement->kind;
	}
	last_token_index = statement->last_token - 1;

	while (token_index <= last_token_index && token_index < parser->token_count) {
		int token = parser->tokens[token_index].parser_token;
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (matching_token > token_index + 1) {
			token_index = matching_token;
			continue;
		}

		switch (token) {
		case SELECT_T: return MYLITE_STATEMENT_SELECT;
		case INSERT_T: return MYLITE_STATEMENT_INSERT;
		case REPLACE_T: return MYLITE_STATEMENT_REPLACE;
		case UPDATE_T: return MYLITE_STATEMENT_UPDATE;
		case DELETE_T: return MYLITE_STATEMENT_DELETE;
		default:
			token_index++;
			break;
		}
	}
	return statement->kind;
}

static void classify_labeled_statement_metadata(mylite_parser *parser)
{
	size_t i;

	for (i = 0; i < parser->statement_count; i++) {
		classify_labeled_statement(parser, &parser->statements[i]);
	}
}

static int classify_labeled_statement(mylite_parser *parser, mylite_statement *statement)
{
	size_t label_token_index;
	size_t separator_token_index;
	size_t head_token_index;
	mylite_statement_kind labeled_kind;

	if (statement->first_token == 0 ||
	    statement->last_token < statement->first_token + 2 ||
	    statement->last_token > parser->token_count) {
		return 0;
	}

	label_token_index = statement->first_token - 1;
	separator_token_index = label_token_index + 1;
	head_token_index = label_token_index + 2;
	if (!token_can_start_label_name(&parser->tokens[label_token_index]) ||
	    !token_text_equals(parser, separator_token_index, ":")) {
		return 0;
	}

	labeled_kind = labeled_statement_kind_from_token(parser->tokens[head_token_index].parser_token);
	if (labeled_kind == MYLITE_STATEMENT_UNKNOWN) {
		return 0;
	}

	statement->kind = labeled_kind;
	statement->object_kind = MYLITE_STATEMENT_OBJECT_LABEL;
	set_statement_object_name_from_first_token(parser, statement, label_token_index, label_token_index);
	return 1;
}

static mylite_statement_kind labeled_statement_kind_from_token(int token)
{
	switch (token) {
	case BEGIN_T: return MYLITE_STATEMENT_BEGIN;
	case LOOP_T: return MYLITE_STATEMENT_LOOP;
	case REPEAT_T: return MYLITE_STATEMENT_REPEAT;
	case WHILE_T: return MYLITE_STATEMENT_WHILE;
	default: return MYLITE_STATEMENT_UNKNOWN;
	}
}

static void classify_statement_objects(mylite_parser *parser)
{
	size_t i;

	for (i = 0; i < parser->statement_count; i++) {
		classify_statement_object(parser, &parser->statements[i]);
	}
}

static void classify_statement_object(const mylite_parser *parser, mylite_statement *statement)
{
	size_t token_index;
	size_t last_token_index;

	if (classify_dml_statement_object(parser, statement)) {
		return;
	}
	if (classify_direct_statement_object(parser, statement)) {
		return;
	}

	if (!statement_kind_uses_object_scan(statement->kind) ||
	    statement->first_token == 0 ||
	    statement->last_token < statement->first_token) {
		return;
	}

	token_index = statement->first_token;
	last_token_index = statement->last_token - 1;
	for (; token_index <= last_token_index && token_index < parser->token_count; token_index++) {
		size_t definer_clause_last_token = last_definer_clause_token(parser, token_index, last_token_index);

		if (definer_clause_last_token > token_index) {
			token_index = definer_clause_last_token;
			continue;
		}

		mylite_statement_object_kind object_kind =
			object_kind_from_token_sequence(parser, token_index, last_token_index);
		if (object_kind != MYLITE_STATEMENT_OBJECT_NONE) {
			statement->object_kind = object_kind;
			set_statement_object_name(parser, statement, token_index, last_token_index);
			return;
		}
	}
}

static size_t last_definer_clause_token(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index)
{
	size_t first_account_token;

	if (!token_text_equals(parser, token_index, "DEFINER") ||
	    token_index + 2 > last_token_index ||
	    !token_is_assignment_operator(parser, token_index + 1)) {
		return token_index;
	}

	first_account_token = token_index + 2;
	if (!token_can_start_definer_account_name(&parser->tokens[first_account_token])) {
		return token_index;
	}

	return last_account_name_token(parser, first_account_token, last_token_index);
}

static int token_can_start_definer_account_name(const mylite_token *token)
{
	return token_can_start_object_name(token) ||
	       token->kind == MYLITE_TOKEN_KEYWORD;
}

static int classify_dml_statement_object(const mylite_parser *parser, mylite_statement *statement)
{
	size_t verb_token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;
	size_t name_token_index;

	if (verb_token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	last_token_index = statement->last_token - 1;
	switch (statement->kind) {
	case MYLITE_STATEMENT_INSERT:
	case MYLITE_STATEMENT_REPLACE:
		name_token_index = find_insert_or_replace_name_token(parser, verb_token_index + 1, last_token_index);
		break;
	case MYLITE_STATEMENT_UPDATE:
		name_token_index = find_update_name_token(parser, verb_token_index + 1, last_token_index);
		break;
	case MYLITE_STATEMENT_DELETE:
		name_token_index = find_delete_name_token(parser, verb_token_index + 1, last_token_index);
		break;
	default:
		return 0;
	}

	if (name_token_index >= parser->token_count) {
		return 0;
	}

	statement->object_kind = MYLITE_STATEMENT_OBJECT_TABLE;
	set_statement_object_name_from_first_token(parser, statement, name_token_index, last_token_index);
	return 1;
}

static size_t find_statement_kind_token(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index;
	size_t last_token_index;
	int desired_token = 0;

	if (statement->first_token == 0 || statement->last_token < statement->first_token) {
		return parser->token_count;
	}

	switch (statement->kind) {
	case MYLITE_STATEMENT_SELECT: desired_token = SELECT_T; break;
	case MYLITE_STATEMENT_INSERT: desired_token = INSERT_T; break;
	case MYLITE_STATEMENT_REPLACE: desired_token = REPLACE_T; break;
	case MYLITE_STATEMENT_UPDATE: desired_token = UPDATE_T; break;
	case MYLITE_STATEMENT_DELETE: desired_token = DELETE_T; break;
	default:
		return statement->first_token - 1;
	}

	token_index = statement->first_token - 1;
	last_token_index = statement->last_token - 1;
	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;
		if (matching_token > token_index + 1) {
			token_index = matching_token;
			continue;
		}
		if (parser->tokens[token_index].parser_token == desired_token) {
			return token_index;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_insert_or_replace_name_token(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index)
{
	token_index = skip_dml_modifiers(parser, token_index, last_token_index);
	if (token_index <= last_token_index && parser->tokens[token_index].parser_token == INTO_T) {
		token_index++;
	}
	if (token_index <= last_token_index && token_can_start_object_name(&parser->tokens[token_index])) {
		return token_index;
	}
	return parser->token_count;
}

static size_t find_update_name_token(const mylite_parser *parser,
                                     size_t token_index,
                                     size_t last_token_index)
{
	token_index = skip_dml_modifiers(parser, token_index, last_token_index);
	return find_table_reference_name_token(parser, token_index, last_token_index, SET_T);
}

static size_t find_table_reference_name_token(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index,
                                              int stop_token)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (parser->tokens[token_index].parser_token == stop_token) {
			return parser->token_count;
		}

		if (matching_token > token_index + 1) {
			size_t close_token_index = matching_token - 1;

			if (group_starts_query_expression(parser, token_index)) {
				token_index = skip_table_reference_alias(parser, matching_token, last_token_index);
			} else {
				size_t nested_name_token = find_table_reference_name_token(parser,
				                                                          token_index + 1,
				                                                          close_token_index - 1,
				                                                          stop_token);
				if (nested_name_token < parser->token_count) {
					return nested_name_token;
				}
				token_index = matching_token;
			}
			continue;
		}

		if (token_can_start_object_name(&parser->tokens[token_index])) {
			return token_index;
		}
		token_index++;
	}
	return parser->token_count;
}

static int group_starts_query_expression(const mylite_parser *parser,
                                         size_t open_token_index)
{
	size_t first_inner_token = open_token_index + 1;

	if (first_inner_token >= parser->token_count) {
		return 0;
	}

	switch (parser->tokens[first_inner_token].parser_token) {
	case SELECT_T:
	case WITH_T:
	case TABLE_T:
	case VALUES_T:
		return 1;
	default:
		return 0;
	}
}

static size_t skip_table_reference_alias(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index)
{
	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    parser->tokens[token_index].parser_token == AS_T) {
		token_index++;
	}
	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    token_can_start_object_name(&parser->tokens[token_index])) {
		token_index++;
	}
	return token_index;
}

static size_t find_delete_name_token(const mylite_parser *parser,
                                     size_t token_index,
                                     size_t last_token_index)
{
	token_index = skip_dml_modifiers(parser, token_index, last_token_index);
	if (token_index <= last_token_index && parser->tokens[token_index].parser_token == FROM_T) {
		token_index++;
	}
	if (token_index <= last_token_index && token_can_start_object_name(&parser->tokens[token_index])) {
		return token_index;
	}
	return parser->token_count;
}

static size_t skip_dml_modifiers(const mylite_parser *parser,
                                 size_t token_index,
                                 size_t last_token_index)
{
	while (token_index <= last_token_index && is_dml_modifier_token(parser->tokens[token_index].parser_token)) {
		token_index++;
	}
	return token_index;
}

static int is_dml_modifier_token(int token)
{
	return token == LOW_PRIORITY_T ||
	       token == DELAYED_T ||
	       token == HIGH_PRIORITY_T ||
	       token == IGNORE_T ||
	       token == QUICK_T;
}

static int classify_direct_statement_object(const mylite_parser *parser, mylite_statement *statement)
{
	size_t token_index;
	size_t last_token_index;
	size_t name_token_index;
	mylite_statement_object_kind object_kind = MYLITE_STATEMENT_OBJECT_NONE;

	if (statement->first_token == 0 ||
	    statement->last_token < statement->first_token ||
	    statement->first_token > parser->token_count) {
		return 0;
	}

	token_index = statement->first_token - 1;
	last_token_index = statement->last_token - 1;
	name_token_index = token_index + 1;

	switch (statement->kind) {
	case MYLITE_STATEMENT_SELECT:
		return classify_select_statement_object(parser, statement);
	case MYLITE_STATEMENT_DO:
	case MYLITE_STATEMENT_VALUES:
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_QUERY);
	case MYLITE_STATEMENT_ALTER:
		return classify_instance_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_RESTART:
	case MYLITE_STATEMENT_SHUTDOWN:
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_INSTANCE);
	case MYLITE_STATEMENT_IMPORT:
		object_kind = MYLITE_STATEMENT_OBJECT_SDI_FILE;
		name_token_index = find_import_sdi_file_token(parser, name_token_index, last_token_index);
		break;
	case MYLITE_STATEMENT_CALL:
		object_kind = MYLITE_STATEMENT_OBJECT_PROCEDURE;
		name_token_index = find_call_procedure_name_token(parser, name_token_index, last_token_index);
		break;
	case MYLITE_STATEMENT_SIGNAL:
	case MYLITE_STATEMENT_RESIGNAL:
		return classify_signal_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_GET:
		return classify_get_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_USE:
		object_kind = MYLITE_STATEMENT_OBJECT_DATABASE;
		break;
	case MYLITE_STATEMENT_DESCRIBE:
	case MYLITE_STATEMENT_EXPLAIN:
		return classify_describe_or_explain_statement_object(parser,
		                                                     statement,
		                                                     name_token_index,
		                                                     last_token_index);
	case MYLITE_STATEMENT_HELP:
		if (name_token_index > last_token_index ||
		    name_token_index >= parser->token_count) {
			return 0;
		}
		return set_statement_direct_object_name_range(parser,
		                                             statement,
		                                             MYLITE_STATEMENT_OBJECT_HELP_TOPIC,
		                                             name_token_index,
		                                             last_token_index);
	case MYLITE_STATEMENT_HANDLER:
		object_kind = MYLITE_STATEMENT_OBJECT_TABLE;
		break;
	case MYLITE_STATEMENT_TABLE:
		return classify_table_statement_object(parser, statement, token_index, last_token_index);
	case MYLITE_STATEMENT_TRUNCATE:
		object_kind = MYLITE_STATEMENT_OBJECT_TABLE;
		if (name_token_index <= last_token_index &&
		    parser->tokens[name_token_index].parser_token == TABLE_T) {
			name_token_index++;
		}
		break;
	case MYLITE_STATEMENT_ANALYZE:
	case MYLITE_STATEMENT_CHECK:
	case MYLITE_STATEMENT_CHECKSUM:
	case MYLITE_STATEMENT_OPTIMIZE:
	case MYLITE_STATEMENT_REPAIR:
		object_kind = MYLITE_STATEMENT_OBJECT_TABLE;
		name_token_index = find_maintenance_table_name_token(parser, name_token_index, last_token_index);
		break;
	case MYLITE_STATEMENT_LOAD:
		object_kind = MYLITE_STATEMENT_OBJECT_TABLE;
		name_token_index = find_load_table_name_token(parser, name_token_index, last_token_index);
		break;
	case MYLITE_STATEMENT_CACHE:
		object_kind = MYLITE_STATEMENT_OBJECT_TABLE;
		name_token_index = find_cache_index_name_token(parser, name_token_index, last_token_index);
		break;
	case MYLITE_STATEMENT_LOCK:
		if (classify_instance_statement_object(parser, statement, name_token_index, last_token_index)) {
			return 1;
		}
		object_kind = MYLITE_STATEMENT_OBJECT_TABLE;
		name_token_index = find_lock_table_name_token(parser, name_token_index, last_token_index);
		break;
	case MYLITE_STATEMENT_UNLOCK:
		if (name_token_index <= last_token_index &&
		    name_token_index < parser->token_count &&
		    (parser->tokens[name_token_index].parser_token == TABLE_T ||
		     token_text_equals(parser, name_token_index, "TABLES"))) {
			return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_TABLE);
		}
		return classify_instance_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_FLUSH:
		return classify_flush_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_START:
		if (classify_transaction_statement_object(parser, statement, name_token_index, last_token_index)) {
			return 1;
		}
		return classify_replication_channel_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_BEGIN:
	case MYLITE_STATEMENT_COMMIT:
		return classify_transaction_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_STOP:
	case MYLITE_STATEMENT_CHANGE:
		return classify_replication_channel_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_KILL:
		return classify_kill_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_CLONE:
		return classify_clone_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_PURGE:
		return classify_purge_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_BINLOG:
		if (name_token_index > last_token_index ||
		    name_token_index >= parser->token_count ||
		    parser->tokens[name_token_index].kind != MYLITE_TOKEN_STRING) {
			return 0;
		}
		object_kind = MYLITE_STATEMENT_OBJECT_BINARY_LOG_EVENT;
		break;
	case MYLITE_STATEMENT_RESET:
		return classify_reset_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_SET:
		return classify_set_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_INSTALL:
	case MYLITE_STATEMENT_UNINSTALL:
		return classify_install_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_XA:
		return classify_xa_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_PREPARE:
	case MYLITE_STATEMENT_EXECUTE:
	case MYLITE_STATEMENT_DEALLOCATE:
		return classify_prepared_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_GRANT:
	case MYLITE_STATEMENT_REVOKE:
		return classify_principal_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_SAVEPOINT:
	case MYLITE_STATEMENT_RELEASE:
	case MYLITE_STATEMENT_ROLLBACK:
		return classify_savepoint_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_DECLARE:
		return classify_declare_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_OPEN:
	case MYLITE_STATEMENT_FETCH:
	case MYLITE_STATEMENT_CLOSE:
		return classify_cursor_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_LEAVE:
	case MYLITE_STATEMENT_ITERATE:
		return classify_label_target_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_SHOW:
		return classify_show_statement_object(parser, statement, name_token_index, last_token_index);
	default:
		return 0;
	}

	return set_statement_direct_object_name(parser, statement, object_kind, name_token_index, last_token_index);
}

static int classify_select_statement_object(const mylite_parser *parser, mylite_statement *statement)
{
	mylite_statement_object_kind object_kind = MYLITE_STATEMENT_OBJECT_NONE;
	size_t name_token_index = find_select_into_target_token(parser, statement, &object_kind);

	if (name_token_index >= parser->token_count) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_QUERY);
	}
	if (object_kind == MYLITE_STATEMENT_OBJECT_NONE) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_QUERY);
	}
	return set_statement_direct_object_name(parser, statement, object_kind, name_token_index, statement->last_token - 1);
}

static size_t find_select_into_target_token(const mylite_parser *parser,
                                            const mylite_statement *statement,
                                            mylite_statement_object_kind *object_kind)
{
	size_t token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;

	if (token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return parser->token_count;
	}

	token_index++;
	last_token_index = statement->last_token - 1;
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (matching_token > token_index + 1) {
			token_index = matching_token;
			continue;
		}
		if (parser->tokens[token_index].parser_token == INTO_T) {
			return find_into_target_token(parser, token_index, last_token_index, object_kind);
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_into_target_token(const mylite_parser *parser,
                                     size_t into_token_index,
                                     size_t last_token_index,
                                     mylite_statement_object_kind *object_kind)
{
	mylite_statement_object_kind variable_kind;
	size_t target_token_index = into_token_index + 1;

	if (target_token_index > last_token_index || target_token_index >= parser->token_count) {
		return parser->token_count;
	}

	if (token_text_equals(parser, target_token_index, "OUTFILE") ||
	    token_text_equals(parser, target_token_index, "DUMPFILE")) {
		if (target_token_index + 1 > last_token_index ||
		    parser->tokens[target_token_index + 1].kind != MYLITE_TOKEN_STRING) {
			return parser->token_count;
		}
		*object_kind = token_text_equals(parser, target_token_index, "OUTFILE") ?
			MYLITE_STATEMENT_OBJECT_OUTFILE :
			MYLITE_STATEMENT_OBJECT_DUMPFILE;
		return target_token_index + 1;
	}

	variable_kind = variable_object_kind_from_token(&parser->tokens[target_token_index]);
	if (variable_kind == MYLITE_STATEMENT_OBJECT_NONE ||
	    variable_kind == MYLITE_STATEMENT_OBJECT_SYSTEM_VARIABLE) {
		return parser->token_count;
	}
	*object_kind = variable_kind;
	return target_token_index;
}

static size_t find_import_sdi_file_token(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == FROM_T &&
		    parser->tokens[token_index + 1].kind == MYLITE_TOKEN_STRING) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_call_procedure_name_token(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index)
{
	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    token_can_continue_object_name(&parser->tokens[token_index])) {
		return token_index;
	}
	return parser->token_count;
}

static int classify_signal_statement_object(const mylite_parser *parser,
                                            mylite_statement *statement,
                                            size_t token_index,
                                            size_t last_token_index)
{
	return classify_condition_value_statement_object(parser, statement, token_index, last_token_index, 0);
}

static int classify_condition_value_statement_object(const mylite_parser *parser,
                                                     mylite_statement *statement,
                                                     size_t token_index,
                                                     size_t last_token_index,
                                                     int allow_condition_class)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "SET")) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "SQLSTATE")) {
		size_t name_token_index = token_index + 1;

		if (name_token_index <= last_token_index &&
		    token_text_equals(parser, name_token_index, "VALUE")) {
			name_token_index++;
		}
		if (name_token_index > last_token_index ||
		    name_token_index >= parser->token_count ||
		    parser->tokens[name_token_index].kind != MYLITE_TOKEN_STRING) {
			return 0;
		}
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_SQLSTATE,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (allow_condition_class &&
	    token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "NOT") &&
	    token_text_equals(parser, token_index + 1, "FOUND")) {
		return set_statement_direct_object_name_range(parser,
		                                              statement,
		                                              MYLITE_STATEMENT_OBJECT_CONDITION,
		                                              token_index,
		                                              token_index + 1);
	}

	if (allow_condition_class && parser->tokens[token_index].kind == MYLITE_TOKEN_NUMBER) {
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_CONDITION,
		                                        token_index,
		                                        last_token_index);
	}

	if (!token_can_continue_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_CONDITION,
	                                        token_index,
	                                        last_token_index);
}

static int classify_get_statement_object(const mylite_parser *parser,
                                         mylite_statement *statement,
                                         size_t token_index,
                                         size_t last_token_index)
{
	size_t diagnostics_target_token;

	while (token_index <= last_token_index && token_index < parser->token_count) {
		if (token_text_equals(parser, token_index, "DIAGNOSTICS")) {
			token_index++;
			break;
		}
		token_index++;
	}

	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (token_text_equals(parser, token_index, "CONDITION") &&
		    token_can_start_diagnostics_condition_number(&parser->tokens[token_index + 1])) {
			return set_statement_direct_object_name_range(parser,
			                                              statement,
			                                              MYLITE_STATEMENT_OBJECT_DIAGNOSTICS_CONDITION,
			                                              token_index + 1,
			                                              token_index + 1);
		}
		token_index++;
	}

	diagnostics_target_token = find_get_diagnostics_target_token(parser, statement->first_token, last_token_index);
	if (diagnostics_target_token >= parser->token_count) {
		return 0;
	}
	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        variable_object_kind_from_token(&parser->tokens[diagnostics_target_token]),
	                                        diagnostics_target_token,
	                                        last_token_index);
}

static size_t find_get_diagnostics_target_token(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index)
{
	while (token_index + 2 <= last_token_index && token_index < parser->token_count) {
		mylite_statement_object_kind object_kind = variable_object_kind_from_token(&parser->tokens[token_index]);
		if (object_kind != MYLITE_STATEMENT_OBJECT_NONE &&
		    object_kind != MYLITE_STATEMENT_OBJECT_SYSTEM_VARIABLE &&
		    token_is_assignment_operator(parser, token_index + 1)) {
			return token_index;
		}
		token_index++;
	}
	return parser->token_count;
}

static int token_can_start_diagnostics_condition_number(const mylite_token *token)
{
	return token->kind == MYLITE_TOKEN_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_QUOTED_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_NUMBER ||
	       token->kind == MYLITE_TOKEN_USER_VARIABLE ||
	       token->kind == MYLITE_TOKEN_SYSTEM_VARIABLE;
}

static int classify_label_target_statement_object(const mylite_parser *parser,
                                                  mylite_statement *statement,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	if (token_index > last_token_index ||
	    token_index >= parser->token_count ||
	    !token_can_start_label_name(&parser->tokens[token_index])) {
		return 0;
	}

	return set_statement_direct_object_name_range(parser,
	                                              statement,
	                                              MYLITE_STATEMENT_OBJECT_LABEL,
	                                              token_index,
	                                              token_index);
}

static int classify_describe_or_explain_statement_object(const mylite_parser *parser,
                                                         mylite_statement *statement,
                                                         size_t token_index,
                                                         size_t last_token_index)
{
	size_t name_token_index = find_describe_or_explain_table_name_token(parser,
	                                                                   token_index,
	                                                                   last_token_index);
	if (name_token_index < parser->token_count) {
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_TABLE,
		                                        name_token_index,
		                                        last_token_index);
	}

	name_token_index = find_explain_into_target_token(parser, token_index, last_token_index);
	if (name_token_index < parser->token_count) {
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_USER_VARIABLE,
		                                        name_token_index,
		                                        last_token_index);
	}

	name_token_index = find_explain_connection_id_token(parser, token_index, last_token_index);
	if (name_token_index < parser->token_count) {
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_CONNECTION,
		                                        name_token_index,
		                                        last_token_index);
	}

	name_token_index = find_explainable_statement_token(parser, token_index, last_token_index);
	if (name_token_index < parser->token_count) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_QUERY);
	}

	return 0;
}

static size_t find_explain_into_target_token(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token != ANALYZE_T &&
		    token_is_explainable_statement_head(parser->tokens[token_index].parser_token)) {
			return parser->token_count;
		}
		if (parser->tokens[token_index].parser_token == INTO_T &&
		    parser->tokens[token_index + 1].kind == MYLITE_TOKEN_USER_VARIABLE) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_explain_connection_id_token(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index)
{
	while (token_index + 2 <= last_token_index && token_index < parser->token_count) {
		if (token_is_explainable_statement_head(parser->tokens[token_index].parser_token)) {
			return parser->token_count;
		}
		if (token_text_equals(parser, token_index, "FOR") &&
		    token_text_equals(parser, token_index + 1, "CONNECTION") &&
		    token_can_start_object_name(&parser->tokens[token_index + 2])) {
			return token_index + 2;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_explainable_statement_token(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		if (token_is_explainable_statement_head(parser->tokens[token_index].parser_token)) {
			return token_index;
		}
		token_index++;
	}
	return parser->token_count;
}

static int token_is_explainable_statement_head(int token)
{
	switch (token) {
	case SELECT_T:
	case TABLE_T:
	case DELETE_T:
	case INSERT_T:
	case REPLACE_T:
	case UPDATE_T:
	case ANALYZE_T:
		return 1;
	default:
		return 0;
	}
}

static int classify_table_statement_object(const mylite_parser *parser,
                                           mylite_statement *statement,
                                           size_t token_index,
                                           size_t last_token_index)
{
	mylite_statement_object_kind object_kind = MYLITE_STATEMENT_OBJECT_NONE;
	size_t name_token_index = find_table_into_target_token(parser,
	                                                       token_index,
	                                                       last_token_index,
	                                                       &object_kind);

	if (name_token_index < parser->token_count && object_kind != MYLITE_STATEMENT_OBJECT_NONE) {
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        object_kind,
		                                        name_token_index,
		                                        last_token_index);
	}

	name_token_index = find_table_statement_name_token(parser, token_index, last_token_index);
	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_TABLE,
	                                        name_token_index,
	                                        last_token_index);
}

static size_t find_table_into_target_token(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index,
                                           mylite_statement_object_kind *object_kind)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == INTO_T) {
			return find_into_target_token(parser, token_index, last_token_index, object_kind);
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_table_statement_name_token(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index)
{
	while (token_index <= last_token_index &&
	       token_index < parser->token_count &&
	       parser->tokens[token_index].parser_token == '(') {
		token_index++;
	}
	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    parser->tokens[token_index].parser_token == TABLE_T) {
		return token_index + 1;
	}
	return parser->token_count;
}

static size_t find_describe_or_explain_table_name_token(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index)
{
	if (token_index > last_token_index ||
	    token_index >= parser->token_count ||
	    token_text_equals(parser, token_index, "FOR") ||
	    token_text_equals(parser, token_index, "FORMAT") ||
	    token_text_equals(parser, token_index, "EXTENDED") ||
	    token_text_equals(parser, token_index, "PARTITIONS") ||
	    token_text_equals(parser, token_index, "CONNECTION")) {
		return parser->token_count;
	}
	if (token_can_start_object_name(&parser->tokens[token_index])) {
		return token_index;
	}
	return parser->token_count;
}

static size_t find_load_table_name_token(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index)
{
	if (token_index + 3 <= last_token_index &&
	    parser->tokens[token_index].parser_token == INDEX_T &&
	    parser->tokens[token_index + 1].parser_token == INTO_T &&
	    parser->tokens[token_index + 2].parser_token == CACHE_T &&
	    token_can_start_object_name(&parser->tokens[token_index + 3])) {
		return token_index + 3;
	}

	while (token_index + 2 <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == INTO_T &&
		    parser->tokens[token_index + 1].parser_token == TABLE_T &&
		    token_can_start_object_name(&parser->tokens[token_index + 2])) {
			return token_index + 2;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_cache_index_name_token(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index)
{
	if (token_index + 1 <= last_token_index &&
	    token_index + 1 < parser->token_count &&
	    parser->tokens[token_index].parser_token == INDEX_T &&
	    token_can_start_object_name(&parser->tokens[token_index + 1])) {
		return token_index + 1;
	}
	return parser->token_count;
}

static size_t find_lock_table_name_token(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return parser->token_count;
	}

	if (parser->tokens[token_index].parser_token == TABLE_T ||
	    token_text_equals(parser, token_index, "TABLES")) {
		token_index++;
		if (token_index <= last_token_index && token_can_start_object_name(&parser->tokens[token_index])) {
			return token_index;
		}
	}

	return parser->token_count;
}

static size_t find_flush_table_name_token(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index)
{
	if (token_index > last_token_index ||
	    token_index >= parser->token_count ||
	    (parser->tokens[token_index].parser_token != TABLE_T &&
	     !token_text_equals(parser, token_index, "TABLES"))) {
		return parser->token_count;
	}

	token_index++;
	if (token_index <= last_token_index && token_can_start_object_name(&parser->tokens[token_index])) {
		return token_index;
	}
	return parser->token_count;
}

static int classify_flush_statement_object(const mylite_parser *parser,
                                           mylite_statement *statement,
                                           size_t token_index,
                                           size_t last_token_index)
{
	size_t name_token_index;

	while (token_index <= last_token_index &&
	       token_index < parser->token_count &&
	       (parser->tokens[token_index].parser_token == LOCAL_T ||
	        token_text_equals(parser, token_index, "NO_WRITE_TO_BINLOG"))) {
		token_index++;
	}

	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "RELAY") &&
	    token_text_equals(parser, token_index + 1, "LOGS")) {
		name_token_index = find_replication_channel_name_token(parser, token_index + 2, last_token_index);
		if (name_token_index >= parser->token_count) {
			if (has_replication_channel_clause(parser, token_index + 2, last_token_index)) {
				return 0;
			}
			return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_REPLICATION_CHANNEL);
		}
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_REPLICATION_CHANNEL,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "BINARY") &&
	    token_text_equals(parser, token_index + 1, "LOGS")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_BINARY_LOG);
	}

	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "ENGINE") &&
	    token_text_equals(parser, token_index + 1, "LOGS")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_ENGINE_LOG);
	}

	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "ERROR") &&
	    token_text_equals(parser, token_index + 1, "LOGS")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_ERROR_LOG);
	}

	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "GENERAL") &&
	    token_text_equals(parser, token_index + 1, "LOGS")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_GENERAL_LOG);
	}

	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "SLOW") &&
	    token_text_equals(parser, token_index + 1, "LOGS")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_SLOW_LOG);
	}

	if (token_text_equals(parser, token_index, "LOGS")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_LOG);
	}

	if (token_text_equals(parser, token_index, "PRIVILEGES")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_PRIVILEGE);
	}

	if (token_text_equals(parser, token_index, "STATUS")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_STATUS_VARIABLE);
	}

	if (token_text_equals(parser, token_index, "HOSTS")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_HOST_CACHE);
	}

	if (token_text_equals(parser, token_index, "OPTIMIZER_COSTS")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_OPTIMIZER_COST);
	}

	if (token_text_equals(parser, token_index, "USER_RESOURCES")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_USER_RESOURCE);
	}

	name_token_index = find_flush_table_name_token(parser, token_index, last_token_index);
	if (name_token_index >= parser->token_count &&
	    token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    (parser->tokens[token_index].parser_token == TABLE_T ||
	     token_text_equals(parser, token_index, "TABLES"))) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_TABLE);
	}

	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_TABLE,
	                                        name_token_index,
	                                        last_token_index);
}

static size_t find_maintenance_table_name_token(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if ((parser->tokens[token_index].parser_token == TABLE_T ||
		     token_text_equals(parser, token_index, "TABLES")) &&
		    token_can_start_object_name(&parser->tokens[token_index + 1])) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static int classify_instance_statement_object(const mylite_parser *parser,
                                              mylite_statement *statement,
                                              size_t token_index,
                                              size_t last_token_index)
{
	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    token_text_equals(parser, token_index, "INSTANCE")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_INSTANCE);
	}
	return 0;
}

static int classify_kill_statement_object(const mylite_parser *parser,
                                          mylite_statement *statement,
                                          size_t token_index,
                                          size_t last_token_index)
{
	mylite_statement_object_kind object_kind = MYLITE_STATEMENT_OBJECT_CONNECTION;

	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "QUERY")) {
		object_kind = MYLITE_STATEMENT_OBJECT_QUERY;
		token_index++;
	} else if (token_text_equals(parser, token_index, "CONNECTION")) {
		token_index++;
	}

	if (token_index > last_token_index ||
	    token_index >= parser->token_count ||
	    !token_can_start_processlist_expression(parser, token_index, last_token_index) ||
	    !processlist_expression_is_single_target(parser, token_index, last_token_index)) {
		return 0;
	}

	return set_statement_direct_object_name_range(parser,
	                                             statement,
	                                             object_kind,
	                                             token_index,
	                                             last_token_index);
}

static int token_can_start_processlist_id(const mylite_token *token)
{
	return token->kind == MYLITE_TOKEN_NUMBER ||
	       token->kind == MYLITE_TOKEN_USER_VARIABLE;
}

static int token_can_start_processlist_expression(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	const mylite_token *token;

	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	token = &parser->tokens[token_index];
	if (token_can_start_processlist_id(token) ||
	    token->kind == MYLITE_TOKEN_STRING ||
	    token->kind == MYLITE_TOKEN_PARAMETER ||
	    token->kind == MYLITE_TOKEN_SYSTEM_VARIABLE) {
		return 1;
	}

	if (token_index + 1 > last_token_index ||
	    parser->tokens[token_index + 1].parser_token != '(') {
		return 0;
	}
	return token->kind == MYLITE_TOKEN_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_QUOTED_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_KEYWORD;
}

static int processlist_expression_is_single_target(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (matching_token > token_index + 1) {
			token_index = matching_token;
			continue;
		}
		if (parser->tokens[token_index].parser_token == ',') {
			return 0;
		}
		token_index++;
	}
	return 1;
}

static int classify_clone_statement_object(const mylite_parser *parser,
                                           mylite_statement *statement,
                                           size_t token_index,
                                           size_t last_token_index)
{
	size_t name_token_index;
	size_t endpoint_last_token;

	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == LOCAL_T) {
		name_token_index = find_data_directory_value_token(parser, token_index + 1, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_DIRECTORY,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (!token_text_equals(parser, token_index, "INSTANCE") ||
	    token_index + 1 > last_token_index ||
	    parser->tokens[token_index + 1].parser_token != FROM_T) {
		return 0;
	}

	name_token_index = token_index + 2;
	endpoint_last_token = find_clone_endpoint_last_token(parser, name_token_index, last_token_index);
	if (endpoint_last_token >= parser->token_count) {
		return 0;
	}

	return set_statement_direct_object_name_range(parser,
	                                              statement,
	                                              MYLITE_STATEMENT_OBJECT_SERVER,
	                                              name_token_index,
	                                              endpoint_last_token);
}

static size_t find_data_directory_value_token(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index)
{
	while (token_index + 1 <= last_token_index) {
		if (parser->tokens[token_index].parser_token == DATA_T &&
		    token_text_equals(parser, token_index + 1, "DIRECTORY")) {
			token_index += 2;
			if (token_index <= last_token_index &&
			    token_text_equals(parser, token_index, "=")) {
				token_index++;
			}
			if (token_index <= last_token_index &&
			    parser->tokens[token_index].kind == MYLITE_TOKEN_STRING) {
				return token_index;
			}
			return parser->token_count;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_clone_endpoint_last_token(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index)
{
	size_t endpoint_first_token = token_index;

	while (token_index <= last_token_index) {
		if (token_text_equals(parser, token_index, "IDENTIFIED")) {
			if (token_index == endpoint_first_token) {
				return parser->token_count;
			}
			return token_index - 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static int classify_purge_statement_object(const mylite_parser *parser,
                                           mylite_statement *statement,
                                           size_t token_index,
                                           size_t last_token_index)
{
	size_t name_token_index;

	if (token_index + 3 > last_token_index ||
	    token_index + 3 >= parser->token_count ||
	    (!token_text_equals(parser, token_index, "BINARY") &&
	     !token_text_equals(parser, token_index, "MASTER")) ||
	    !token_text_equals(parser, token_index + 1, "LOGS")) {
		return 0;
	}

	name_token_index = token_index + 2;
	while (name_token_index + 1 <= last_token_index && name_token_index < parser->token_count) {
		if (parser->tokens[name_token_index].parser_token == TO_T &&
		    token_can_start_object_name(&parser->tokens[name_token_index + 1])) {
			return set_statement_direct_object_name(parser,
			                                        statement,
			                                        MYLITE_STATEMENT_OBJECT_BINARY_LOG,
			                                        name_token_index + 1,
			                                        last_token_index);
		}
		if (token_text_equals(parser, name_token_index, "BEFORE")) {
			return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_BINARY_LOG);
		}
		name_token_index++;
	}

	return 0;
}

static int classify_replication_channel_statement_object(const mylite_parser *parser,
                                                         mylite_statement *statement,
                                                         size_t token_index,
                                                         size_t last_token_index)
{
	size_t name_token_index;

	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    token_text_equals(parser, token_index, "GROUP_REPLICATION")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_GROUP_REPLICATION);
	}

	name_token_index = find_replication_channel_name_token(parser, token_index, last_token_index);

	if (name_token_index >= parser->token_count) {
		if (is_replication_channel_operation(parser, statement->kind, token_index, last_token_index)) {
			return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_REPLICATION_CHANNEL);
		}
		return 0;
	}

	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_REPLICATION_CHANNEL,
	                                        name_token_index,
	                                        last_token_index);
}

static int is_replication_channel_operation(const mylite_parser *parser,
                                            mylite_statement_kind statement_kind,
                                            size_t token_index,
                                            size_t last_token_index)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	return token_text_equals(parser, token_index, "REPLICA") ||
	       token_text_equals(parser, token_index, "SLAVE") ||
	       (statement_kind == MYLITE_STATEMENT_CHANGE &&
	        token_text_equals(parser, token_index, "MASTER")) ||
	       token_text_equals(parser, token_index, "REPLICATION");
}

static int classify_reset_statement_object(const mylite_parser *parser,
                                           mylite_statement *statement,
                                           size_t token_index,
                                           size_t last_token_index)
{
	size_t name_token_index;

	if (token_index > last_token_index ||
	    token_index >= parser->token_count) {
		return 0;
	}

	if (token_index + 3 <= last_token_index &&
	    token_text_equals(parser, token_index, "BINARY") &&
	    token_text_equals(parser, token_index + 1, "LOGS") &&
	    parser->tokens[token_index + 2].parser_token == AND_T &&
	    token_text_equals(parser, token_index + 3, "GTIDS")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_BINARY_LOG);
	}

	if (token_text_equals(parser, token_index, "MASTER")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_BINARY_LOG);
	}

	if (!token_text_equals(parser, token_index, "PERSIST")) {
		return classify_replication_channel_statement_object(parser, statement, token_index, last_token_index);
	}

	token_index++;
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_SYSTEM_VARIABLE);
	}

	if (parser->tokens[token_index].parser_token == IF_T) {
		if (token_index + 2 > last_token_index ||
		    parser->tokens[token_index + 1].parser_token != EXISTS_T) {
			return 0;
		}
		token_index += 2;
		if (token_index > last_token_index || token_index >= parser->token_count) {
			return 0;
		}
	}

	name_token_index = token_index;
	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_SYSTEM_VARIABLE,
	                                        name_token_index,
	                                        last_token_index);
}

static size_t find_replication_channel_name_token(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	while (token_index + 2 <= last_token_index && token_index < parser->token_count) {
		if (token_text_equals(parser, token_index, "FOR") &&
		    token_text_equals(parser, token_index + 1, "CHANNEL") &&
		    token_can_start_object_name(&parser->tokens[token_index + 2])) {
			return token_index + 2;
		}
		token_index++;
	}
	return parser->token_count;
}

static int has_replication_channel_clause(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (token_text_equals(parser, token_index, "FOR") &&
		    token_text_equals(parser, token_index + 1, "CHANNEL")) {
			return 1;
		}
		token_index++;
	}
	return 0;
}

static int classify_transaction_statement_object(const mylite_parser *parser,
                                                 mylite_statement *statement,
                                                 size_t token_index,
                                                 size_t last_token_index)
{
	switch (statement->kind) {
	case MYLITE_STATEMENT_BEGIN:
		if (token_index > last_token_index) {
			return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_TRANSACTION);
		}
		if (token_index < parser->token_count && token_text_equals(parser, token_index, "WORK")) {
			return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_TRANSACTION);
		}
		return 0;
	case MYLITE_STATEMENT_COMMIT:
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_TRANSACTION);
	case MYLITE_STATEMENT_START:
		if (token_index <= last_token_index &&
		    token_index < parser->token_count &&
		    parser->tokens[token_index].parser_token == TRANSACTION_T) {
			return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_TRANSACTION);
		}
		return 0;
	case MYLITE_STATEMENT_SET:
		if (token_index > last_token_index || token_index >= parser->token_count) {
			return 0;
		}
		if (parser->tokens[token_index].parser_token == TRANSACTION_T) {
			return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_TRANSACTION);
		}
		if (token_index + 1 <= last_token_index &&
		    token_index + 1 < parser->token_count &&
		    (token_text_equals(parser, token_index, "GLOBAL") ||
		     token_text_equals(parser, token_index, "LOCAL") ||
		     token_text_equals(parser, token_index, "SESSION")) &&
		    parser->tokens[token_index + 1].parser_token == TRANSACTION_T) {
			return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_TRANSACTION);
		}
		return 0;
	default:
		return 0;
	}
}

static int classify_set_statement_object(const mylite_parser *parser,
                                         mylite_statement *statement,
                                         size_t token_index,
                                         size_t last_token_index)
{
	size_t name_token_index;

	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == ROLE_T) {
		name_token_index = find_set_role_name_token(parser, token_index + 1, last_token_index);
		if (name_token_index >= parser->token_count) {
			if (is_set_role_collection_target(parser, token_index + 1, last_token_index)) {
				return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_ROLE);
			}
			return 0;
		}
		statement->object_kind = MYLITE_STATEMENT_OBJECT_ROLE;
		set_statement_account_name_from_first_token(parser, statement, name_token_index, last_token_index);
		return 1;
	}

	if (token_text_equals(parser, token_index, "RESOURCE") &&
	    token_index + 2 <= last_token_index &&
	    parser->tokens[token_index + 1].parser_token == GROUP_T &&
	    token_can_start_object_name(&parser->tokens[token_index + 2])) {
		statement->object_kind = MYLITE_STATEMENT_OBJECT_RESOURCE_GROUP;
		set_statement_object_name_from_first_token(parser, statement, token_index + 2, last_token_index);
		return 1;
	}

	if (parser->tokens[token_index].parser_token == DEFAULT_T &&
	    token_index + 1 <= last_token_index &&
	    parser->tokens[token_index + 1].parser_token == ROLE_T) {
		name_token_index = find_set_role_name_token(parser, token_index + 2, last_token_index);
		if (name_token_index < parser->token_count) {
			statement->object_kind = MYLITE_STATEMENT_OBJECT_ROLE;
			set_statement_account_name_from_first_token(parser, statement, name_token_index, last_token_index);
			return 1;
		}

		name_token_index = find_set_default_role_user_name_token(parser, token_index + 2, last_token_index);
		if (name_token_index >= parser->token_count) {
			return 0;
		}
		statement->object_kind = MYLITE_STATEMENT_OBJECT_USER;
		set_statement_account_name_from_first_token(parser, statement, name_token_index, last_token_index);
		return 1;
	}

	if (token_text_equals(parser, token_index, "PASSWORD")) {
		name_token_index = find_set_password_name_token(parser, token_index + 1, last_token_index);
		if (name_token_index >= parser->token_count) {
			return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_USER);
		}
		statement->object_kind = MYLITE_STATEMENT_OBJECT_USER;
		set_statement_account_name_from_first_token(parser, statement, name_token_index, last_token_index);
		return 1;
	}

	if (classify_transaction_statement_object(parser, statement, token_index, last_token_index)) {
		return 1;
	}

	if (classify_set_character_set_statement_object(parser, statement, token_index, last_token_index)) {
		return 1;
	}

	if (token_index + 1 <= last_token_index &&
	    parser->tokens[token_index].kind == MYLITE_TOKEN_USER_VARIABLE &&
	    token_is_assignment_operator(parser, token_index + 1)) {
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_USER_VARIABLE,
		                                        token_index,
		                                        last_token_index);
	}

	name_token_index = find_set_system_variable_name_token(parser, token_index, last_token_index);
	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_SYSTEM_VARIABLE,
	                                        name_token_index,
	                                        last_token_index);
}

static size_t find_set_role_name_token(const mylite_parser *parser,
                                       size_t token_index,
                                       size_t last_token_index)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return parser->token_count;
	}

	if (parser->tokens[token_index].parser_token == ALL_T) {
		if (token_index + 2 <= last_token_index &&
		    token_text_equals(parser, token_index + 1, "EXCEPT") &&
		    token_can_start_object_name(&parser->tokens[token_index + 2])) {
			return token_index + 2;
		}
		return parser->token_count;
	}

	if (parser->tokens[token_index].parser_token == DEFAULT_T ||
	    token_text_equals(parser, token_index, "NONE")) {
		return parser->token_count;
	}

	if (token_can_start_object_name(&parser->tokens[token_index])) {
		return token_index;
	}
	return parser->token_count;
}

static int is_set_role_collection_target(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == ALL_T) {
		return token_index + 1 > last_token_index ||
		       !token_text_equals(parser, token_index + 1, "EXCEPT");
	}

	return parser->tokens[token_index].parser_token == DEFAULT_T ||
	       token_text_equals(parser, token_index, "NONE");
}

static size_t find_set_default_role_user_name_token(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == TO_T &&
		    token_can_start_object_name(&parser->tokens[token_index + 1])) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_set_password_name_token(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index)
{
	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "FOR") &&
	    token_can_start_object_name(&parser->tokens[token_index + 1])) {
		return token_index + 1;
	}
	return parser->token_count;
}

static int classify_set_character_set_statement_object(const mylite_parser *parser,
                                                       mylite_statement *statement,
                                                       size_t token_index,
                                                       size_t last_token_index)
{
	size_t name_token_index;

	if (token_text_equals(parser, token_index, "NAMES")) {
		name_token_index = token_index + 1;
	} else if (parser->tokens[token_index].parser_token == CHARACTER_T &&
	           token_index + 2 <= last_token_index &&
	           parser->tokens[token_index + 1].parser_token == SET_T) {
		name_token_index = token_index + 2;
	} else if (parser->tokens[token_index].parser_token == CHARSET_T) {
		name_token_index = token_index + 1;
	} else {
		return 0;
	}

	if (name_token_index > last_token_index ||
	    name_token_index >= parser->token_count ||
	    (!token_can_start_object_name(&parser->tokens[name_token_index]) &&
	     parser->tokens[name_token_index].parser_token != DEFAULT_T)) {
		return 0;
	}

	return set_statement_direct_object_name_range(parser,
	                                              statement,
	                                              MYLITE_STATEMENT_OBJECT_CHARACTER_SET,
	                                              name_token_index,
	                                              name_token_index);
}

static size_t find_set_system_variable_name_token(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return parser->token_count;
	}

	if (parser->tokens[token_index].kind == MYLITE_TOKEN_SYSTEM_VARIABLE) {
		if (token_index + 1 <= last_token_index &&
		    token_is_assignment_operator(parser, token_index + 1)) {
			return token_index;
		}
		return parser->token_count;
	}

	if ((token_text_equals(parser, token_index, "GLOBAL") ||
	     token_text_equals(parser, token_index, "SESSION") ||
	     token_text_equals(parser, token_index, "LOCAL") ||
	     token_text_equals(parser, token_index, "PERSIST") ||
	     token_text_equals(parser, token_index, "PERSIST_ONLY"))) {
		size_t name_token_index = token_index + 1;
		size_t last_name_token;

		if (name_token_index > last_token_index ||
		    name_token_index >= parser->token_count ||
		    !token_can_start_set_system_variable_name(parser, name_token_index, last_token_index)) {
			return parser->token_count;
		}

		last_name_token = last_qualified_name_token(parser, name_token_index, last_token_index);
		if (last_name_token + 1 <= last_token_index &&
		    token_is_assignment_operator(parser, last_name_token + 1)) {
			return name_token_index;
		}
		return parser->token_count;
	}

	if (token_can_start_set_system_variable_name(parser, token_index, last_token_index)) {
		size_t last_name_token = last_qualified_name_token(parser, token_index, last_token_index);
		if (last_name_token + 1 > last_token_index ||
		    !token_is_assignment_operator(parser, last_name_token + 1)) {
			return parser->token_count;
		}
		return token_index;
	}

	return parser->token_count;
}

static int token_can_start_set_system_variable_name(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index].kind == MYLITE_TOKEN_IDENTIFIER ||
	    parser->tokens[token_index].kind == MYLITE_TOKEN_QUOTED_IDENTIFIER ||
	    token_can_be_unquoted_object_name_keyword(parser->tokens[token_index].parser_token)) {
		return 1;
	}

	return parser->tokens[token_index].parser_token == DEFAULT_T &&
	       token_index + 2 <= last_token_index &&
	       parser->tokens[token_index + 1].parser_token == '.' &&
	       token_can_continue_qualified_object_name(&parser->tokens[token_index + 2]);
}

static int classify_install_statement_object(const mylite_parser *parser,
                                             mylite_statement *statement,
                                             size_t token_index,
                                             size_t last_token_index)
{
	mylite_statement_object_kind object_kind;

	if (token_index + 1 > last_token_index ||
	    token_index + 1 >= parser->token_count ||
	    !token_can_start_object_name(&parser->tokens[token_index + 1])) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "COMPONENT")) {
		object_kind = MYLITE_STATEMENT_OBJECT_COMPONENT;
	} else if (token_text_equals(parser, token_index, "PLUGIN")) {
		object_kind = MYLITE_STATEMENT_OBJECT_PLUGIN;
	} else {
		return 0;
	}

	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        object_kind,
	                                        token_index + 1,
	                                        last_token_index);
}

static int classify_xa_statement_object(const mylite_parser *parser,
                                        mylite_statement *statement,
                                        size_t token_index,
                                        size_t last_token_index)
{
	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    token_text_equals(parser, token_index, "RECOVER")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_XA_TRANSACTION);
	}

	if (token_index + 1 > last_token_index ||
	    token_index + 1 >= parser->token_count ||
	    !token_can_start_object_name(&parser->tokens[token_index + 1])) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token != START_T &&
	    parser->tokens[token_index].parser_token != BEGIN_T &&
	    parser->tokens[token_index].parser_token != END_T &&
	    parser->tokens[token_index].parser_token != PREPARE_T &&
	    parser->tokens[token_index].parser_token != COMMIT_T &&
	    parser->tokens[token_index].parser_token != ROLLBACK_T) {
		return 0;
	}

	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_XA_TRANSACTION,
	                                        token_index + 1,
	                                        last_token_index);
}

static int classify_prepared_statement_object(const mylite_parser *parser,
                                              mylite_statement *statement,
                                              size_t token_index,
                                              size_t last_token_index)
{
	if (statement->kind == MYLITE_STATEMENT_DEALLOCATE &&
	    token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    parser->tokens[token_index].parser_token == PREPARE_T) {
		token_index++;
	}

	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_PREPARED_STATEMENT,
	                                        token_index,
	                                        last_token_index);
}

static int classify_principal_statement_object(const mylite_parser *parser,
                                               mylite_statement *statement,
                                               size_t token_index,
                                               size_t last_token_index)
{
	int marker_token = statement->kind == MYLITE_STATEMENT_GRANT ? TO_T : FROM_T;
	size_t name_token_index = find_principal_name_token(parser, token_index, last_token_index, marker_token);

	if (name_token_index > last_token_index || name_token_index >= parser->token_count) {
		return 0;
	}

	statement->object_kind = MYLITE_STATEMENT_OBJECT_USER;
	set_statement_account_name_from_first_token(parser, statement, name_token_index, last_token_index);
	return 1;
}

static int classify_savepoint_statement_object(const mylite_parser *parser,
                                               mylite_statement *statement,
                                               size_t token_index,
                                               size_t last_token_index)
{
	size_t name_token_index;

	if (statement->kind == MYLITE_STATEMENT_SAVEPOINT) {
		name_token_index = token_index;
	} else if (statement->kind == MYLITE_STATEMENT_ROLLBACK) {
		name_token_index = find_rollback_savepoint_name_token(parser, token_index, last_token_index);
	} else {
		name_token_index = find_savepoint_name_token(parser, token_index, last_token_index);
	}

	if (statement->kind == MYLITE_STATEMENT_ROLLBACK && name_token_index >= parser->token_count) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_TRANSACTION);
	}

	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_SAVEPOINT,
	                                        name_token_index,
	                                        last_token_index);
}

static size_t find_rollback_savepoint_name_token(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index)
{
	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    token_text_equals(parser, token_index, "WORK")) {
		token_index++;
	}
	if (token_index > last_token_index ||
	    token_index >= parser->token_count ||
	    parser->tokens[token_index].parser_token != TO_T) {
		return parser->token_count;
	}

	token_index++;
	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    parser->tokens[token_index].parser_token == SAVEPOINT_T) {
		token_index++;
	}
	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    token_can_start_object_name(&parser->tokens[token_index])) {
		return token_index;
	}
	return parser->token_count;
}

static int classify_declare_statement_object(const mylite_parser *parser,
                                             mylite_statement *statement,
                                             size_t token_index,
                                             size_t last_token_index)
{
	size_t handler_condition_token;

	if (token_index + 1 <= last_token_index &&
	    token_index + 1 < parser->token_count &&
	    token_can_continue_object_name(&parser->tokens[token_index]) &&
	    token_text_equals(parser, token_index + 1, "CONDITION")) {
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_CONDITION,
		                                        token_index,
		                                        last_token_index);
	}

	handler_condition_token = find_declare_handler_condition_token(parser, token_index, last_token_index);
	if (handler_condition_token < parser->token_count) {
		return classify_condition_value_statement_object(parser,
		                                                 statement,
		                                                 handler_condition_token,
		                                                 last_token_index,
		                                                 1);
	}

	if (!statement_contains_token(parser, token_index, last_token_index, CURSOR_T)) {
		if (token_index <= last_token_index &&
		    token_index < parser->token_count &&
		    token_can_continue_object_name(&parser->tokens[token_index])) {
			return set_statement_direct_object_name(parser,
			                                        statement,
			                                        MYLITE_STATEMENT_OBJECT_LOCAL_VARIABLE,
			                                        token_index,
			                                        last_token_index);
		}
		return 0;
	}
	return classify_cursor_statement_object(parser, statement, token_index, last_token_index);
}

static size_t find_declare_handler_condition_token(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == HANDLER_T) {
			token_index++;
			break;
		}
		token_index++;
	}
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (token_text_equals(parser, token_index, "FOR")) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static int classify_cursor_statement_object(const mylite_parser *parser,
                                            mylite_statement *statement,
                                            size_t token_index,
                                            size_t last_token_index)
{
	size_t name_token_index = find_cursor_name_token(parser,
	                                                 statement->kind,
	                                                 token_index,
	                                                 last_token_index);

	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_CURSOR,
	                                        name_token_index,
	                                        last_token_index);
}

static size_t find_cursor_name_token(const mylite_parser *parser,
                                     mylite_statement_kind statement_kind,
                                     size_t token_index,
                                     size_t last_token_index)
{
	if (statement_kind == MYLITE_STATEMENT_FETCH) {
		if (token_index <= last_token_index &&
		    token_index < parser->token_count &&
		    token_text_equals(parser, token_index, "NEXT")) {
			if (token_index + 1 > last_token_index ||
			    !token_text_equals(parser, token_index + 1, "FROM")) {
				return parser->token_count;
			}
			token_index += 2;
		} else if (token_index <= last_token_index &&
		           token_index < parser->token_count &&
		           token_text_equals(parser, token_index, "FROM")) {
			token_index++;
		}
	}

	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    token_can_continue_object_name(&parser->tokens[token_index])) {
		return token_index;
	}
	return parser->token_count;
}

static int statement_contains_token(const mylite_parser *parser,
                                    size_t token_index,
                                    size_t last_token_index,
                                    int wanted_token)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == wanted_token) {
			return 1;
		}
		token_index++;
	}
	return 0;
}

static size_t find_savepoint_name_token(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == SAVEPOINT_T &&
		    token_can_start_object_name(&parser->tokens[token_index + 1])) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_principal_name_token(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index,
                                        int marker_token)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == marker_token &&
		    token_can_start_object_name(&parser->tokens[token_index + 1])) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static void set_statement_account_name_from_first_token(const mylite_parser *parser,
                                                        mylite_statement *statement,
                                                        size_t first_name_token,
                                                        size_t last_token_index)
{
	size_t last_name_token;
	const mylite_token *first;
	const mylite_token *last;

	if (first_name_token >= parser->token_count) {
		return;
	}

	last_name_token = last_account_name_token(parser, first_name_token, last_token_index);
	first = &parser->tokens[first_name_token];
	last = &parser->tokens[last_name_token];

	statement->object_name_first_token = first_name_token + 1;
	statement->object_name_last_token = last_name_token + 1;
	statement->object_name_start_offset = first->start_offset;
	statement->object_name_end_offset = last->end_offset;
	statement->object_name_start_line = first->start_line;
	statement->object_name_start_column = first->start_column;
	statement->object_name_end_line = last->end_line;
	statement->object_name_end_column = last->end_column;
}

static size_t last_account_name_token(const mylite_parser *parser,
                                      size_t first_name_token,
                                      size_t last_token_index)
{
	if (first_name_token + 2 <= last_token_index &&
	    token_is_current_user_function_name(parser, first_name_token) &&
	    token_pair_is_empty_parentheses(parser, first_name_token + 1)) {
		return first_name_token + 2;
	}
	if (first_name_token + 1 <= last_token_index &&
	    token_is_account_host_suffix(parser, first_name_token + 1)) {
		return first_name_token + 1;
	}
	if (first_name_token + 2 <= last_token_index &&
	    token_is_account_at_marker(parser, first_name_token + 1) &&
	    !token_is_account_name_clause_boundary(parser, first_name_token + 2) &&
	    token_can_start_object_name(&parser->tokens[first_name_token + 2])) {
		return first_name_token + 2;
	}
	return first_name_token;
}

static int token_is_account_name_clause_boundary(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "ADD") ||
	       token_text_equals(parser, token_index, "ACCOUNT") ||
	       token_text_equals(parser, token_index, "ATTRIBUTE") ||
	       token_text_equals(parser, token_index, "COMMENT") ||
	       token_text_equals(parser, token_index, "DEFAULT") ||
	       token_text_equals(parser, token_index, "DISCARD") ||
	       token_text_equals(parser, token_index, "DROP") ||
	       token_text_equals(parser, token_index, "FAILED_LOGIN_ATTEMPTS") ||
	       token_text_equals(parser, token_index, "FROM") ||
	       token_text_equals(parser, token_index, "IDENTIFIED") ||
	       token_text_equals(parser, token_index, "MODIFY") ||
	       token_text_equals(parser, token_index, "PASSWORD") ||
	       token_text_equals(parser, token_index, "PASSWORD_LOCK_TIME") ||
	       token_text_equals(parser, token_index, "REPLACE") ||
	       token_text_equals(parser, token_index, "REQUIRE") ||
	       token_text_equals(parser, token_index, "RETAIN") ||
	       token_text_equals(parser, token_index, "TO") ||
	       token_text_equals(parser, token_index, "WITH");
}

static int token_is_current_user_function_name(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "CURRENT_USER");
}

static int token_pair_is_empty_parentheses(const mylite_parser *parser, size_t open_token_index)
{
	return open_token_index + 1 < parser->token_count &&
	       parser->tokens[open_token_index].parser_token == '(' &&
	       parser->tokens[open_token_index + 1].parser_token == ')' &&
	       parser->tokens[open_token_index].matching_token == open_token_index + 2;
}

static int token_is_account_host_suffix(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       parser->tokens[token_index].parser_token == USER_VARIABLE &&
	       parser->tokens[token_index].end_offset > parser->tokens[token_index].start_offset + 1 &&
	       parser->lexer.input[parser->tokens[token_index].start_offset] == '@';
}

static int token_is_account_at_marker(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       parser->tokens[token_index].parser_token == USER_VARIABLE &&
	       parser->tokens[token_index].end_offset - parser->tokens[token_index].start_offset == 1 &&
	       parser->lexer.input[parser->tokens[token_index].start_offset] == '@';
}

static int classify_show_statement_object(const mylite_parser *parser,
                                          mylite_statement *statement,
                                          size_t token_index,
                                          size_t last_token_index)
{
	mylite_statement_object_kind object_kind;
	size_t name_token_index;

	token_index = skip_show_modifiers(parser, token_index, last_token_index);
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == CREATE_T && token_index + 1 <= last_token_index) {
		size_t object_token_index = token_index + 1;
		object_kind = object_kind_from_token_sequence(parser, object_token_index, last_token_index);
		if (object_kind == MYLITE_STATEMENT_OBJECT_NONE) {
			return 0;
		}
		name_token_index = first_name_token_after_object(parser, object_token_index, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        object_kind,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (token_text_equals(parser, token_index, "GRANTS")) {
		name_token_index = find_show_grants_name_token(parser, token_index + 1, last_token_index);
		if (name_token_index >= parser->token_count) {
			return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_USER);
		}
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_USER,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (classify_show_diagnostics_statement_object(parser, statement, token_index, last_token_index)) {
		return 1;
	}

	if (classify_show_database_statement_object(parser, statement, token_index, last_token_index)) {
		return 1;
	}

	if (classify_show_collection_statement_object(parser, statement, token_index, last_token_index)) {
		return 1;
	}

	if (classify_show_binary_log_statement_object(parser, statement, token_index, last_token_index)) {
		return 1;
	}

	if (classify_show_replica_statement_object(parser, statement, token_index, last_token_index)) {
		return 1;
	}

	if (classify_show_variable_statement_object(parser, statement, token_index, last_token_index)) {
		return 1;
	}

	if (classify_show_character_set_statement_object(parser, statement, token_index, last_token_index)) {
		return 1;
	}

	if (token_text_equals(parser, token_index, "RELAYLOG") &&
	    token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index + 1, "EVENTS")) {
		name_token_index = find_show_binlog_events_name_token(parser, token_index + 2, last_token_index);
		if (name_token_index < parser->token_count) {
			return set_statement_direct_object_name(parser,
			                                        statement,
			                                        MYLITE_STATEMENT_OBJECT_RELAY_LOG,
			                                        name_token_index,
			                                        last_token_index);
		}
		name_token_index = find_replication_channel_name_token(parser, token_index + 2, last_token_index);
		if (name_token_index >= parser->token_count &&
		    has_replication_channel_clause(parser, token_index + 2, last_token_index)) {
			return 0;
		}
		if (name_token_index >= parser->token_count) {
			return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_RELAY_LOG);
		}
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_REPLICATION_CHANNEL,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (token_text_equals(parser, token_index, "PROFILE")) {
		name_token_index = find_show_profile_query_id_token(parser, token_index + 1, last_token_index);
		if (name_token_index >= parser->token_count &&
		    has_show_profile_for_query_clause(parser, token_index + 1, last_token_index)) {
			return 0;
		}
		if (name_token_index >= parser->token_count) {
			return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_QUERY);
		}
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_QUERY,
		                                        name_token_index,
		                                        last_token_index);
	}
	if (token_text_equals(parser, token_index, "PROFILES")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_QUERY);
	}

	if (token_text_equals(parser, token_index, "PARSE_TREE")) {
		if (token_index + 1 <= last_token_index &&
		    token_index + 1 < parser->token_count &&
		    parser->tokens[token_index + 1].parser_token == SELECT_T) {
			return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_QUERY);
		}
		return 0;
	}

	if (classify_show_routine_status_statement_object(parser, statement, token_index, last_token_index)) {
		return 1;
	}

	if (parser->tokens[token_index].parser_token == ENGINE_T &&
	    token_index + 2 <= last_token_index &&
	    token_can_start_object_name(&parser->tokens[token_index + 1]) &&
	    (token_text_equals(parser, token_index + 2, "STATUS") ||
	     token_text_equals(parser, token_index + 2, "MUTEX") ||
	     token_text_equals(parser, token_index + 2, "LOGS"))) {
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_ENGINE,
		                                        token_index + 1,
		                                        last_token_index);
	}

	if ((parser->tokens[token_index].parser_token == FUNCTION_T ||
	     parser->tokens[token_index].parser_token == PROCEDURE_T) &&
	    token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index + 1, "CODE")) {
		object_kind = parser->tokens[token_index].parser_token == FUNCTION_T ?
			MYLITE_STATEMENT_OBJECT_FUNCTION :
			MYLITE_STATEMENT_OBJECT_PROCEDURE;
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        object_kind,
		                                        token_index + 2,
		                                        last_token_index);
	}

	if (token_text_equals(parser, token_index, "COLUMNS") ||
	    token_text_equals(parser, token_index, "FIELDS")) {
		name_token_index = find_show_from_name_token(parser, token_index + 1, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_TABLE,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (parser->tokens[token_index].parser_token == INDEX_T ||
	    parser->tokens[token_index].parser_token == KEY_T ||
	    token_text_equals(parser, token_index, "INDEXES") ||
	    token_text_equals(parser, token_index, "KEYS")) {
		name_token_index = find_show_from_name_token(parser, token_index + 1, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_TABLE,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (token_text_equals(parser, token_index, "TABLES")) {
		return classify_show_schema_collection_target(parser,
		                                             statement,
		                                             token_index + 1,
		                                             last_token_index,
		                                             MYLITE_STATEMENT_OBJECT_TABLE,
		                                             MYLITE_STATEMENT_OBJECT_TABLE);
	}

	if (parser->tokens[token_index].parser_token == TABLE_T &&
	    token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index + 1, "STATUS")) {
		return classify_show_schema_collection_target(parser,
		                                             statement,
		                                             token_index + 2,
		                                             last_token_index,
		                                             MYLITE_STATEMENT_OBJECT_TABLE,
		                                             MYLITE_STATEMENT_OBJECT_TABLE);
	}

	if (parser->tokens[token_index].parser_token == OPEN_T &&
	    token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index + 1, "TABLES")) {
		return classify_show_schema_collection_target(parser,
		                                             statement,
		                                             token_index + 2,
		                                             last_token_index,
		                                             MYLITE_STATEMENT_OBJECT_TABLE,
		                                             MYLITE_STATEMENT_OBJECT_TABLE);
	}

	if (token_text_equals(parser, token_index, "EVENTS")) {
		return classify_show_schema_collection_target(parser,
		                                             statement,
		                                             token_index + 1,
		                                             last_token_index,
		                                             MYLITE_STATEMENT_OBJECT_EVENT,
		                                             MYLITE_STATEMENT_OBJECT_EVENT);
	}

	if (token_text_equals(parser, token_index, "TRIGGERS")) {
		return classify_show_schema_collection_target(parser,
		                                             statement,
		                                             token_index + 1,
		                                             last_token_index,
		                                             MYLITE_STATEMENT_OBJECT_TRIGGER,
		                                             MYLITE_STATEMENT_OBJECT_TABLE);
	}

	return 0;
}

static int classify_show_diagnostics_statement_object(const mylite_parser *parser,
                                                      mylite_statement *statement,
                                                      size_t token_index,
                                                      size_t last_token_index)
{
	if (token_text_equals(parser, token_index, "WARNINGS") ||
	    token_text_equals(parser, token_index, "ERRORS")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_DIAGNOSTICS_AREA);
	}

	if (!token_text_equals(parser, token_index, "COUNT") ||
	    token_index + 4 > last_token_index ||
	    token_index + 4 >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index + 1].parser_token != '(' ||
	    parser->tokens[token_index + 2].parser_token != '*' ||
	    parser->tokens[token_index + 3].parser_token != ')' ||
	    (!token_text_equals(parser, token_index + 4, "WARNINGS") &&
	     !token_text_equals(parser, token_index + 4, "ERRORS"))) {
		return 0;
	}

	return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_DIAGNOSTICS_AREA);
}

static int classify_show_database_statement_object(const mylite_parser *parser,
                                                   mylite_statement *statement,
                                                   size_t token_index,
                                                   size_t last_token_index)
{
	size_t name_token_index;

	if (!token_text_equals(parser, token_index, "DATABASES") &&
	    !token_text_equals(parser, token_index, "SCHEMAS")) {
		return 0;
	}

	name_token_index = find_show_like_pattern_token(parser, token_index + 1, last_token_index);
	if (name_token_index < parser->token_count) {
		return set_statement_direct_object_name_range(parser,
		                                              statement,
		                                              MYLITE_STATEMENT_OBJECT_DATABASE,
		                                              name_token_index,
		                                              name_token_index);
	}

	return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_DATABASE);
}

static int classify_show_collection_statement_object(const mylite_parser *parser,
                                                     mylite_statement *statement,
                                                     size_t token_index,
                                                     size_t last_token_index)
{
	if (token_text_equals(parser, token_index, "ENGINES")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_ENGINE);
	}
	if (token_text_equals(parser, token_index, "STORAGE") &&
	    token_index + 1 <= last_token_index &&
	    token_index + 1 < parser->token_count &&
	    token_text_equals(parser, token_index + 1, "ENGINES")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_ENGINE);
	}
	if (token_text_equals(parser, token_index, "PLUGINS")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_PLUGIN);
	}
	if (token_text_equals(parser, token_index, "PRIVILEGES")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_PRIVILEGE);
	}
	if (token_text_equals(parser, token_index, "PROCESSLIST")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_CONNECTION);
	}
	return 0;
}

static int classify_show_binary_log_statement_object(const mylite_parser *parser,
                                                     mylite_statement *statement,
                                                     size_t token_index,
                                                     size_t last_token_index)
{
	size_t name_token_index;

	if (parser->tokens[token_index].parser_token == BINLOG_T &&
	    token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index + 1, "EVENTS")) {
		name_token_index = find_show_binlog_events_name_token(parser, token_index + 2, last_token_index);
		if (name_token_index < parser->token_count) {
			return set_statement_direct_object_name(parser,
			                                        statement,
			                                        MYLITE_STATEMENT_OBJECT_BINARY_LOG,
			                                        name_token_index,
			                                        last_token_index);
		}
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_BINARY_LOG);
	}

	if (token_text_equals(parser, token_index, "BINARY") &&
	    token_index + 1 <= last_token_index &&
	    token_index + 1 < parser->token_count &&
	    (token_text_equals(parser, token_index + 1, "LOGS") ||
	     (token_text_equals(parser, token_index + 1, "LOG") &&
	      token_index + 2 <= last_token_index &&
	      token_index + 2 < parser->token_count &&
	      token_text_equals(parser, token_index + 2, "STATUS")))) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_BINARY_LOG);
	}

	if (token_text_equals(parser, token_index, "MASTER") &&
	    token_index + 1 <= last_token_index &&
	    token_index + 1 < parser->token_count &&
	    (token_text_equals(parser, token_index + 1, "STATUS") ||
	     token_text_equals(parser, token_index + 1, "LOGS"))) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_BINARY_LOG);
	}

	return 0;
}

static int classify_show_replica_statement_object(const mylite_parser *parser,
                                                  mylite_statement *statement,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	size_t name_token_index;

	if (token_text_equals(parser, token_index, "REPLICAS")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_REPLICATION_CHANNEL);
	}

	if (token_text_equals(parser, token_index, "SLAVE") &&
	    token_index + 1 <= last_token_index &&
	    token_index + 1 < parser->token_count &&
	    token_text_equals(parser, token_index + 1, "HOSTS")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_REPLICATION_CHANNEL);
	}

	if ((!token_text_equals(parser, token_index, "REPLICA") &&
	     !token_text_equals(parser, token_index, "SLAVE")) ||
	    token_index + 1 > last_token_index ||
	    token_index + 1 >= parser->token_count ||
	    !token_text_equals(parser, token_index + 1, "STATUS")) {
		return 0;
	}

	name_token_index = find_replication_channel_name_token(parser, token_index + 2, last_token_index);
	if (name_token_index < parser->token_count) {
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_REPLICATION_CHANNEL,
		                                        name_token_index,
		                                        last_token_index);
	}
	return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_REPLICATION_CHANNEL);
}

static int classify_show_routine_status_statement_object(const mylite_parser *parser,
                                                         mylite_statement *statement,
                                                         size_t token_index,
                                                         size_t last_token_index)
{
	mylite_statement_object_kind object_kind;
	size_t name_token_index;

	if ((parser->tokens[token_index].parser_token != FUNCTION_T &&
	     parser->tokens[token_index].parser_token != PROCEDURE_T) ||
	    token_index + 1 > last_token_index ||
	    token_index + 1 >= parser->token_count ||
	    !token_text_equals(parser, token_index + 1, "STATUS")) {
		return 0;
	}

	object_kind = parser->tokens[token_index].parser_token == FUNCTION_T ?
		MYLITE_STATEMENT_OBJECT_FUNCTION :
		MYLITE_STATEMENT_OBJECT_PROCEDURE;
	name_token_index = find_show_like_pattern_token(parser, token_index + 2, last_token_index);
	if (name_token_index < parser->token_count) {
		return set_statement_direct_object_name_range(parser,
		                                              statement,
		                                              object_kind,
		                                              name_token_index,
		                                              name_token_index);
	}
	return set_statement_direct_object(statement, object_kind);
}

static int classify_show_variable_statement_object(const mylite_parser *parser,
                                                   mylite_statement *statement,
                                                   size_t token_index,
                                                   size_t last_token_index)
{
	mylite_statement_object_kind object_kind;
	size_t name_token_index;

	if ((token_text_equals(parser, token_index, "GLOBAL") ||
	     token_text_equals(parser, token_index, "SESSION") ||
	     token_text_equals(parser, token_index, "LOCAL")) &&
	    token_index + 1 <= last_token_index &&
	    token_index + 1 < parser->token_count) {
		token_index++;
	}

	if (token_text_equals(parser, token_index, "VARIABLES")) {
		object_kind = MYLITE_STATEMENT_OBJECT_SYSTEM_VARIABLE;
	} else if (token_text_equals(parser, token_index, "STATUS")) {
		object_kind = MYLITE_STATEMENT_OBJECT_STATUS_VARIABLE;
	} else {
		return 0;
	}

	name_token_index = find_show_like_pattern_token(parser, token_index + 1, last_token_index);
	if (name_token_index < parser->token_count) {
		return set_statement_direct_object_name_range(parser,
		                                              statement,
		                                              object_kind,
		                                              name_token_index,
		                                              name_token_index);
	}

	return set_statement_direct_object(statement, object_kind);
}

static int classify_show_character_set_statement_object(const mylite_parser *parser,
                                                        mylite_statement *statement,
                                                        size_t token_index,
                                                        size_t last_token_index)
{
	mylite_statement_object_kind object_kind;
	size_t name_token_index;
	size_t first_filter_token;

	if (parser->tokens[token_index].parser_token == CHARACTER_T &&
	    token_index + 1 <= last_token_index &&
	    token_index + 1 < parser->token_count &&
	    parser->tokens[token_index + 1].parser_token == SET_T) {
		object_kind = MYLITE_STATEMENT_OBJECT_CHARACTER_SET;
		first_filter_token = token_index + 2;
	} else if (parser->tokens[token_index].parser_token == CHARSET_T) {
		object_kind = MYLITE_STATEMENT_OBJECT_CHARACTER_SET;
		first_filter_token = token_index + 1;
	} else if (token_text_equals(parser, token_index, "COLLATION")) {
		object_kind = MYLITE_STATEMENT_OBJECT_COLLATION;
		first_filter_token = token_index + 1;
	} else {
		return 0;
	}

	name_token_index = find_show_like_pattern_token(parser, first_filter_token, last_token_index);
	if (name_token_index < parser->token_count) {
		return set_statement_direct_object_name_range(parser,
		                                              statement,
		                                              object_kind,
		                                              name_token_index,
		                                              name_token_index);
	}

	return set_statement_direct_object(statement, object_kind);
}

static int classify_show_schema_collection_target(const mylite_parser *parser,
                                                  mylite_statement *statement,
                                                  size_t first_filter_token,
                                                  size_t last_token_index,
                                                  mylite_statement_object_kind collection_object_kind,
                                                  mylite_statement_object_kind like_pattern_object_kind)
{
	size_t name_token_index = find_show_from_name_token(parser, first_filter_token, last_token_index);
	if (name_token_index < parser->token_count) {
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_DATABASE,
		                                        name_token_index,
		                                        last_token_index);
	}

	name_token_index = find_show_like_pattern_token(parser, first_filter_token, last_token_index);
	if (name_token_index < parser->token_count) {
		return set_statement_direct_object_name_range(parser,
		                                              statement,
		                                              like_pattern_object_kind,
		                                              name_token_index,
		                                              name_token_index);
	}

	return set_statement_direct_object(statement, collection_object_kind);
}

static size_t find_show_profile_query_id_token(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index)
{
	while (token_index + 2 <= last_token_index && token_index < parser->token_count) {
		if (token_text_equals(parser, token_index, "FOR") &&
		    token_text_equals(parser, token_index + 1, "QUERY") &&
		    parser->tokens[token_index + 2].kind == MYLITE_TOKEN_NUMBER) {
			return token_index + 2;
		}
		token_index++;
	}
	return parser->token_count;
}

static int has_show_profile_for_query_clause(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (token_text_equals(parser, token_index, "FOR") &&
		    token_text_equals(parser, token_index + 1, "QUERY")) {
			return 1;
		}
		token_index++;
	}
	return 0;
}

static size_t find_show_like_pattern_token(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (token_text_equals(parser, token_index, "LIKE") &&
		    parser->tokens[token_index + 1].kind == MYLITE_TOKEN_STRING) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_show_binlog_events_name_token(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == IN_T &&
		    token_can_start_object_name(&parser->tokens[token_index + 1])) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t skip_show_modifiers(const mylite_parser *parser,
                                  size_t token_index,
                                  size_t last_token_index)
{
	while (token_index <= last_token_index && is_show_modifier_token(parser, token_index)) {
		token_index++;
	}
	return token_index;
}

static int is_show_modifier_token(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "FULL") ||
	       token_text_equals(parser, token_index, "EXTENDED");
}

static size_t find_show_from_name_token(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if ((parser->tokens[token_index].parser_token == FROM_T ||
		     parser->tokens[token_index].parser_token == IN_T) &&
		    token_can_start_object_name(&parser->tokens[token_index + 1])) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_show_grants_name_token(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (token_text_equals(parser, token_index, "FOR") &&
		    token_can_start_object_name(&parser->tokens[token_index + 1])) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static int set_statement_direct_object(mylite_statement *statement, mylite_statement_object_kind object_kind)
{
	statement->object_kind = object_kind;
	return 1;
}

static int set_statement_direct_object_name(const mylite_parser *parser,
                                            mylite_statement *statement,
                                            mylite_statement_object_kind object_kind,
                                            size_t name_token_index,
                                            size_t last_token_index)
{
	if (name_token_index > last_token_index ||
	    name_token_index >= parser->token_count ||
	    (!token_can_start_object_name(&parser->tokens[name_token_index]) &&
	     object_kind != MYLITE_STATEMENT_OBJECT_USER_VARIABLE &&
	     object_kind != MYLITE_STATEMENT_OBJECT_SYSTEM_VARIABLE)) {
		return 0;
	}

	statement->object_kind = object_kind;
	if (object_kind == MYLITE_STATEMENT_OBJECT_USER ||
	    object_kind == MYLITE_STATEMENT_OBJECT_ROLE) {
		set_statement_account_name_from_first_token(parser, statement, name_token_index, last_token_index);
	} else {
		set_statement_object_name_from_first_token(parser, statement, name_token_index, last_token_index);
	}
	return 1;
}

static int set_statement_direct_object_name_range(const mylite_parser *parser,
                                                  mylite_statement *statement,
                                                  mylite_statement_object_kind object_kind,
                                                  size_t first_name_token,
                                                  size_t last_name_token)
{
	const mylite_token *first;
	const mylite_token *last;

	if (first_name_token > last_name_token ||
	    last_name_token >= parser->token_count) {
		return 0;
	}

	first = &parser->tokens[first_name_token];
	last = &parser->tokens[last_name_token];
	statement->object_kind = object_kind;
	statement->object_name_first_token = first_name_token + 1;
	statement->object_name_last_token = last_name_token + 1;
	statement->object_name_start_offset = first->start_offset;
	statement->object_name_end_offset = last->end_offset;
	statement->object_name_start_line = first->start_line;
	statement->object_name_start_column = first->start_column;
	statement->object_name_end_line = last->end_line;
	statement->object_name_end_column = last->end_column;
	return 1;
}

static mylite_statement_object_kind object_kind_from_token_sequence(const mylite_parser *parser,
                                                                    size_t token_index,
                                                                    size_t last_token_index)
{
	int token = parser->tokens[token_index].parser_token;

	switch (token) {
	case DATABASE_T: return MYLITE_STATEMENT_OBJECT_DATABASE;
	case EVENT_T: return MYLITE_STATEMENT_OBJECT_EVENT;
	case FUNCTION_T: return MYLITE_STATEMENT_OBJECT_FUNCTION;
	case INDEX_T: return MYLITE_STATEMENT_OBJECT_INDEX;
	case PREPARE_T: return MYLITE_STATEMENT_OBJECT_PREPARED_STATEMENT;
	case PROCEDURE_T: return MYLITE_STATEMENT_OBJECT_PROCEDURE;
	case ROLE_T: return MYLITE_STATEMENT_OBJECT_ROLE;
	case SCHEMA_T: return MYLITE_STATEMENT_OBJECT_SCHEMA;
	case TABLE_T: return MYLITE_STATEMENT_OBJECT_TABLE;
	case TRIGGER_T: return MYLITE_STATEMENT_OBJECT_TRIGGER;
	case USER_T: return MYLITE_STATEMENT_OBJECT_USER;
	case VIEW_T: return MYLITE_STATEMENT_OBJECT_VIEW;
	case SPATIAL_T:
		if (token_index + 1 <= last_token_index &&
		    parser->tokens[token_index + 1].parser_token == INDEX_T) {
			return MYLITE_STATEMENT_OBJECT_INDEX;
		}
		if (token_index + 2 <= last_token_index &&
		    parser->tokens[token_index + 1].kind == MYLITE_TOKEN_IDENTIFIER &&
		    parser->tokens[token_index + 2].kind == MYLITE_TOKEN_IDENTIFIER) {
			return MYLITE_STATEMENT_OBJECT_SPATIAL_REFERENCE_SYSTEM;
		}
		return MYLITE_STATEMENT_OBJECT_NONE;
	default:
		if ((parser->tokens[token_index].parser_token == UNIQUE_T ||
		     token_text_equals(parser, token_index, "FULLTEXT")) &&
		    token_index + 1 <= last_token_index &&
		    parser->tokens[token_index + 1].parser_token == INDEX_T) {
			return MYLITE_STATEMENT_OBJECT_INDEX;
		}
		if (token_text_equals(parser, token_index, "AGGREGATE") &&
		    token_index + 1 <= last_token_index &&
		    parser->tokens[token_index + 1].parser_token == FUNCTION_T) {
			return MYLITE_STATEMENT_OBJECT_FUNCTION;
		}
		if (token_text_equals(parser, token_index, "LOGFILE") &&
		    token_index + 1 <= last_token_index &&
		    parser->tokens[token_index + 1].parser_token == GROUP_T) {
			return MYLITE_STATEMENT_OBJECT_LOGFILE_GROUP;
		}
		if (token_text_equals(parser, token_index, "RESOURCE") &&
		    token_index + 1 <= last_token_index &&
		    parser->tokens[token_index + 1].parser_token == GROUP_T) {
			return MYLITE_STATEMENT_OBJECT_RESOURCE_GROUP;
		}
		if (token_text_equals(parser, token_index, "TABLES")) {
			return MYLITE_STATEMENT_OBJECT_TABLE;
		}
		if (token_text_equals(parser, token_index, "SERVER")) {
			return MYLITE_STATEMENT_OBJECT_SERVER;
		}
		if (token_text_equals(parser, token_index, "UNDO") &&
		    token_index + 1 <= last_token_index &&
		    token_text_equals(parser, token_index + 1, "TABLESPACE")) {
			return MYLITE_STATEMENT_OBJECT_UNDO_TABLESPACE;
		}
		if (parser->tokens[token_index].kind == MYLITE_TOKEN_IDENTIFIER &&
		    parser->tokens[token_index].end_offset - parser->tokens[token_index].start_offset == 10 &&
		    strncasecmp("TABLESPACE",
		                parser->lexer.input + parser->tokens[token_index].start_offset,
		                10) == 0) {
			return MYLITE_STATEMENT_OBJECT_TABLESPACE;
		}
		return MYLITE_STATEMENT_OBJECT_NONE;
	}
}

static void set_statement_object_name(const mylite_parser *parser,
                                      mylite_statement *statement,
                                      size_t object_token_index,
                                      size_t last_token_index)
{
	size_t first_name_token = first_name_token_after_object(parser, object_token_index, last_token_index);

	if (statement->kind == MYLITE_STATEMENT_ALTER &&
	    (statement->object_kind == MYLITE_STATEMENT_OBJECT_DATABASE ||
	     statement->object_kind == MYLITE_STATEMENT_OBJECT_SCHEMA) &&
	    token_starts_alter_database_option(parser, object_token_index + 1, last_token_index)) {
		return;
	}

	if (statement->object_kind == MYLITE_STATEMENT_OBJECT_USER ||
	    statement->object_kind == MYLITE_STATEMENT_OBJECT_ROLE) {
		set_statement_account_name_from_first_token(parser, statement, first_name_token, last_token_index);
		return;
	}

	set_statement_object_name_from_first_token(parser, statement, first_name_token, last_token_index);
}

static int token_starts_alter_database_option(const mylite_parser *parser,
                                              size_t token_index,
                                              size_t last_token_index)
{
	return token_starts_database_option(parser, token_index, last_token_index, 1);
}

static void set_statement_object_name_from_first_token(const mylite_parser *parser,
                                                       mylite_statement *statement,
                                                       size_t first_name_token,
                                                       size_t last_token_index)
{
	size_t last_name_token;
	const mylite_token *first;
	const mylite_token *last;

	if (first_name_token >= parser->token_count) {
		return;
	}

	last_name_token = last_qualified_name_token(parser, first_name_token, last_token_index);
	first = &parser->tokens[first_name_token];
	last = &parser->tokens[last_name_token];

	statement->object_name_first_token = first_name_token + 1;
	statement->object_name_last_token = last_name_token + 1;
	statement->object_name_start_offset = first->start_offset;
	statement->object_name_end_offset = last->end_offset;
	statement->object_name_start_line = first->start_line;
	statement->object_name_start_column = first->start_column;
	statement->object_name_end_line = last->end_line;
	statement->object_name_end_column = last->end_column;
}

static size_t first_name_token_after_object(const mylite_parser *parser,
                                            size_t object_token_index,
                                            size_t last_token_index)
{
	size_t token_index = object_token_index + 1;

	if (parser->tokens[object_token_index].parser_token == SPATIAL_T &&
	    token_index + 2 <= last_token_index &&
	    parser->tokens[token_index].kind == MYLITE_TOKEN_IDENTIFIER &&
	    parser->tokens[token_index + 1].kind == MYLITE_TOKEN_IDENTIFIER) {
		token_index += 2;
	}
	if (token_text_equals(parser, object_token_index, "LOGFILE") &&
	    token_index <= last_token_index &&
	    parser->tokens[token_index].parser_token == GROUP_T) {
		token_index++;
	}
	if (token_text_equals(parser, object_token_index, "RESOURCE") &&
	    token_index <= last_token_index &&
	    parser->tokens[token_index].parser_token == GROUP_T) {
		token_index++;
	}
	if (token_text_equals(parser, object_token_index, "AGGREGATE") &&
	    token_index <= last_token_index &&
	    parser->tokens[token_index].parser_token == FUNCTION_T) {
		token_index++;
	}
	if (token_is_create_index_modifier(parser, object_token_index) &&
	    token_index <= last_token_index &&
	    parser->tokens[token_index].parser_token == INDEX_T) {
		token_index++;
	}
	if (token_text_equals(parser, object_token_index, "UNDO") &&
	    token_index <= last_token_index &&
	    token_text_equals(parser, token_index, "TABLESPACE")) {
		token_index++;
	}

	while (token_index <= last_token_index) {
		if (is_optional_name_modifier(parser->tokens[token_index].parser_token)) {
			token_index++;
			continue;
		}
		if (token_can_start_object_name(&parser->tokens[token_index])) {
			return token_index;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t last_qualified_name_token(const mylite_parser *parser,
                                        size_t first_name_token,
                                        size_t last_token_index)
{
	size_t token_index = first_name_token;

	while (token_index + 2 <= last_token_index &&
	       parser->tokens[token_index + 1].parser_token == '.' &&
	       token_can_continue_qualified_object_name(&parser->tokens[token_index + 2])) {
		token_index += 2;
	}
	return token_index;
}

static int token_can_start_object_name(const mylite_token *token)
{
	return token->kind == MYLITE_TOKEN_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_QUOTED_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_STRING ||
	       token->kind == MYLITE_TOKEN_NUMBER ||
	       token_can_be_unquoted_object_name_keyword(token->parser_token);
}

static int token_can_continue_object_name(const mylite_token *token)
{
	return token->kind == MYLITE_TOKEN_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_QUOTED_IDENTIFIER ||
	       token_can_be_unquoted_object_name_keyword(token->parser_token);
}

static int token_can_continue_qualified_object_name(const mylite_token *token)
{
	return token_can_continue_object_name(token) ||
	       token->kind == MYLITE_TOKEN_KEYWORD;
}

static int token_can_be_unquoted_object_name_keyword(int token)
{
	switch (token) {
	case AUTO_INCREMENT_T:
	case BEGIN_T:
	case BINLOG_T:
	case CACHE_T:
	case CHARSET_T:
	case CHAIN_T:
	case CHECKSUM_T:
	case CLONE_T:
	case CLOSE_T:
	case COMMIT_T:
	case CURSOR_T:
	case COLUMNS_T:
	case DATA_T:
	case DECLARE_T:
	case DEALLOCATE_T:
	case DO_T:
	case END_T:
	case ENGINE_T:
	case EVENT_T:
	case EXECUTE_T:
	case FETCH_T:
	case FIELDS_T:
	case FLUSH_T:
	case FORMAT_T:
	case FULL_T:
	case HANDLER_T:
	case HELP_T:
	case IMPORT_T:
	case INFILE_T:
	case INSTALL_T:
	case ITERATE_T:
	case JSON_T:
	case LEAVE_T:
	case LOCAL_T:
	case NO_T:
	case OFFSET_T:
	case OPEN_T:
	case PREPARE_T:
	case QUICK_T:
	case READ_T:
	case REPAIR_T:
	case RESET_T:
	case RESTART_T:
	case ROLE_T:
	case ROLLBACK_T:
	case RETURN_T:
	case SAVEPOINT_T:
	case SHUTDOWN_T:
	case START_T:
	case STOP_T:
	case TEMPORARY_T:
	case TO_T:
	case TRUNCATE_T:
	case TRANSACTION_T:
	case UNINSTALL_T:
	case UNTIL_T:
	case USER_T:
	case VALUE_T:
	case VIEW_T:
	case WRITE_T:
	case XA_T:
		return 1;
	default:
		return 0;
	}
}

static int token_can_be_unquoted_local_name_keyword(int token)
{
	switch (token) {
	case CHAIN_T:
	case CLOSE_T:
	case CURSOR_T:
	case COLUMNS_T:
	case DATA_T:
	case DECLARE_T:
	case FETCH_T:
	case FIELDS_T:
	case FORMAT_T:
	case FULL_T:
	case INFILE_T:
	case ITERATE_T:
	case JSON_T:
	case LEAVE_T:
	case LOCAL_T:
	case NO_T:
	case OPEN_T:
	case READ_T:
	case RETURN_T:
	case START_T:
	case STOP_T:
	case TO_T:
	case TRANSACTION_T:
	case VALUE_T:
	case WRITE_T:
		return 1;
	default:
		return 0;
	}
}

static mylite_statement_object_kind variable_object_kind_from_token(const mylite_token *token)
{
	if (token->kind == MYLITE_TOKEN_USER_VARIABLE) {
		return MYLITE_STATEMENT_OBJECT_USER_VARIABLE;
	}
	if (token->kind == MYLITE_TOKEN_SYSTEM_VARIABLE) {
		return MYLITE_STATEMENT_OBJECT_SYSTEM_VARIABLE;
	}
	if (token_can_start_local_variable_name(token)) {
		return MYLITE_STATEMENT_OBJECT_LOCAL_VARIABLE;
	}
	return MYLITE_STATEMENT_OBJECT_NONE;
}

static int token_can_start_local_variable_name(const mylite_token *token)
{
	return token->kind == MYLITE_TOKEN_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_QUOTED_IDENTIFIER ||
	       token_can_be_unquoted_local_name_keyword(token->parser_token);
}

static int token_can_start_label_name(const mylite_token *token)
{
	return token->kind == MYLITE_TOKEN_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_QUOTED_IDENTIFIER ||
	       token_can_be_unquoted_label_keyword(token->parser_token);
}

static int token_can_be_unquoted_label_keyword(int token)
{
	switch (token) {
	case AUTO_INCREMENT_T:
	case BINLOG_T:
	case CHAIN_T:
	case CLOSE_T:
	case COLUMNS_T:
	case DATA_T:
	case ENGINE_T:
	case EVENT_T:
	case FIELDS_T:
	case FORMAT_T:
	case FULL_T:
	case INFILE_T:
	case JSON_T:
	case LOCAL_T:
	case OFFSET_T:
	case OPEN_T:
	case QUICK_T:
	case ROLE_T:
	case TEMPORARY_T:
	case TRANSACTION_T:
	case UNTIL_T:
	case USER_T:
	case VALUE_T:
	case VIEW_T:
		return 1;
	default:
		return 0;
	}
}

static int is_optional_name_modifier(int token)
{
	return token == IF_T ||
	       token == NOT_T ||
	       token == EXISTS_T;
}

static int token_text_equals(const mylite_parser *parser, size_t token_index, const char *expected)
{
	const mylite_token *token;
	size_t expected_length = strlen(expected);

	if (token_index >= parser->token_count) {
		return 0;
	}

	token = &parser->tokens[token_index];
	return token->end_offset - token->start_offset == expected_length &&
	       strncasecmp(expected, parser->lexer.input + token->start_offset, expected_length) == 0;
}

static int token_is_assignment_operator(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "=") ||
	       token_text_equals(parser, token_index, ":=");
}

static int statement_kind_uses_object_scan(mylite_statement_kind kind)
{
	switch (kind) {
	case MYLITE_STATEMENT_CREATE:
	case MYLITE_STATEMENT_ALTER:
	case MYLITE_STATEMENT_DROP:
	case MYLITE_STATEMENT_TRUNCATE:
	case MYLITE_STATEMENT_RENAME:
		return 1;
	default:
		return 0;
	}
}

static mylite_token_kind token_kind_from_parser_token(int token)
{
	switch (token) {
	case IDENT: return MYLITE_TOKEN_IDENTIFIER;
	case QUOTED_IDENT: return MYLITE_TOKEN_QUOTED_IDENTIFIER;
	case STRING: return MYLITE_TOKEN_STRING;
	case NUMBER: return MYLITE_TOKEN_NUMBER;
	case PARAM: return MYLITE_TOKEN_PARAMETER;
	case USER_VARIABLE: return MYLITE_TOKEN_USER_VARIABLE;
	case SYSTEM_VARIABLE: return MYLITE_TOKEN_SYSTEM_VARIABLE;
	case OPERATOR: return MYLITE_TOKEN_OPERATOR;
	default:
		if (token > 0 && token < 256) {
			return MYLITE_TOKEN_PUNCTUATION;
		}
		return MYLITE_TOKEN_KEYWORD;
	}
}
