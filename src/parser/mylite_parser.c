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
static int select_modifier_flag(int token_id);
static int select_order_direction_boundary(int token_id);
static int select_rollup_boundary(int token_id, MyliteToken token);
static int select_set_operator(int token_id);
static int select_set_option(int token_id);
static int select_set_operand_start(int token_id);
static int select_window_name_token(int token_id, MyliteToken token);
static int select_lock_table_ref_start(int token_id, MyliteToken token);
static int select_lock_table_ref_part(int token_id);
static int select_into_output_option_start(int token_id);
static int select_outfile_field_option_start(int token_id);
static int select_outfile_line_option_start(int token_id);
static int select_index_hint_name_token(int token_id);
static int select_index_hint_type(int token_id);
static int select_partition_name_token(int token_id);
static int select_tablesample_boundary(int token_id, MyliteToken token);
static int select_tablesample_percentage_token(int token_id);
static int select_charset_name_token(int token_id, MyliteToken token);
static int select_limit_option_token(int token_id);
static int select_string_literal_token(int token_id);
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
  enum {
    SELECT_MODIFIER_ALL = 1 << 0,
    SELECT_MODIFIER_DISTINCT = 1 << 1
  };
  enum {
    SELECT_LOCK_NONE,
    SELECT_LOCK_AFTER_LOCK,
    SELECT_LOCK_AFTER_LOCK_IN,
    SELECT_LOCK_AFTER_LOCK_IN_SHARE,
    SELECT_LOCK_AFTER_FOR,
    SELECT_LOCK_AFTER_STRENGTH,
    SELECT_LOCK_AFTER_OF,
    SELECT_LOCK_AFTER_TABLE,
    SELECT_LOCK_AFTER_DOT,
    SELECT_LOCK_AFTER_COMMA,
    SELECT_LOCK_AFTER_SKIP,
    SELECT_LOCK_COMPLETE
  };
  enum {
    SELECT_INTO_NONE,
    SELECT_INTO_AFTER_INTO,
    SELECT_INTO_AFTER_OUTFILE,
    SELECT_INTO_AFTER_DUMPFILE,
    SELECT_INTO_OUTFILE_READY,
    SELECT_INTO_DUMPFILE_READY,
    SELECT_INTO_AFTER_CHARACTER,
    SELECT_INTO_AFTER_CHARSET,
    SELECT_INTO_AFTER_FIELDS,
    SELECT_INTO_AFTER_FIELD_OPTION,
    SELECT_INTO_AFTER_FIELD_BY,
    SELECT_INTO_AFTER_OPTIONALLY,
    SELECT_INTO_AFTER_OPTIONALLY_ENCLOSED,
    SELECT_INTO_AFTER_LINES,
    SELECT_INTO_AFTER_LINE_OPTION,
    SELECT_INTO_AFTER_LINE_BY
  };
  enum {
    SELECT_LIMIT_NONE,
    SELECT_LIMIT_AFTER_LIMIT,
    SELECT_LIMIT_AFTER_FIRST_VALUE,
    SELECT_LIMIT_AFTER_COMMA,
    SELECT_LIMIT_AFTER_OFFSET,
    SELECT_LIMIT_AFTER_FINAL_VALUE
  };
  enum {
    SELECT_WINDOW_NONE,
    SELECT_WINDOW_AFTER_WINDOW,
    SELECT_WINDOW_AFTER_NAME,
    SELECT_WINDOW_AFTER_AS,
    SELECT_WINDOW_AFTER_SPEC
  };
  enum {
    SELECT_ROLLUP_NONE,
    SELECT_ROLLUP_AFTER_WITH,
    SELECT_ROLLUP_COMPLETE
  };
  enum {
    SELECT_ORDER_DIRECTION_NONE,
    SELECT_ORDER_DIRECTION_COMPLETE
  };
  enum {
    SELECT_INDEX_HINT_NONE,
    SELECT_INDEX_HINT_AFTER_TYPE,
    SELECT_INDEX_HINT_AFTER_KEY,
    SELECT_INDEX_HINT_AFTER_FOR,
    SELECT_INDEX_HINT_AFTER_FOR_ORDER_GROUP,
    SELECT_INDEX_HINT_AFTER_SCOPE,
    SELECT_INDEX_HINT_AFTER_LP,
    SELECT_INDEX_HINT_AFTER_NAME,
    SELECT_INDEX_HINT_AFTER_COMMA
  };
  enum {
    SELECT_PARTITION_NONE,
    SELECT_PARTITION_AFTER_PARTITION,
    SELECT_PARTITION_AFTER_LP,
    SELECT_PARTITION_AFTER_NAME,
    SELECT_PARTITION_AFTER_COMMA
  };
  enum {
    SELECT_TABLESAMPLE_NONE,
    SELECT_TABLESAMPLE_AFTER_TABLESAMPLE,
    SELECT_TABLESAMPLE_AFTER_METHOD,
    SELECT_TABLESAMPLE_AFTER_LP,
    SELECT_TABLESAMPLE_AFTER_PERCENTAGE,
    SELECT_TABLESAMPLE_COMPLETE
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token;
  int token_id;
  int depth = 0;
  int saw_select = 0;
  int select_prefix = 1;
  int select_modifiers = 0;
  int need_by = 0;
  int need_operand = 0;
  int need_set_operand = 0;
  int lock_state = SELECT_LOCK_NONE;
  int saw_lock_tail = 0;
  int into_state = SELECT_INTO_NONE;
  int outfile_fields = 0;
  int outfile_lines = 0;
  int limit_state = SELECT_LIMIT_NONE;
  int window_state = SELECT_WINDOW_NONE;
  int group_clause = 0;
  int rollup_state = SELECT_ROLLUP_NONE;
  int order_clause = 0;
  int order_direction_state = SELECT_ORDER_DIRECTION_NONE;
  int index_hint_state = SELECT_INDEX_HINT_NONE;
  int index_hint_allow_empty = 0;
  int from_clause = 0;
  int partition_state = SELECT_PARTITION_NONE;
  int tablesample_state = SELECT_TABLESAMPLE_NONE;

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

    if (select_prefix) {
      int modifier_flag = select_modifier_flag(token_id);
      if (modifier_flag) {
        if ((select_modifiers & SELECT_MODIFIER_ALL) &&
            (modifier_flag & SELECT_MODIFIER_DISTINCT)) {
          mylite_parser_reject(ctx, token, "invalid SELECT modifiers");
          return;
        }
        if ((select_modifiers & SELECT_MODIFIER_DISTINCT) &&
            (modifier_flag & SELECT_MODIFIER_ALL)) {
          mylite_parser_reject(ctx, token, "invalid SELECT modifiers");
          return;
        }
        select_modifiers |= modifier_flag;
        continue;
      }
      select_prefix = 0;
    }

    if (rollup_state == SELECT_ROLLUP_AFTER_WITH) {
      if (!token_ascii_equal(token, "rollup")) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT GROUP BY rollup clause");
        return;
      }
      rollup_state = SELECT_ROLLUP_COMPLETE;
      continue;
    }
    if (rollup_state == SELECT_ROLLUP_COMPLETE) {
      if (!select_rollup_boundary(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "malformed SELECT GROUP BY rollup clause");
        return;
      }
      group_clause = 0;
      rollup_state = SELECT_ROLLUP_NONE;
    }

    if (order_direction_state == SELECT_ORDER_DIRECTION_COMPLETE) {
      if (!select_order_direction_boundary(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "malformed SELECT ORDER BY direction");
        return;
      }
      order_direction_state = SELECT_ORDER_DIRECTION_NONE;
      order_clause = 0;
    }

    if (index_hint_state == SELECT_INDEX_HINT_AFTER_TYPE) {
      if (token_id != ML_INDEX && token_id != ML_KEY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT index hint");
        return;
      }
      index_hint_state = SELECT_INDEX_HINT_AFTER_KEY;
      continue;
    }
    if (index_hint_state == SELECT_INDEX_HINT_AFTER_KEY) {
      if (token_id == ML_FOR) {
        index_hint_state = SELECT_INDEX_HINT_AFTER_FOR;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LP) {
        index_hint_state = SELECT_INDEX_HINT_AFTER_LP;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "incomplete SELECT index hint");
      return;
    }
    if (index_hint_state == SELECT_INDEX_HINT_AFTER_FOR) {
      if (token_id == ML_JOIN) {
        index_hint_state = SELECT_INDEX_HINT_AFTER_SCOPE;
        continue;
      }
      if (token_id == ML_GROUP || token_id == ML_ORDER) {
        index_hint_state = SELECT_INDEX_HINT_AFTER_FOR_ORDER_GROUP;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "incomplete SELECT index hint");
      return;
    }
    if (index_hint_state == SELECT_INDEX_HINT_AFTER_FOR_ORDER_GROUP) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT index hint");
        return;
      }
      index_hint_state = SELECT_INDEX_HINT_AFTER_SCOPE;
      continue;
    }
    if (index_hint_state == SELECT_INDEX_HINT_AFTER_SCOPE) {
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT index hint");
        return;
      }
      index_hint_state = SELECT_INDEX_HINT_AFTER_LP;
      pending_token = token;
      continue;
    }
    if (index_hint_state == SELECT_INDEX_HINT_AFTER_LP) {
      if (token_id == ML_RP && index_hint_allow_empty) {
        index_hint_state = SELECT_INDEX_HINT_NONE;
        continue;
      }
      if (!select_index_hint_name_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT index hint");
        return;
      }
      index_hint_state = SELECT_INDEX_HINT_AFTER_NAME;
      continue;
    }
    if (index_hint_state == SELECT_INDEX_HINT_AFTER_NAME) {
      if (token_id == ML_COMMA) {
        index_hint_state = SELECT_INDEX_HINT_AFTER_COMMA;
        pending_token = token;
        continue;
      }
      if (token_id == ML_RP) {
        index_hint_state = SELECT_INDEX_HINT_NONE;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "malformed SELECT index hint");
      return;
    }
    if (index_hint_state == SELECT_INDEX_HINT_AFTER_COMMA) {
      if (!select_index_hint_name_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT index hint");
        return;
      }
      index_hint_state = SELECT_INDEX_HINT_AFTER_NAME;
      continue;
    }

    if (partition_state == SELECT_PARTITION_AFTER_PARTITION) {
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT partition clause");
        return;
      }
      partition_state = SELECT_PARTITION_AFTER_LP;
      pending_token = token;
      continue;
    }
    if (partition_state == SELECT_PARTITION_AFTER_LP) {
      if (!select_partition_name_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT partition clause");
        return;
      }
      partition_state = SELECT_PARTITION_AFTER_NAME;
      continue;
    }
    if (partition_state == SELECT_PARTITION_AFTER_NAME) {
      if (token_id == ML_COMMA) {
        partition_state = SELECT_PARTITION_AFTER_COMMA;
        pending_token = token;
        continue;
      }
      if (token_id == ML_RP) {
        partition_state = SELECT_PARTITION_NONE;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "malformed SELECT partition clause");
      return;
    }
    if (partition_state == SELECT_PARTITION_AFTER_COMMA) {
      if (!select_partition_name_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT partition clause");
        return;
      }
      partition_state = SELECT_PARTITION_AFTER_NAME;
      continue;
    }

    if (tablesample_state == SELECT_TABLESAMPLE_AFTER_TABLESAMPLE) {
      if (!token_ascii_equal(token, "bernoulli") &&
          token_id != ML_SYSTEM) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT TABLESAMPLE clause");
        return;
      }
      tablesample_state = SELECT_TABLESAMPLE_AFTER_METHOD;
      continue;
    }
    if (tablesample_state == SELECT_TABLESAMPLE_AFTER_METHOD) {
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT TABLESAMPLE clause");
        return;
      }
      tablesample_state = SELECT_TABLESAMPLE_AFTER_LP;
      pending_token = token;
      continue;
    }
    if (tablesample_state == SELECT_TABLESAMPLE_AFTER_LP) {
      if (!select_tablesample_percentage_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT TABLESAMPLE clause");
        return;
      }
      tablesample_state = SELECT_TABLESAMPLE_AFTER_PERCENTAGE;
      continue;
    }
    if (tablesample_state == SELECT_TABLESAMPLE_AFTER_PERCENTAGE) {
      if (token_id != ML_RP) {
        mylite_parser_reject(ctx, pending_token,
                             "malformed SELECT TABLESAMPLE clause");
        return;
      }
      tablesample_state = SELECT_TABLESAMPLE_COMPLETE;
      continue;
    }
    if (tablesample_state == SELECT_TABLESAMPLE_COMPLETE) {
      if (!select_tablesample_boundary(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "malformed SELECT TABLESAMPLE clause");
        return;
      }
      tablesample_state = SELECT_TABLESAMPLE_NONE;
    }

    if (lock_state == SELECT_LOCK_AFTER_LOCK) {
      if (token_id != ML_IN) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT lock clause");
        return;
      }
      lock_state = SELECT_LOCK_AFTER_LOCK_IN;
      continue;
    }
    if (lock_state == SELECT_LOCK_AFTER_LOCK_IN) {
      if (!token_ascii_equal(token, "share")) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT lock clause");
        return;
      }
      lock_state = SELECT_LOCK_AFTER_LOCK_IN_SHARE;
      continue;
    }
    if (lock_state == SELECT_LOCK_AFTER_LOCK_IN_SHARE) {
      if (!token_ascii_equal(token, "mode")) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT lock clause");
        return;
      }
      lock_state = SELECT_LOCK_COMPLETE;
      saw_lock_tail = 1;
      continue;
    }
    if (lock_state == SELECT_LOCK_AFTER_FOR) {
      if (!saw_lock_tail &&
          (token_id == ML_GROUP || token_id == ML_JOIN ||
           token_id == ML_ORDER)) {
        lock_state = SELECT_LOCK_NONE;
        continue;
      }
      if (token_id == ML_UPDATE || token_ascii_equal(token, "share")) {
        lock_state = SELECT_LOCK_AFTER_STRENGTH;
        saw_lock_tail = 1;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "incomplete SELECT lock clause");
      return;
    }
    if (lock_state == SELECT_LOCK_AFTER_STRENGTH) {
      if (token_ascii_equal(token, "of")) {
        lock_state = SELECT_LOCK_AFTER_OF;
        pending_token = token;
        continue;
      }
      if (token_ascii_equal(token, "skip")) {
        lock_state = SELECT_LOCK_AFTER_SKIP;
        pending_token = token;
        continue;
      }
      if (token_ascii_equal(token, "nowait")) {
        lock_state = SELECT_LOCK_COMPLETE;
        continue;
      }
      if (token_id == ML_FOR) {
        lock_state = SELECT_LOCK_AFTER_FOR;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LOCK) {
        lock_state = SELECT_LOCK_AFTER_LOCK;
        pending_token = token;
        continue;
      }
      if (token_id == ML_INTO) {
        lock_state = SELECT_LOCK_NONE;
        into_state = SELECT_INTO_AFTER_INTO;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "malformed SELECT lock clause");
      return;
    }
    if (lock_state == SELECT_LOCK_AFTER_OF ||
        lock_state == SELECT_LOCK_AFTER_COMMA) {
      if (!select_lock_table_ref_start(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT lock clause");
        return;
      }
      lock_state = SELECT_LOCK_AFTER_TABLE;
      continue;
    }
    if (lock_state == SELECT_LOCK_AFTER_TABLE) {
      if (token_id == ML_DOT) {
        lock_state = SELECT_LOCK_AFTER_DOT;
        pending_token = token;
        continue;
      }
      if (token_id == ML_COMMA) {
        lock_state = SELECT_LOCK_AFTER_COMMA;
        pending_token = token;
        continue;
      }
      if (token_ascii_equal(token, "skip")) {
        lock_state = SELECT_LOCK_AFTER_SKIP;
        pending_token = token;
        continue;
      }
      if (token_ascii_equal(token, "nowait")) {
        lock_state = SELECT_LOCK_COMPLETE;
        continue;
      }
      if (token_id == ML_FOR) {
        lock_state = SELECT_LOCK_AFTER_FOR;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LOCK) {
        lock_state = SELECT_LOCK_AFTER_LOCK;
        pending_token = token;
        continue;
      }
      if (token_id == ML_INTO) {
        lock_state = SELECT_LOCK_NONE;
        into_state = SELECT_INTO_AFTER_INTO;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "malformed SELECT lock clause");
      return;
    }
    if (lock_state == SELECT_LOCK_AFTER_DOT) {
      if (!select_lock_table_ref_part(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT lock clause");
        return;
      }
      lock_state = SELECT_LOCK_AFTER_TABLE;
      continue;
    }
    if (lock_state == SELECT_LOCK_AFTER_SKIP) {
      if (!token_ascii_equal(token, "locked")) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT lock clause");
        return;
      }
      lock_state = SELECT_LOCK_COMPLETE;
      continue;
    }
    if (lock_state == SELECT_LOCK_COMPLETE) {
      if (token_id == ML_FOR) {
        lock_state = SELECT_LOCK_AFTER_FOR;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LOCK) {
        lock_state = SELECT_LOCK_AFTER_LOCK;
        pending_token = token;
        continue;
      }
      if (token_id == ML_INTO) {
        lock_state = SELECT_LOCK_NONE;
        into_state = SELECT_INTO_AFTER_INTO;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "malformed SELECT lock clause");
      return;
    }

    if (into_state == SELECT_INTO_AFTER_INTO) {
      if (token_id == ML_OUTFILE) {
        into_state = SELECT_INTO_AFTER_OUTFILE;
        outfile_fields = 0;
        outfile_lines = 0;
        pending_token = token;
        continue;
      }
      if (token_id == ML_DUMPFILE) {
        into_state = SELECT_INTO_AFTER_DUMPFILE;
        pending_token = token;
        continue;
      }
      if (select_operand_boundary(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO clause");
        return;
      }
      into_state = SELECT_INTO_NONE;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_OUTFILE) {
      if (!select_string_literal_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO file target");
        return;
      }
      into_state = SELECT_INTO_OUTFILE_READY;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_DUMPFILE) {
      if (!select_string_literal_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO file target");
        return;
      }
      into_state = SELECT_INTO_DUMPFILE_READY;
      continue;
    }
    if (into_state == SELECT_INTO_OUTFILE_READY) {
      if (token_id == ML_CHARACTER) {
        if (outfile_fields || outfile_lines) {
          mylite_parser_reject(ctx, token,
                               "malformed SELECT INTO OUTFILE option");
          return;
        }
        into_state = SELECT_INTO_AFTER_CHARACTER;
        pending_token = token;
        continue;
      }
      if (token_id == ML_CHARSET) {
        if (outfile_fields || outfile_lines) {
          mylite_parser_reject(ctx, token,
                               "malformed SELECT INTO OUTFILE option");
          return;
        }
        into_state = SELECT_INTO_AFTER_CHARSET;
        pending_token = token;
        continue;
      }
      if (token_id == ML_FIELDS || token_id == ML_COLUMNS) {
        if (outfile_fields || outfile_lines) {
          mylite_parser_reject(ctx, token,
                               "malformed SELECT INTO OUTFILE option");
          return;
        }
        outfile_fields = 1;
        into_state = SELECT_INTO_AFTER_FIELDS;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LINES) {
        if (outfile_lines) {
          mylite_parser_reject(ctx, token,
                               "malformed SELECT INTO OUTFILE option");
          return;
        }
        outfile_lines = 1;
        into_state = SELECT_INTO_AFTER_LINES;
        pending_token = token;
        continue;
      }
      if (select_outfile_line_option_start(token_id) && outfile_lines) {
        into_state = SELECT_INTO_AFTER_LINE_OPTION;
        pending_token = token;
        continue;
      }
      if (select_outfile_field_option_start(token_id)) {
        if (!outfile_fields || outfile_lines) {
          mylite_parser_reject(ctx, token,
                               "malformed SELECT INTO OUTFILE option");
          return;
        }
        if (token_id == ML_OPTIONALLY) {
          into_state = SELECT_INTO_AFTER_OPTIONALLY;
        } else {
          into_state = SELECT_INTO_AFTER_FIELD_OPTION;
        }
        pending_token = token;
        continue;
      }
      if (select_outfile_line_option_start(token_id)) {
        mylite_parser_reject(ctx, token,
                             "malformed SELECT INTO OUTFILE option");
        return;
      }
      into_state = SELECT_INTO_NONE;
    }
    if (into_state == SELECT_INTO_DUMPFILE_READY) {
      if (select_into_output_option_start(token_id)) {
        mylite_parser_reject(ctx, token,
                             "malformed SELECT INTO DUMPFILE option");
        return;
      }
      into_state = SELECT_INTO_NONE;
    }
    if (into_state == SELECT_INTO_AFTER_CHARACTER) {
      if (token_id != ML_SET) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE charset");
        return;
      }
      into_state = SELECT_INTO_AFTER_CHARSET;
      pending_token = token;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_CHARSET) {
      if (!select_charset_name_token(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE charset");
        return;
      }
      into_state = SELECT_INTO_OUTFILE_READY;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_FIELDS) {
      if (!select_outfile_field_option_start(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE fields option");
        return;
      }
      if (token_id == ML_OPTIONALLY) {
        into_state = SELECT_INTO_AFTER_OPTIONALLY;
      } else {
        into_state = SELECT_INTO_AFTER_FIELD_OPTION;
      }
      pending_token = token;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_FIELD_OPTION) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE fields option");
        return;
      }
      into_state = SELECT_INTO_AFTER_FIELD_BY;
      pending_token = token;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_FIELD_BY) {
      if (!select_string_literal_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE fields option");
        return;
      }
      into_state = SELECT_INTO_OUTFILE_READY;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_OPTIONALLY) {
      if (token_id != ML_ENCLOSED) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE fields option");
        return;
      }
      into_state = SELECT_INTO_AFTER_OPTIONALLY_ENCLOSED;
      pending_token = token;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_OPTIONALLY_ENCLOSED) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE fields option");
        return;
      }
      into_state = SELECT_INTO_AFTER_FIELD_BY;
      pending_token = token;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_LINES) {
      if (!select_outfile_line_option_start(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE lines option");
        return;
      }
      into_state = SELECT_INTO_AFTER_LINE_OPTION;
      pending_token = token;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_LINE_OPTION) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE lines option");
        return;
      }
      into_state = SELECT_INTO_AFTER_LINE_BY;
      pending_token = token;
      continue;
    }
    if (into_state == SELECT_INTO_AFTER_LINE_BY) {
      if (!select_string_literal_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT INTO OUTFILE lines option");
        return;
      }
      into_state = SELECT_INTO_OUTFILE_READY;
      continue;
    }

    if (limit_state == SELECT_LIMIT_AFTER_LIMIT ||
        limit_state == SELECT_LIMIT_AFTER_COMMA ||
        limit_state == SELECT_LIMIT_AFTER_OFFSET) {
      if (!select_limit_option_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT LIMIT clause");
        return;
      }
      if (limit_state == SELECT_LIMIT_AFTER_LIMIT) {
        limit_state = SELECT_LIMIT_AFTER_FIRST_VALUE;
      } else {
        limit_state = SELECT_LIMIT_AFTER_FINAL_VALUE;
      }
      continue;
    }
    if (limit_state == SELECT_LIMIT_AFTER_FIRST_VALUE) {
      if (token_id == ML_COMMA) {
        limit_state = SELECT_LIMIT_AFTER_COMMA;
        pending_token = token;
        continue;
      }
      if (token_id == ML_OFFSET) {
        limit_state = SELECT_LIMIT_AFTER_OFFSET;
        pending_token = token;
        continue;
      }
      limit_state = SELECT_LIMIT_NONE;
    }
    if (limit_state == SELECT_LIMIT_AFTER_FINAL_VALUE) {
      if (token_id == ML_COMMA || token_id == ML_OFFSET) {
        mylite_parser_reject(ctx, token, "malformed SELECT LIMIT clause");
        return;
      }
      limit_state = SELECT_LIMIT_NONE;
    }

    if (window_state == SELECT_WINDOW_AFTER_WINDOW) {
      if (!select_window_name_token(token_id, token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT WINDOW clause");
        return;
      }
      window_state = SELECT_WINDOW_AFTER_NAME;
      continue;
    }
    if (window_state == SELECT_WINDOW_AFTER_NAME) {
      if (token_id != ML_AS) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT WINDOW clause");
        return;
      }
      window_state = SELECT_WINDOW_AFTER_AS;
      pending_token = token;
      continue;
    }
    if (window_state == SELECT_WINDOW_AFTER_AS) {
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT WINDOW clause");
        return;
      }
      depth++;
      window_state = SELECT_WINDOW_AFTER_SPEC;
      continue;
    }
    if (window_state == SELECT_WINDOW_AFTER_SPEC) {
      if (token_id == ML_COMMA) {
        window_state = SELECT_WINDOW_AFTER_WINDOW;
        pending_token = token;
        continue;
      }
      window_state = SELECT_WINDOW_NONE;
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

    if (need_set_operand) {
      if (select_set_option(token_id)) {
        continue;
      }
      if (!select_set_operand_start(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete SELECT set operation");
        return;
      }
      need_set_operand = 0;
    }

    if (need_operand) {
      if (select_operand_boundary(token_id) ||
          token_ascii_equal(token, "qualify")) {
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

    if (token_id == ML_COMMA) {
      need_operand = 1;
      pending_token = token;
      continue;
    }

    if (order_clause && (token_id == ML_ASC || token_id == ML_DESC)) {
      order_direction_state = SELECT_ORDER_DIRECTION_COMPLETE;
      pending_token = token;
      continue;
    }

    if (select_index_hint_type(token_id)) {
      index_hint_state = SELECT_INDEX_HINT_AFTER_TYPE;
      index_hint_allow_empty = token_id == ML_USE;
      pending_token = token;
      continue;
    }

    if (from_clause && token_id == ML_PARTITION) {
      partition_state = SELECT_PARTITION_AFTER_PARTITION;
      pending_token = token;
      continue;
    }

    if (from_clause && token_ascii_equal(token, "tablesample")) {
      tablesample_state = SELECT_TABLESAMPLE_AFTER_TABLESAMPLE;
      pending_token = token;
      continue;
    }

    if (group_clause && token_id == ML_WITH) {
      rollup_state = SELECT_ROLLUP_AFTER_WITH;
      pending_token = token;
      continue;
    }

    if (token_id == ML_LOCK) {
      group_clause = 0;
      order_clause = 0;
      from_clause = 0;
      lock_state = SELECT_LOCK_AFTER_LOCK;
      pending_token = token;
      continue;
    }
    if (token_id == ML_FOR) {
      group_clause = 0;
      order_clause = 0;
      from_clause = 0;
      lock_state = SELECT_LOCK_AFTER_FOR;
      pending_token = token;
      continue;
    }
    if (token_id == ML_INTO) {
      group_clause = 0;
      order_clause = 0;
      from_clause = 0;
      into_state = SELECT_INTO_AFTER_INTO;
      pending_token = token;
      continue;
    }
    if (token_id == ML_LIMIT) {
      group_clause = 0;
      order_clause = 0;
      from_clause = 0;
      limit_state = SELECT_LIMIT_AFTER_LIMIT;
      pending_token = token;
      continue;
    }
    if (token_ascii_equal(token, "window")) {
      group_clause = 0;
      from_clause = 0;
      window_state = SELECT_WINDOW_AFTER_WINDOW;
      pending_token = token;
      continue;
    }
    if (token_ascii_equal(token, "qualify")) {
      group_clause = 0;
      from_clause = 0;
      need_operand = 1;
      pending_token = token;
      continue;
    }

    if (select_clause_requires_by(token_id)) {
      from_clause = 0;
      group_clause = token_id == ML_GROUP;
      order_clause = token_id == ML_ORDER;
      need_by = 1;
      pending_token = token;
      continue;
    }
    if (select_clause_requires_operand(token_id)) {
      if (token_id == ML_HAVING || token_id == ML_JOIN || token_id == ML_ON ||
          token_id == ML_PROCEDURE || token_id == ML_USING ||
          token_id == ML_WHERE) {
        group_clause = 0;
        order_clause = 0;
      }
      from_clause = token_id == ML_FROM;
      need_operand = 1;
      pending_token = token;
      continue;
    }
    if (select_set_operator(token_id)) {
      group_clause = 0;
      order_clause = 0;
      from_clause = 0;
      need_set_operand = 1;
      pending_token = token;
      continue;
    }
  }

  if (lock_state == SELECT_LOCK_AFTER_LOCK ||
      lock_state == SELECT_LOCK_AFTER_LOCK_IN ||
      lock_state == SELECT_LOCK_AFTER_LOCK_IN_SHARE ||
      lock_state == SELECT_LOCK_AFTER_FOR ||
      lock_state == SELECT_LOCK_AFTER_OF ||
      lock_state == SELECT_LOCK_AFTER_DOT ||
      lock_state == SELECT_LOCK_AFTER_COMMA ||
      lock_state == SELECT_LOCK_AFTER_SKIP) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT lock clause");
  } else if (into_state == SELECT_INTO_AFTER_INTO) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT INTO clause");
  } else if (into_state == SELECT_INTO_AFTER_OUTFILE ||
             into_state == SELECT_INTO_AFTER_DUMPFILE) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT INTO file target");
  } else if (into_state == SELECT_INTO_AFTER_CHARACTER ||
             into_state == SELECT_INTO_AFTER_CHARSET) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT INTO OUTFILE charset");
  } else if (into_state == SELECT_INTO_AFTER_FIELDS ||
             into_state == SELECT_INTO_AFTER_FIELD_OPTION ||
             into_state == SELECT_INTO_AFTER_FIELD_BY ||
             into_state == SELECT_INTO_AFTER_OPTIONALLY ||
             into_state == SELECT_INTO_AFTER_OPTIONALLY_ENCLOSED) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT INTO OUTFILE fields option");
  } else if (into_state == SELECT_INTO_AFTER_LINES ||
             into_state == SELECT_INTO_AFTER_LINE_OPTION ||
             into_state == SELECT_INTO_AFTER_LINE_BY) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT INTO OUTFILE lines option");
  } else if (limit_state == SELECT_LIMIT_AFTER_LIMIT ||
             limit_state == SELECT_LIMIT_AFTER_COMMA ||
             limit_state == SELECT_LIMIT_AFTER_OFFSET) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT LIMIT clause");
  } else if (window_state == SELECT_WINDOW_AFTER_WINDOW ||
             window_state == SELECT_WINDOW_AFTER_NAME ||
             window_state == SELECT_WINDOW_AFTER_AS) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT WINDOW clause");
  } else if (rollup_state == SELECT_ROLLUP_AFTER_WITH) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT GROUP BY rollup clause");
  } else if (index_hint_state == SELECT_INDEX_HINT_AFTER_TYPE ||
             index_hint_state == SELECT_INDEX_HINT_AFTER_KEY ||
             index_hint_state == SELECT_INDEX_HINT_AFTER_FOR ||
             index_hint_state == SELECT_INDEX_HINT_AFTER_FOR_ORDER_GROUP ||
             index_hint_state == SELECT_INDEX_HINT_AFTER_SCOPE ||
             index_hint_state == SELECT_INDEX_HINT_AFTER_LP ||
             index_hint_state == SELECT_INDEX_HINT_AFTER_COMMA) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT index hint");
  } else if (partition_state == SELECT_PARTITION_AFTER_PARTITION ||
             partition_state == SELECT_PARTITION_AFTER_LP ||
             partition_state == SELECT_PARTITION_AFTER_COMMA) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT partition clause");
  } else if (tablesample_state == SELECT_TABLESAMPLE_AFTER_TABLESAMPLE ||
             tablesample_state == SELECT_TABLESAMPLE_AFTER_METHOD ||
             tablesample_state == SELECT_TABLESAMPLE_AFTER_LP) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT TABLESAMPLE clause");
  } else if (need_set_operand) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete SELECT set operation");
  } else if (need_by || need_operand) {
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
  return token_id == ML_FROM || token_id == ML_HAVING ||
         token_id == ML_JOIN || token_id == ML_LIMIT || token_id == ML_ON ||
         token_id == ML_PROCEDURE || token_id == ML_USING ||
         token_id == ML_WHERE;
}

