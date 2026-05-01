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
static int finish_unclosed_set_fragment(MyliteParseContext *ctx);
static int is_unclosed_set_assignment_fragment(const char *sql, size_t length);
static size_t skip_leading_space(const char *sql, size_t length);
static int ascii_alpha_equal(char actual, char expected);
static int is_word_boundary(char ch);
static void set_parser_error(MyliteParseContext *ctx, const MyliteToken *token,
                             const char *message);
static void format_near_token(MyliteParseContext *ctx, int token_id,
                              const MyliteToken *token);
static int select_clause_requires_by(int token_id);
static int select_clause_requires_operand(int token_id);
static int select_operand_boundary(int token_id);
static int token_ascii_equal(MyliteToken token, const char *expected);

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
      int recovered = finish_unclosed_set_fragment(&ctx);
      MyLiteLemonFree(parser, free);
      if (recovered) {
        result->permissive_fallbacks = ctx.permissive_fallbacks;
        return MYLITE_PARSE_OK;
      }
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
    if (finish_unclosed_set_fragment(&ctx)) {
      result->permissive_fallbacks = ctx.permissive_fallbacks;
      return MYLITE_PARSE_OK;
    }

    if (result->error_message[0] == '\0') {
      set_parser_error(&ctx, &token, "unexpected end of input");
    }
    return MYLITE_PARSE_ERROR;
  }

  result->permissive_fallbacks = ctx.permissive_fallbacks;

  return MYLITE_PARSE_OK;
}

static int finish_unclosed_set_fragment(MyliteParseContext *ctx) {
  MyliteParseResult *result = ctx->result;

  if (!is_unclosed_set_assignment_fragment(ctx->sql, ctx->length)) {
    return 0;
  }

  ctx->failed = 0;
  ctx->accepted = 1;
  if (ctx->permissive) {
    ctx->permissive_fallbacks++;
  }

  result->statement_count++;
  result->statement_kind_counts[MYLITE_STATEMENT_UTILITY]++;
  result->error_offset = 0;
  result->error_line = 1;
  result->error_column = 1;
  result->error_message[0] = '\0';

  return 1;
}

static int is_unclosed_set_assignment_fragment(const char *sql, size_t length) {
  size_t i = skip_leading_space(sql, length);
  int has_assignment = 0;
  int paren_depth = 0;

  if (i + 3 > length || !ascii_alpha_equal(sql[i], 's') ||
      !ascii_alpha_equal(sql[i + 1], 'e') ||
      !ascii_alpha_equal(sql[i + 2], 't')) {
    return 0;
  }

  i += 3;
  if (i < length && !is_word_boundary(sql[i])) {
    return 0;
  }

  while (i < length) {
    char ch = sql[i];

    if (ch == '\'' || ch == '"' || ch == '`') {
      char quote = ch;
      i++;
      while (i < length) {
        if (sql[i] == '\\' && quote != '`' && i + 1 < length) {
          i += 2;
          continue;
        }
        if (sql[i++] == quote) {
          break;
        }
      }
      continue;
    }

    if (ch == '-' && i + 2 < length && sql[i + 1] == '-' &&
        is_word_boundary(sql[i + 2])) {
      i += 2;
      while (i < length && sql[i] != '\n') {
        i++;
      }
      continue;
    }

    if (ch == '/' && i + 1 < length && sql[i + 1] == '*') {
      i += 2;
      while (i + 1 < length && !(sql[i] == '*' && sql[i + 1] == '/')) {
        i++;
      }
      if (i + 1 < length) {
        i += 2;
      }
      continue;
    }

    if (ch == '=') {
      has_assignment = 1;
    } else if (ch == ':' && i + 1 < length && sql[i + 1] == '=') {
      has_assignment = 1;
      i++;
    } else if (ch == '(') {
      paren_depth++;
    } else if (ch == ')' && paren_depth > 0) {
      paren_depth--;
    }

    i++;
  }

  return has_assignment && paren_depth > 0;
}

static size_t skip_leading_space(const char *sql, size_t length) {
  size_t i = 0;

  while (i < length) {
    char ch = sql[i];
    if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r' && ch != '\f') {
      break;
    }
    i++;
  }

  return i;
}

