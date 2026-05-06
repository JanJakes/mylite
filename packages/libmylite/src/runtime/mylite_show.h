#ifndef MYLITE_RUNTIME_MYLITE_SHOW_H
#define MYLITE_RUNTIME_MYLITE_SHOW_H

#include <mylite/mylite.h>

#include "sql/mylite_ast.h"

struct mylite_show_character_set_query;
struct mylite_show_columns_query;
struct mylite_show_columns_source_nodes;
struct mylite_show_columns_target;
struct mylite_show_collation_query;
struct mylite_show_diagnostics_query;
struct mylite_show_index_query;
struct mylite_show_status_query;
struct mylite_show_table_status_query;
struct mylite_show_tables_query;
struct mylite_show_variables_query;

int mylite_show_prepare_character_set_statement(mylite_db *database,
                                                const struct mylite_sql_ast_node *statement,
                                                mylite_stmt **out_stmt);
int mylite_show_prepare_collation_statement(mylite_db *database,
                                            const struct mylite_sql_ast_node *statement,
                                            mylite_stmt **out_stmt);
int mylite_show_prepare_columns_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          mylite_stmt **out_stmt);
int mylite_show_prepare_create_table_statement(mylite_db *database,
                                               const struct mylite_sql_ast_node *statement,
                                               mylite_stmt **out_stmt);
int mylite_show_prepare_create_schema_statement(mylite_db *database,
                                                const struct mylite_sql_ast_node *statement,
                                                mylite_stmt **out_stmt);
int mylite_show_prepare_describe_table_statement(mylite_db *database,
                                                 const struct mylite_sql_ast_node *statement,
                                                 mylite_stmt **out_stmt);
int mylite_show_prepare_diagnostics_statement(mylite_db *database,
                                              const struct mylite_sql_ast_node *statement,
                                              mylite_stmt **out_stmt);
int mylite_show_prepare_diagnostics_count_statement(mylite_db *database,
                                                    const struct mylite_sql_ast_node *statement,
                                                    mylite_stmt **out_stmt);
int mylite_show_prepare_engines_statement(mylite_db *database, mylite_stmt **out_stmt);
int mylite_show_prepare_schemas_statement(mylite_db *database, mylite_stmt **out_stmt);
int mylite_show_prepare_status_statement(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         mylite_stmt **out_stmt);
int mylite_show_prepare_index_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt);
int mylite_show_prepare_tables_statement(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         mylite_stmt **out_stmt);
int mylite_show_prepare_table_status_statement(mylite_db *database,
                                               const struct mylite_sql_ast_node *statement,
                                               mylite_stmt **out_stmt);
int mylite_show_prepare_variables_statement(mylite_db *database,
                                            const struct mylite_sql_ast_node *statement,
                                            mylite_stmt **out_stmt);
int mylite_show_character_set_sql(mylite_db *database,
                                  const struct mylite_show_character_set_query *query,
                                  char **out_sql);
char *mylite_show_columns_sql(mylite_db *database, const struct mylite_show_columns_query *query);
int mylite_show_collation_sql(mylite_db *database, const struct mylite_show_collation_query *query,
                              char **out_sql);
int mylite_show_index_sql(mylite_db *database, const struct mylite_show_index_query *query,
                          char **out_sql);
char *mylite_show_diagnostics_sql(mylite_db *database,
                                  const struct mylite_show_diagnostics_query *query);
char *mylite_show_diagnostics_count_sql(mylite_db *database,
                                        enum mylite_sql_ast_show_diagnostics_kind kind);
int mylite_show_status_sql(mylite_db *database, const struct mylite_show_status_query *query,
                           char **out_sql);
char *mylite_show_tables_sql(mylite_db *database, const struct mylite_show_tables_query *query);
char *mylite_show_table_status_sql(mylite_db *database,
                                   const struct mylite_show_table_status_query *query);
int mylite_show_variables_sql(mylite_db *database, const struct mylite_show_variables_query *query,
                              char **out_sql);
int mylite_show_copy_columns_table_target(const struct mylite_show_columns_source_nodes *source,
                                          struct mylite_show_columns_target *out_target);
int mylite_show_copy_columns_selected_schema(mylite_db *database,
                                             struct mylite_show_columns_target *target);
int mylite_show_validate_columns_target(mylite_db *database,
                                        struct mylite_show_columns_target *target,
                                        const char *information_schema_unsupported_message);
void mylite_show_columns_target_deinit(struct mylite_show_columns_target *target);
char *mylite_show_copy_like_pattern_span(const struct mylite_sql_ast_node *node);
const char *mylite_show_schemas_sql(void);
int mylite_show_attach_engines_result_metadata(mylite_db *database, mylite_stmt *stmt);

#endif