static int select_operand_boundary(int token_id) {
  return token_id == ML_SEMI || token_id == ML_COMMA || token_id == ML_RP ||
         select_set_operator(token_id) ||
         select_clause_requires_by(token_id) ||
         select_clause_requires_operand(token_id);
}

static int select_modifier_flag(int token_id) {
  if (token_id == ML_ALL) {
    return 1 << 0;
  }
  if (token_id == ML_DISTINCT || token_id == ML_DISTINCTROW) {
    return 1 << 1;
  }
  if (token_id == ML_HIGH_PRIORITY || token_id == ML_SQL_BIG_RESULT ||
      token_id == ML_SQL_BUFFER_RESULT ||
      token_id == ML_SQL_CALC_FOUND_ROWS || token_id == ML_SQL_NO_CACHE ||
      token_id == ML_SQL_SMALL_RESULT || token_id == ML_STRAIGHT_JOIN) {
    return 1 << 2;
  }

  return 0;
}

static int select_order_direction_boundary(int token_id) {
  return token_id == ML_COMMA || token_id == ML_FOR || token_id == ML_INTO ||
         token_id == ML_LIMIT || token_id == ML_LOCK || token_id == ML_RP ||
         select_set_operator(token_id);
}

static int select_rollup_boundary(int token_id, MyliteToken token) {
  return token_id == ML_FOR || token_id == ML_HAVING || token_id == ML_INTO ||
         token_id == ML_LIMIT || token_id == ML_LOCK || token_id == ML_ORDER ||
         select_set_operator(token_id) || token_ascii_equal(token, "qualify") ||
         token_ascii_equal(token, "window");
}