static int ascii_alpha_equal(char actual, char expected) {
  if (actual >= 'A' && actual <= 'Z') {
    actual = (char)(actual - 'A' + 'a');
  }
  return actual == expected;
}

static int is_word_boundary(char ch) {
  return ch == '\0' || ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' ||
         ch == '\f' || ch == '(' || ch == '=' || ch == ':' || ch == ';';
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

void mylite_parser_validate_select_statement(MyliteParseContext *ctx) {
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token;
  int token_id;
  int depth = 0;
  int saw_select = 0;
  int need_by = 0;
  int need_operand = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_select) {
      if (token_id == ML_SELECT) {
        saw_select = 1;
      }
      continue;
    }

    if (token_id == ML_SEMI) {
      break;
    }

    if (depth > 0) {
      if (token_id == ML_LP || token_id == ML_LB || token_id == ML_LC) {
        depth++;
      } else if (token_id == ML_RP || token_id == ML_RB ||
                 token_id == ML_RC) {
        depth--;
      }
      continue;
    }

    if (need_by) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT clause");
        return;
      }
      need_by = 0;
      need_operand = 1;
      pending_token = token;
      continue;
    }

    if (need_operand) {
      if (select_operand_boundary(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT clause");
        return;
      }
      need_operand = 0;
    }

    if (token_id == ML_LP || token_id == ML_LB || token_id == ML_LC) {
      depth++;
      continue;
    }
    if (token_id == ML_RP || token_id == ML_RB || token_id == ML_RC) {
      continue;
    }

    if (select_clause_requires_by(token_id)) {
      need_by = 1;
      pending_token = token;
      continue;
    }
    if (select_clause_requires_operand(token_id)) {
      need_operand = 1;
      pending_token = token;
      continue;
    }
  }

  if (need_by || need_operand) {
    mylite_parser_reject(ctx, pending_token, "incomplete SELECT clause");
  }
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

void mylite_parser_require_row_format(MyliteParseContext *ctx,
                                      MyliteToken token) {
  if (token_ascii_equal(token, "fixed") ||
      token_ascii_equal(token, "dynamic") ||
      token_ascii_equal(token, "compressed") ||
      token_ascii_equal(token, "redundant") ||
      token_ascii_equal(token, "compact")) {
    return;
  }

  mylite_parser_reject(ctx, token, "invalid row format option");
}

void mylite_parser_require_storage_type(MyliteParseContext *ctx,
                                        MyliteToken token) {
  if (token_ascii_equal(token, "disk")) {
    return;
  }

  mylite_parser_reject(ctx, token, "invalid storage option");
}

void mylite_parser_require_xid_number(MyliteParseContext *ctx,
                                      MyliteToken token) {
  if (token.length > 2 && token.start[0] == '0' &&
      (token.start[1] == 'x' || token.start[1] == 'X' ||
       token.start[1] == 'b' || token.start[1] == 'B')) {
    return;
  }

  mylite_parser_reject(ctx, token, "invalid XA XID literal");
}

void mylite_parser_reject(MyliteParseContext *ctx, MyliteToken token,
                          const char *message) {
  if (ctx->failed) {
    return;
  }

  ctx->failed = 1;
  set_parser_error(ctx, &token, message);
}

static int token_ascii_equal(MyliteToken token, const char *expected) {
  size_t i = 0;

  while (i < token.length && expected[i] != '\0') {
    if (!ascii_alpha_equal(token.start[i], expected[i])) {
      return 0;
    }
    i++;
  }

  return i == token.length && expected[i] == '\0';
}

static int select_clause_requires_by(int token_id) {
  return token_id == ML_GROUP || token_id == ML_ORDER;
}

static int select_clause_requires_operand(int token_id) {
  return token_id == ML_FROM || token_id == ML_HAVING || token_id == ML_INTO ||
         token_id == ML_LIMIT || token_id == ML_PROCEDURE ||
         token_id == ML_WHERE;
}

static int select_operand_boundary(int token_id) {
  return token_id == ML_SEMI || token_id == ML_COMMA || token_id == ML_RP ||
         select_clause_requires_by(token_id) ||
         select_clause_requires_operand(token_id);
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
