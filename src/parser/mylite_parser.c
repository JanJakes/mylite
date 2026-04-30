#include "mylite_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mylite_lexer.h"
#include "mylite_parser_internal.h"
#include "generated/mylite_lemon.h"

static void result_init(MyliteParseResult *result);
static MyliteParseStatus parse_sql(const char *sql, size_t length,
                                   int permissive,
                                   MyliteParseResult *result);
static void set_parser_error(MyliteParseContext *ctx, const MyliteToken *token,
                             const char *message);
static void format_near_token(MyliteParseContext *ctx, int token_id,
                              const MyliteToken *token);

MyliteParseStatus mylite_parse_sql(const char *sql, size_t length,
                                   MyliteParseResult *result) {
  return parse_sql(sql, length, 0, result);
}

MyliteParseStatus mylite_parse_sql_permissive(const char *sql, size_t length,
                                              MyliteParseResult *result) {
  return parse_sql(sql, length, 1, result);
}

const char *mylite_statement_kind_name(MyliteStatementKind kind) {
  static const char *const names[] = {
      "empty",
      "select",
      "insert",
      "replace",
      "update",
      "delete",
      "ddl",
      "transaction",
      "prepared",
      "show",
      "utility",
      "admin",
      "stored_program",
      "replication",
      "permissive",
  };

  if (kind >= MYLITE_STATEMENT_KIND_COUNT) {
    return "unknown";
  }

  return names[kind];
}

static MyliteParseStatus parse_sql(const char *sql, size_t length,
                                   int permissive,
                                   MyliteParseResult *result) {
  MyliteParseResult local_result;
  MyliteParseContext ctx;
  MyliteLexer lexer;
  MyliteToken token;
  void *parser;
  int token_id;
  int last_token_id = 0;

  if (result == NULL) {
    result = &local_result;
  }
  result_init(result);

  ctx.sql = sql;
  ctx.length = length;
  ctx.permissive = permissive;
  ctx.permissive_fallbacks = 0;
  ctx.accepted = 0;
  ctx.failed = 0;
  ctx.result = result;

  parser = MyLiteLemonAlloc(malloc);
  if (parser == NULL) {
    snprintf(result->error_message, sizeof(result->error_message),
             "failed to allocate parser");
    return MYLITE_PARSE_ERROR;
  }

  mylite_lexer_init(&lexer, sql, length, result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    result->token_count++;
    MyLiteLemon(parser, token_id, token, &ctx);
    last_token_id = token_id;
    if (ctx.failed) {
      MyLiteLemonFree(parser, free);
      return MYLITE_PARSE_ERROR;
    }
  }

  if (token_id < 0) {
    MyLiteLemonFree(parser, free);
    return MYLITE_PARSE_ERROR;
  }

  token.start = sql + length;
  token.length = 0;
  token.offset = length;
  token.line = lexer.line;
  token.column = lexer.column;
  if (result->token_count > 0 && last_token_id != ML_SEMI) {
    MyLiteLemon(parser, ML_SEMI, token, &ctx);
  }
  MyLiteLemon(parser, 0, token, &ctx);
  MyLiteLemonFree(parser, free);

  if (ctx.failed || !ctx.accepted) {
    if (result->error_message[0] == '\0') {
      set_parser_error(&ctx, &token, "unexpected end of input");
    }
    return MYLITE_PARSE_ERROR;
  }

  result->permissive_fallbacks = ctx.permissive_fallbacks;

  return MYLITE_PARSE_OK;
}

void mylite_parser_accept(MyliteParseContext *ctx) {
  ctx->accepted = 1;
}

void mylite_parser_failure(MyliteParseContext *ctx) {
  ctx->failed = 1;
}

void mylite_parser_syntax_error(MyliteParseContext *ctx, int token_id,
                                MyliteToken token) {
  if (ctx->failed) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, token_id, &token);
}

void mylite_parser_record_statement(MyliteParseContext *ctx,
                                    MyliteStatementKind kind) {
  if (kind >= MYLITE_STATEMENT_KIND_COUNT) {
    kind = MYLITE_STATEMENT_UTILITY;
  }

  ctx->result->statement_count++;
  ctx->result->statement_kind_counts[kind]++;
}

void mylite_parser_record_empty_statement(MyliteParseContext *ctx) {
  ctx->result->empty_statement_count++;
  ctx->result->statement_kind_counts[MYLITE_STATEMENT_EMPTY]++;
}

void mylite_parser_require_permissive(MyliteParseContext *ctx,
                                      MyliteToken token) {
  if (ctx->permissive) {
    ctx->permissive_fallbacks++;
    return;
  }

  ctx->failed = 1;
  set_parser_error(ctx, &token, "unsupported statement start");
}

static void result_init(MyliteParseResult *result) {
  memset(result, 0, sizeof(*result));
  result->error_line = 1;
  result->error_column = 1;
}

static void set_parser_error(MyliteParseContext *ctx, const MyliteToken *token,
                             const char *message) {
  MyliteParseResult *result = ctx->result;
  result->error_offset = token->offset;
  result->error_line = token->line;
  result->error_column = token->column;
  snprintf(result->error_message, sizeof(result->error_message), "%s",
           message);
}

static void format_near_token(MyliteParseContext *ctx, int token_id,
                              const MyliteToken *token) {
  MyliteParseResult *result = ctx->result;
  size_t copy_length = token->length;
  char snippet[64];

  (void) token_id;

  if (copy_length >= sizeof(snippet)) {
    copy_length = sizeof(snippet) - 1;
  }
  memcpy(snippet, token->start, copy_length);
  snippet[copy_length] = '\0';

  result->error_offset = token->offset;
  result->error_line = token->line;
  result->error_column = token->column;
  snprintf(result->error_message, sizeof(result->error_message),
           "syntax error near '%s'", snippet);
}