static int select_set_operator(int token_id) {
  return token_id == ML_EXCEPT || token_id == ML_INTERSECT ||
         token_id == ML_UNION;
}

static int select_set_option(int token_id) {
  return token_id == ML_ALL || token_id == ML_DISTINCT ||
         token_id == ML_DISTINCTROW;
}

static int select_set_operand_start(int token_id) {
  return token_id == ML_LP || token_id == ML_SELECT || token_id == ML_TABLE ||
         token_id == ML_VALUES || token_id == ML_WITH;
}

static int select_window_name_token(int token_id, MyliteToken token) {
  if (token_id == ML_AS || token_ascii_equal(token, "window")) {
    return 0;
  }

  return token_id == ML_ATOM || token_id == ML_QUOTED_ID;
}

static int select_lock_table_ref_start(int token_id, MyliteToken token) {
  if (token_ascii_equal(token, "locked") || token_ascii_equal(token, "nowait") ||
      token_ascii_equal(token, "of") || token_ascii_equal(token, "skip")) {
    return 0;
  }

  return token_id == ML_ATOM || token_id == ML_QUOTED_ID;
}

static int select_lock_table_ref_part(int token_id) {
  return token_id == ML_ATOM || token_id == ML_QUOTED_ID ||
         token_id == ML_STAR;
}

