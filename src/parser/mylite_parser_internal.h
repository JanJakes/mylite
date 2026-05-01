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
void mylite_parser_require_token_text(MyliteParseContext *ctx,
                                      MyliteToken token,
                                      const char *text);
void mylite_parser_require_token_text_any(MyliteParseContext *ctx,
                                          MyliteToken token,
                                          const char *first,
                                          const char *second);
void mylite_parser_require_udf_return_type(MyliteParseContext *ctx,
                                           MyliteToken token);
void mylite_parser_require_event_statement_atom(MyliteParseContext *ctx,
                                                MyliteToken token);
void mylite_parser_require_create_procedure_tail_atom(MyliteParseContext *ctx,
                                                      MyliteToken token);
void mylite_parser_require_create_table_tail_atom(MyliteParseContext *ctx,
                                                  MyliteToken token);
void mylite_parser_require_alter_table_action_start(MyliteParseContext *ctx,
                                                    MyliteToken token);
void mylite_parser_require_token_prefix(MyliteParseContext *ctx,
                                        MyliteToken token,
                                        const char *prefix);
void mylite_parser_require_profile_type(MyliteParseContext *ctx,
                                        MyliteToken first,
                                        MyliteToken second);
void mylite_parser_require_start_until_log_pair(MyliteParseContext *ctx,
                                                MyliteToken file,
                                                MyliteToken pos);
void mylite_parser_require_check_table_option(MyliteParseContext *ctx,
                                              MyliteToken token);
void mylite_parser_require_explain_format(MyliteParseContext *ctx,
                                          MyliteToken token);
void mylite_parser_require_diagnostics_statement_item(MyliteParseContext *ctx,
                                                      MyliteToken token);
void mylite_parser_require_diagnostics_condition_item(MyliteParseContext *ctx,
                                                      MyliteToken token);
void mylite_parser_require_signal_condition_item(MyliteParseContext *ctx,
                                                 MyliteToken token);
void mylite_parser_require_permissive(MyliteParseContext *ctx,
                                      MyliteToken token);

void *MyLiteLemonAlloc(void *(*malloc_proc)(size_t));
void MyLiteLemon(void *parser, int token_id, MyliteToken token,
                 MyliteParseContext *ctx);
void MyLiteLemonFree(void *parser, void (*free_proc)(void *));

#endif
