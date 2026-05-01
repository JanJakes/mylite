#ifndef MYLITE_PARSER_INTERNAL_H
#define MYLITE_PARSER_INTERNAL_H

#include <stddef.h>

#include "mylite_parser.h"

typedef struct MyliteToken {
  const char *start;
  size_t length;
  size_t offset;
  size_t line;
  size_t column;
} MyliteToken;

typedef struct MyliteParseContext {
  const char *sql;
  size_t length;
  int permissive;
  size_t permissive_fallbacks;
  int accepted;
  int failed;
  MyliteParseResult *result;
} MyliteParseContext;

void mylite_parser_accept(MyliteParseContext *ctx);
void mylite_parser_failure(MyliteParseContext *ctx);
void mylite_parser_syntax_error(MyliteParseContext *ctx, int token_id,
                                MyliteToken token);
void mylite_parser_record_statement(MyliteParseContext *ctx,
                                    MyliteStatementKind kind);
void mylite_parser_record_empty_statement(MyliteParseContext *ctx);
void mylite_parser_validate_select_statement(MyliteParseContext *ctx);
void mylite_parser_validate_select_statement_from(MyliteParseContext *ctx,
                                                  MyliteToken start);
void mylite_parser_validate_table_statement_from(MyliteParseContext *ctx,
                                                 MyliteToken start);
void mylite_parser_validate_parenthesized_statement(MyliteParseContext *ctx,
                                                    MyliteToken start);
void mylite_parser_validate_dml_statement(MyliteParseContext *ctx,
                                          MyliteToken start,
                                          MyliteStatementKind kind);
void mylite_parser_validate_handler_statement(MyliteParseContext *ctx,
                                              MyliteToken start);
void mylite_parser_validate_do_statement(MyliteParseContext *ctx,
                                          MyliteToken start);
void mylite_parser_validate_kill_statement(MyliteParseContext *ctx,
                                            MyliteToken start);
void mylite_parser_validate_reset_statement(MyliteParseContext *ctx,
                                            MyliteToken start);
void mylite_parser_validate_set_statement(MyliteParseContext *ctx,
                                           MyliteToken start);
void mylite_parser_validate_show_statement(MyliteParseContext *ctx,
                                            MyliteToken start);
void mylite_parser_validate_values_statement_from(MyliteParseContext *ctx,
                                                  MyliteToken start);
void mylite_parser_validate_parenthesized_expression_list_from(
    MyliteParseContext *ctx, MyliteToken start, const char *message);
void mylite_parser_validate_expression_from(MyliteParseContext *ctx,
                                            MyliteToken start,
                                            const char *message);
void mylite_parser_validate_expression_until_from(MyliteParseContext *ctx,
                                                  MyliteToken start,
                                                  int boundary_token_id,
                                                  const char *message);
void mylite_parser_validate_declare_statement(MyliteParseContext *ctx,
                                               MyliteToken start);
void mylite_parser_validate_explain_statement(MyliteParseContext *ctx,
                                               MyliteToken start);
void mylite_parser_validate_with_statement_from(MyliteParseContext *ctx,
                                                MyliteToken start);
void mylite_parser_validate_create_index_statement(MyliteParseContext *ctx,
                                                   MyliteToken start);
void mylite_parser_validate_create_table_statement(MyliteParseContext *ctx,
                                                   MyliteToken start);
void mylite_parser_validate_alter_table_statement(MyliteParseContext *ctx,
                                                  MyliteToken start);
void mylite_parser_validate_view_statement(MyliteParseContext *ctx,
                                            MyliteToken start);
void mylite_parser_validate_event_statement(MyliteParseContext *ctx,
                                             MyliteToken start);
void mylite_parser_validate_trigger_statement(MyliteParseContext *ctx,
                                               MyliteToken start);
void mylite_parser_validate_create_function_statement(MyliteParseContext *ctx,
                                                       MyliteToken start);
void mylite_parser_validate_create_procedure_statement(MyliteParseContext *ctx,
                                                        MyliteToken start);
void mylite_parser_require_permissive(MyliteParseContext *ctx,
                                       MyliteToken token);
void mylite_parser_require_row_format(MyliteParseContext *ctx,
                                      MyliteToken token);
void mylite_parser_require_storage_type(MyliteParseContext *ctx,
                                        MyliteToken token);
void mylite_parser_require_xid_number(MyliteParseContext *ctx,
                                      MyliteToken token);
void mylite_parser_require_name_atom(MyliteParseContext *ctx,
                                     MyliteToken token);
void mylite_parser_require_identifier_atom(MyliteParseContext *ctx,
                                           MyliteToken token);
void mylite_parser_require_charset_name_atom(MyliteParseContext *ctx,
                                             MyliteToken token);
void mylite_parser_reject(MyliteParseContext *ctx, MyliteToken token,
                          const char *message);

void *MyLiteLemonAlloc(void *(*malloc_proc)(size_t));
void MyLiteLemon(void *parser, int token_id, MyliteToken token,
                 MyliteParseContext *ctx);
void MyLiteLemonFree(void *parser, void (*free_proc)(void *));

#endif