static int select_into_output_option_start(int token_id) {
  return token_id == ML_CHARACTER || token_id == ML_CHARSET ||
         token_id == ML_COLUMNS || token_id == ML_ENCLOSED ||
         token_id == ML_ESCAPED || token_id == ML_FIELDS ||
         token_id == ML_LINES || token_id == ML_OPTIONALLY ||
         token_id == ML_STARTING || token_id == ML_TERMINATED;
}

static int select_outfile_field_option_start(int token_id) {
  return token_id == ML_ENCLOSED || token_id == ML_ESCAPED ||
         token_id == ML_OPTIONALLY || token_id == ML_TERMINATED;
}

static int select_outfile_line_option_start(int token_id) {
  return token_id == ML_STARTING || token_id == ML_TERMINATED;
}

static int select_index_hint_name_token(int token_id) {
  return token_id != ML_COMMA && token_id != ML_LP && token_id != ML_RP &&
         token_id != ML_SEMI;
}

static int select_index_hint_type(int token_id) {
  return token_id == ML_FORCE || token_id == ML_IGNORE || token_id == ML_USE;
}

static int select_partition_name_token(int token_id) {
  return token_id != ML_COMMA && token_id != ML_LP && token_id != ML_RP &&
         token_id != ML_SEMI;
}

static int select_tablesample_boundary(int token_id, MyliteToken token) {
  return token_id == ML_COMMA || token_id == ML_FOR || token_id == ML_HAVING ||
         token_id == ML_INTO || token_id == ML_JOIN || token_id == ML_LIMIT ||
         token_id == ML_LOCK || token_id == ML_ORDER || token_id == ML_WHERE ||
         token_id == ML_STRAIGHT_JOIN || select_set_operator(token_id) ||
         token_ascii_equal(token, "cross") || token_ascii_equal(token, "inner") ||
         token_ascii_equal(token, "left") || token_ascii_equal(token, "natural") ||
         token_ascii_equal(token, "qualify") || token_ascii_equal(token, "right") ||
         token_ascii_equal(token, "window");
}

static int select_tablesample_percentage_token(int token_id) {
  return token_id == ML_AT_HOST || token_id == ML_ATOM ||
         token_id == ML_BOOLEAN_NUMBER || token_id == ML_FACTOR_NUMBER ||
         token_id == ML_NUMBER_LITERAL || token_id == ML_QUOTED_ID;
}

static int select_charset_name_token(int token_id, MyliteToken token) {
  return token_id == ML_ATOM || token_id == ML_BINARY ||
         token_id == ML_QUOTED_ID || token_ascii_equal(token, "binary");
}

static int select_limit_option_token(int token_id) {
  return token_id == ML_ATOM || token_id == ML_BOOLEAN_NUMBER ||
         token_id == ML_FACTOR_NUMBER || token_id == ML_NUMBER_LITERAL ||
         token_id == ML_QUOTED_ID;
}

static int select_string_literal_token(int token_id) {
  return token_id == ML_DOUBLE_QUOTED_STRING || token_id == ML_STRING_LITERAL;
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
