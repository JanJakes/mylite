#include "mylite_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mylite_lexer.h"
#include "mylite_parser_internal.h"
#include "generated/mylite_lemon.h"

typedef enum DmlAssignmentMode {
  DML_ASSIGNMENT_NONE = 0,
  DML_ASSIGNMENT_UPDATE,
  DML_ASSIGNMENT_INSERT_SET,
  DML_ASSIGNMENT_REPLACE_SET,
  DML_ASSIGNMENT_DUPLICATE
} DmlAssignmentMode;

typedef enum ColumnDefinitionTailState {
  COLUMN_DEFINITION_TAIL_READY = 0,
  COLUMN_DEFINITION_TAIL_AFTER_NOT,
  COLUMN_DEFINITION_TAIL_AFTER_PRIMARY,
  COLUMN_DEFINITION_TAIL_AFTER_DEFAULT,
  COLUMN_DEFINITION_TAIL_AFTER_DEFAULT_VALUE,
  COLUMN_DEFINITION_TAIL_AFTER_VALUE_SIGN,
  COLUMN_DEFINITION_TAIL_AFTER_COMMENT,
  COLUMN_DEFINITION_TAIL_AFTER_COLLATE,
  COLUMN_DEFINITION_TAIL_AFTER_CHARACTER,
  COLUMN_DEFINITION_TAIL_AFTER_CHARSET,
  COLUMN_DEFINITION_TAIL_AFTER_COLUMN_FORMAT,
  COLUMN_DEFINITION_TAIL_AFTER_STORAGE,
  COLUMN_DEFINITION_TAIL_AFTER_SRID,
  COLUMN_DEFINITION_TAIL_AFTER_AFTER,
  COLUMN_DEFINITION_TAIL_AFTER_ON,
  COLUMN_DEFINITION_TAIL_AFTER_ON_UPDATE,
  COLUMN_DEFINITION_TAIL_AFTER_REFERENCES,
  COLUMN_DEFINITION_TAIL_AFTER_CONSTRAINT,
  COLUMN_DEFINITION_TAIL_AFTER_CONSTRAINT_NAME,
  COLUMN_DEFINITION_TAIL_AFTER_GENERATED,
  COLUMN_DEFINITION_TAIL_AFTER_ALWAYS,
  COLUMN_DEFINITION_TAIL_AFTER_AS
} ColumnDefinitionTailState;

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
static int do_clause_boundary(int token_id);
static int kill_at_sign_target_token(int token_id);
static int kill_target_allows_call(int token_id);
static int kill_target_token(int token_id);
static int create_table_query_body_start(int token_id);
static int create_table_column_name_needs_type_check(int token_id,
                                                     MyliteToken token);
static int create_table_column_type_start(int token_id, MyliteToken token);
static int column_definition_tail_token(
    MyliteParseContext *ctx, int token_id, MyliteToken token,
    ColumnDefinitionTailState *state, int *depth, int *check_pending,
    MyliteToken *pending_token, const char *message);
static int column_definition_tail_complete(ColumnDefinitionTailState state);
static int column_definition_value_token(int token_id, MyliteToken token);
static int column_definition_charset_name_token(int token_id,
                                                MyliteToken token);
static int column_definition_attribute_start(int token_id, MyliteToken token);
static int column_definition_type_modifier(int token_id, MyliteToken token);
static int foreign_key_match_option(int token_id, MyliteToken token);
static int foreign_key_reference_action_token(int token_id);
static int validate_parenthesized_identifier_list(MyliteParseContext *ctx,
                                                  MyliteLexer *lexer,
                                                  MyliteToken start,
                                                  const char *message);
static int validate_parenthesized_nonempty_body(MyliteParseContext *ctx,
                                                MyliteLexer *lexer,
                                                MyliteToken start,
                                                const char *message);
static int validate_create_table_index_key_list(MyliteParseContext *ctx,
                                                MyliteLexer *lexer,
                                                MyliteToken start);
static int create_index_prefix_length_token(int token_id);
static int alter_table_add_index_marker(int token_id);
static int alter_table_add_non_index_marker(int token_id);
static int event_interval_unit_token(MyliteToken token);
static int event_schedule_boundary(int token_id);
static int event_schedule_option_start(MyliteToken token);
static int token_is_plus(MyliteToken token);
static int dml_assignment_boundary(int mode, int token_id);
static int dml_assignment_operator(int token_id);
static int dml_assignment_target_token(int token_id);
static int dml_clause_operand_boundary(int token_id);
static int dml_limit_option_token(int token_id);
static int dml_row_alias_token(int token_id);
static int dml_values_unclosed_string_fragment(int token_id,
                                               MyliteToken token);
static int token_opens_nested_expression(int token_id);
static int token_closes_nested_expression(int token_id);
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

void mylite_parser_validate_dml_statement(MyliteParseContext *ctx,
                                          MyliteToken start,
                                          MyliteStatementKind kind) {
  enum {
    DML_ASSIGN_NONE,
    DML_ASSIGN_TARGET,
    DML_ASSIGN_AFTER_TARGET,
    DML_ASSIGN_AFTER_DOT,
    DML_ASSIGN_VALUE
  };
  enum {
    DML_DUP_NONE,
    DML_DUP_AFTER_ON,
    DML_DUP_AFTER_DUPLICATE,
    DML_DUP_AFTER_KEY
  };
  enum {
    DML_WHERE_NONE,
    DML_WHERE_AFTER_WHERE,
    DML_WHERE_STARTED
  };
  enum {
    DML_ORDER_NONE,
    DML_ORDER_AFTER_ORDER,
    DML_ORDER_AFTER_BY,
    DML_ORDER_STARTED,
    DML_ORDER_AFTER_DIRECTION
  };
  enum {
    DML_LIMIT_NONE,
    DML_LIMIT_AFTER_LIMIT,
    DML_LIMIT_AFTER_VALUE
  };
  enum {
    DML_PAYLOAD_NONE,
    DML_PAYLOAD_SET,
    DML_PAYLOAD_VALUES,
    DML_PAYLOAD_QUERY
  };
  enum {
    DML_VALUES_NONE,
    DML_VALUES_AFTER_VALUES,
    DML_VALUES_AFTER_ROW_KEYWORD,
    DML_VALUES_IN_ROW,
    DML_VALUES_AFTER_ROW,
    DML_VALUES_AFTER_COMMA,
    DML_VALUES_AFTER_AS,
    DML_VALUES_AFTER_ALIAS,
    DML_VALUES_AFTER_ALIAS_LP,
    DML_VALUES_AFTER_ALIAS_COLUMN,
    DML_VALUES_AFTER_ALIAS_COMMA,
    DML_VALUES_AFTER_ALIAS_RP
  };
  enum {
    DML_SET_ALIAS_NONE,
    DML_SET_ALIAS_AFTER_AS,
    DML_SET_ALIAS_AFTER_ALIAS,
    DML_SET_ALIAS_AFTER_LP,
    DML_SET_ALIAS_AFTER_COLUMN,
    DML_SET_ALIAS_AFTER_COMMA,
    DML_SET_ALIAS_AFTER_RP
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  MyliteToken values_row_last_token = start;
  int token_id;
  int values_row_last_token_id = 0;
  int saw_statement = 0;
  int depth = 0;
  int assignment_state = DML_ASSIGN_NONE;
  int assignment_mode = DML_ASSIGNMENT_NONE;
  int assignment_value_started = 0;
  int duplicate_state = DML_DUP_NONE;
  int duplicate_strict = 0;
  int where_state = DML_WHERE_NONE;
  int order_state = DML_ORDER_NONE;
  int limit_state = DML_LIMIT_NONE;
  int seen_where = 0;
  int seen_order = 0;
  int seen_limit = 0;
  int payload_kind = DML_PAYLOAD_NONE;
  int values_state = DML_VALUES_NONE;
  int set_alias_state = DML_SET_ALIAS_NONE;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      } else {
        continue;
      }
    }

    if (depth > 0) {
      if (values_state == DML_VALUES_IN_ROW) {
        values_row_last_token = token;
        values_row_last_token_id = token_id;
      }
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
        if (depth == 0 && values_state == DML_VALUES_IN_ROW) {
          values_state = DML_VALUES_AFTER_ROW;
        }
      }
      continue;
    }

    if (assignment_state != DML_ASSIGN_NONE) {
      int boundary = dml_assignment_boundary(assignment_mode, token_id);
      if (token_id == ML_SEMI || boundary) {
        if (assignment_state != DML_ASSIGN_VALUE ||
            !assignment_value_started) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML assignment");
          return;
        }
        assignment_state = DML_ASSIGN_NONE;
        assignment_mode = DML_ASSIGNMENT_NONE;
        assignment_value_started = 0;
        if (token_id == ML_SEMI) {
          break;
        }
      } else if (token_id == ML_COMMA) {
        if (assignment_state != DML_ASSIGN_VALUE ||
            !assignment_value_started) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML assignment");
          return;
        }
        assignment_state = DML_ASSIGN_TARGET;
        assignment_value_started = 0;
        pending_token = token;
        continue;
      } else if (assignment_state == DML_ASSIGN_TARGET) {
        if (!dml_assignment_target_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML assignment");
          return;
        }
        assignment_state = DML_ASSIGN_AFTER_TARGET;
        continue;
      } else if (assignment_state == DML_ASSIGN_AFTER_TARGET) {
        if (token_id == ML_DOT) {
          assignment_state = DML_ASSIGN_AFTER_DOT;
          pending_token = token;
          continue;
        }
        if (!dml_assignment_operator(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML assignment");
          return;
        }
        assignment_state = DML_ASSIGN_VALUE;
        assignment_value_started = 0;
        pending_token = token;
        continue;
      } else if (assignment_state == DML_ASSIGN_AFTER_DOT) {
        if (!dml_assignment_target_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML assignment");
          return;
        }
        assignment_state = DML_ASSIGN_AFTER_TARGET;
        continue;
      } else if (assignment_state == DML_ASSIGN_VALUE) {
        if (dml_clause_operand_boundary(token_id) ||
            token_id == ML_COMMA || token_id == ML_SEMI) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML assignment");
          return;
        }
        assignment_value_started = 1;
        if (token_opens_nested_expression(token_id)) {
          depth++;
        }
        continue;
      }
    }

    if (values_state != DML_VALUES_NONE) {
      if (values_state == DML_VALUES_AFTER_VALUES ||
          values_state == DML_VALUES_AFTER_COMMA) {
        if (token_id == ML_ROW) {
          values_state = DML_VALUES_AFTER_ROW_KEYWORD;
          pending_token = token;
          continue;
        }
        if (token_id == ML_LP) {
          values_state = DML_VALUES_IN_ROW;
          values_row_last_token_id = 0;
          depth = 1;
          pending_token = token;
          continue;
        }
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DML VALUES row list");
        return;
      }
      if (values_state == DML_VALUES_AFTER_ROW_KEYWORD) {
        if (token_id != ML_LP) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML VALUES row list");
          return;
        }
        values_state = DML_VALUES_IN_ROW;
        values_row_last_token_id = 0;
        depth = 1;
        pending_token = token;
        continue;
      }
      if (values_state == DML_VALUES_AFTER_ROW) {
        if (token_id == ML_COMMA) {
          values_state = DML_VALUES_AFTER_COMMA;
          pending_token = token;
          continue;
        }
        if (kind == MYLITE_STATEMENT_INSERT && token_id == ML_AS) {
          values_state = DML_VALUES_AFTER_AS;
          pending_token = token;
          continue;
        }
        if (kind == MYLITE_STATEMENT_INSERT && token_id == ML_ON) {
          values_state = DML_VALUES_NONE;
          duplicate_state = DML_DUP_AFTER_ON;
          duplicate_strict = 1;
          pending_token = token;
          continue;
        }
        if (token_id != ML_SEMI) {
          mylite_parser_reject(ctx, token, "malformed DML VALUES row list");
          return;
        }
      } else if (values_state == DML_VALUES_AFTER_AS) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML VALUES row alias");
          return;
        }
        values_state = DML_VALUES_AFTER_ALIAS;
        continue;
      } else if (values_state == DML_VALUES_AFTER_ALIAS) {
        if (token_id == ML_LP) {
          values_state = DML_VALUES_AFTER_ALIAS_LP;
          pending_token = token;
          continue;
        }
        if (token_id == ML_ON) {
          values_state = DML_VALUES_NONE;
          duplicate_state = DML_DUP_AFTER_ON;
          duplicate_strict = 1;
          pending_token = token;
          continue;
        }
        if (token_id != ML_SEMI) {
          mylite_parser_reject(ctx, token, "malformed DML VALUES row alias");
          return;
        }
      } else if (values_state == DML_VALUES_AFTER_ALIAS_LP) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML VALUES row alias");
          return;
        }
        values_state = DML_VALUES_AFTER_ALIAS_COLUMN;
        continue;
      } else if (values_state == DML_VALUES_AFTER_ALIAS_COLUMN) {
        if (token_id == ML_COMMA) {
          values_state = DML_VALUES_AFTER_ALIAS_COMMA;
          pending_token = token;
          continue;
        }
        if (token_id == ML_RP) {
          values_state = DML_VALUES_AFTER_ALIAS_RP;
          continue;
        }
        mylite_parser_reject(ctx, token, "malformed DML VALUES row alias");
        return;
      } else if (values_state == DML_VALUES_AFTER_ALIAS_COMMA) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete DML VALUES row alias");
          return;
        }
        values_state = DML_VALUES_AFTER_ALIAS_COLUMN;
        continue;
      } else if (values_state == DML_VALUES_AFTER_ALIAS_RP) {
        if (token_id == ML_ON) {
          values_state = DML_VALUES_NONE;
          duplicate_state = DML_DUP_AFTER_ON;
          duplicate_strict = 1;
          pending_token = token;
          continue;
        }
        if (token_id != ML_SEMI) {
          mylite_parser_reject(ctx, token, "malformed DML VALUES row alias");
          return;
        }
      }
    }

    if (set_alias_state != DML_SET_ALIAS_NONE) {
      if (set_alias_state == DML_SET_ALIAS_AFTER_AS) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete INSERT SET row alias");
          return;
        }
        set_alias_state = DML_SET_ALIAS_AFTER_ALIAS;
        continue;
      }
      if (set_alias_state == DML_SET_ALIAS_AFTER_ALIAS) {
        if (token_id == ML_LP) {
          set_alias_state = DML_SET_ALIAS_AFTER_LP;
          pending_token = token;
          continue;
        }
        if (token_id == ML_ON) {
          set_alias_state = DML_SET_ALIAS_NONE;
          duplicate_state = DML_DUP_AFTER_ON;
          duplicate_strict = 1;
          pending_token = token;
          continue;
        }
        if (token_id != ML_SEMI) {
          mylite_parser_reject(ctx, token, "malformed INSERT SET row alias");
          return;
        }
      } else if (set_alias_state == DML_SET_ALIAS_AFTER_LP) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete INSERT SET row alias");
          return;
        }
        set_alias_state = DML_SET_ALIAS_AFTER_COLUMN;
        continue;
      } else if (set_alias_state == DML_SET_ALIAS_AFTER_COLUMN) {
        if (token_id == ML_COMMA) {
          set_alias_state = DML_SET_ALIAS_AFTER_COMMA;
          pending_token = token;
          continue;
        }
        if (token_id == ML_RP) {
          set_alias_state = DML_SET_ALIAS_AFTER_RP;
          continue;
        }
        mylite_parser_reject(ctx, token, "malformed INSERT SET row alias");
        return;
      } else if (set_alias_state == DML_SET_ALIAS_AFTER_COMMA) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete INSERT SET row alias");
          return;
        }
        set_alias_state = DML_SET_ALIAS_AFTER_COLUMN;
        continue;
      } else if (set_alias_state == DML_SET_ALIAS_AFTER_RP) {
        if (token_id == ML_ON) {
          set_alias_state = DML_SET_ALIAS_NONE;
          duplicate_state = DML_DUP_AFTER_ON;
          duplicate_strict = 1;
          pending_token = token;
          continue;
        }
        if (token_id != ML_SEMI) {
          mylite_parser_reject(ctx, token, "malformed INSERT SET row alias");
          return;
        }
      }
    }

    if (duplicate_state == DML_DUP_AFTER_ON) {
      if (token_id != ML_DUPLICATE) {
        if (duplicate_strict) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete INSERT duplicate key clause");
          return;
        }
        duplicate_state = DML_DUP_NONE;
        continue;
      }
      duplicate_state = DML_DUP_AFTER_DUPLICATE;
      continue;
    }
    if (duplicate_state == DML_DUP_AFTER_DUPLICATE) {
      if (token_id != ML_KEY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete INSERT duplicate key clause");
        return;
      }
      duplicate_state = DML_DUP_AFTER_KEY;
      continue;
    }
    if (duplicate_state == DML_DUP_AFTER_KEY) {
      if (token_id != ML_UPDATE) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete INSERT duplicate key clause");
        return;
      }
      duplicate_state = DML_DUP_NONE;
      assignment_state = DML_ASSIGN_TARGET;
      assignment_mode = DML_ASSIGNMENT_DUPLICATE;
      assignment_value_started = 0;
      pending_token = token;
      continue;
    }

    if (where_state == DML_WHERE_AFTER_WHERE) {
      if (dml_clause_operand_boundary(token_id)) {
        mylite_parser_reject(ctx, pending_token, "incomplete DML WHERE clause");
        return;
      }
      where_state = DML_WHERE_STARTED;
      if (token_opens_nested_expression(token_id)) {
        depth++;
      }
      continue;
    }

    if (order_state == DML_ORDER_AFTER_ORDER) {
      if (token_id != ML_BY) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DML ORDER BY clause");
        return;
      }
      order_state = DML_ORDER_AFTER_BY;
      pending_token = token;
      continue;
    }
    if (order_state == DML_ORDER_AFTER_BY) {
      if (token_id == ML_COMMA || dml_clause_operand_boundary(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DML ORDER BY clause");
        return;
      }
      order_state = DML_ORDER_STARTED;
      if (token_opens_nested_expression(token_id)) {
        depth++;
      }
      continue;
    }
    if (order_state == DML_ORDER_STARTED) {
      if (token_id == ML_COMMA) {
        order_state = DML_ORDER_AFTER_BY;
        pending_token = token;
        continue;
      }
      if (token_id == ML_ORDER || token_id == ML_WHERE) {
        mylite_parser_reject(ctx, token, "malformed DML clause order");
        return;
      }
      if (token_id == ML_ASC || token_id == ML_DESC) {
        order_state = DML_ORDER_AFTER_DIRECTION;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LIMIT) {
        order_state = DML_ORDER_NONE;
      } else if (token_id == ML_SEMI) {
        break;
      } else {
        if (token_opens_nested_expression(token_id)) {
          depth++;
        }
        continue;
      }
    }
    if (order_state == DML_ORDER_AFTER_DIRECTION) {
      if (token_id == ML_COMMA) {
        order_state = DML_ORDER_AFTER_BY;
        pending_token = token;
        continue;
      }
      if (token_id == ML_LIMIT) {
        order_state = DML_ORDER_NONE;
      } else if (token_id == ML_SEMI) {
        break;
      } else {
        mylite_parser_reject(ctx, pending_token,
                             "malformed DML ORDER BY direction");
        return;
      }
    }

    if (limit_state == DML_LIMIT_AFTER_LIMIT) {
      if (!dml_limit_option_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DML LIMIT clause");
        return;
      }
      limit_state = DML_LIMIT_AFTER_VALUE;
      continue;
    }
    if (limit_state == DML_LIMIT_AFTER_VALUE) {
      if (token_id == ML_SEMI) {
        break;
      }
      mylite_parser_reject(ctx, pending_token,
                           "malformed DML LIMIT clause");
      return;
    }

    if (token_id == ML_SEMI) {
      break;
    }

    if (kind == MYLITE_STATEMENT_UPDATE && token_id == ML_SET) {
      assignment_state = DML_ASSIGN_TARGET;
      assignment_mode = DML_ASSIGNMENT_UPDATE;
      assignment_value_started = 0;
      pending_token = token;
      continue;
    }

    if ((kind == MYLITE_STATEMENT_INSERT ||
         kind == MYLITE_STATEMENT_REPLACE) &&
        payload_kind == DML_PAYLOAD_NONE && token_id == ML_SET) {
      payload_kind = DML_PAYLOAD_SET;
      assignment_state = DML_ASSIGN_TARGET;
      assignment_mode = kind == MYLITE_STATEMENT_INSERT
                            ? DML_ASSIGNMENT_INSERT_SET
                            : DML_ASSIGNMENT_REPLACE_SET;
      assignment_value_started = 0;
      pending_token = token;
      continue;
    }

    if (kind == MYLITE_STATEMENT_INSERT && token_id == ML_ON) {
      duplicate_state = DML_DUP_AFTER_ON;
      duplicate_strict = payload_kind != DML_PAYLOAD_QUERY;
      pending_token = token;
      continue;
    }

    if (kind == MYLITE_STATEMENT_INSERT &&
        payload_kind == DML_PAYLOAD_SET && token_id == ML_AS) {
      set_alias_state = DML_SET_ALIAS_AFTER_AS;
      pending_token = token;
      continue;
    }

    if (kind == MYLITE_STATEMENT_REPLACE &&
        payload_kind == DML_PAYLOAD_SET && token_id == ML_AS) {
      mylite_parser_reject(ctx, token, "malformed REPLACE SET clause");
      return;
    }

    if ((kind == MYLITE_STATEMENT_INSERT ||
         kind == MYLITE_STATEMENT_REPLACE) &&
        payload_kind == DML_PAYLOAD_NONE &&
        (token_id == ML_VALUE || token_id == ML_VALUES)) {
      payload_kind = DML_PAYLOAD_VALUES;
      values_state = DML_VALUES_AFTER_VALUES;
      pending_token = token;
      continue;
    }

    if ((kind == MYLITE_STATEMENT_INSERT ||
         kind == MYLITE_STATEMENT_REPLACE) &&
        payload_kind == DML_PAYLOAD_NONE &&
        (token_id == ML_SELECT || token_id == ML_TABLE ||
         token_id == ML_WITH)) {
      payload_kind = DML_PAYLOAD_QUERY;
      continue;
    }

    if ((kind == MYLITE_STATEMENT_UPDATE ||
         kind == MYLITE_STATEMENT_DELETE) &&
        token_id == ML_WHERE) {
      if (seen_where || seen_order || seen_limit) {
        mylite_parser_reject(ctx, token, "malformed DML clause order");
        return;
      }
      seen_where = 1;
      where_state = DML_WHERE_AFTER_WHERE;
      pending_token = token;
      continue;
    }

    if ((kind == MYLITE_STATEMENT_UPDATE ||
         kind == MYLITE_STATEMENT_DELETE) &&
        token_id == ML_ORDER) {
      if (seen_order || seen_limit) {
        mylite_parser_reject(ctx, token, "malformed DML clause order");
        return;
      }
      seen_order = 1;
      order_state = DML_ORDER_AFTER_ORDER;
      pending_token = token;
      continue;
    }

    if ((kind == MYLITE_STATEMENT_UPDATE ||
         kind == MYLITE_STATEMENT_DELETE) &&
        token_id == ML_LIMIT) {
      if (seen_limit) {
        mylite_parser_reject(ctx, token, "malformed DML clause order");
        return;
      }
      seen_limit = 1;
      limit_state = DML_LIMIT_AFTER_LIMIT;
      pending_token = token;
      continue;
    }

    if (token_opens_nested_expression(token_id)) {
      depth++;
    }
  }

  if (assignment_state != DML_ASSIGN_NONE) {
    if (assignment_state == DML_ASSIGN_VALUE && assignment_value_started) {
      return;
    }
    mylite_parser_reject(ctx, pending_token, "incomplete DML assignment");
  } else if (duplicate_state != DML_DUP_NONE) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete INSERT duplicate key clause");
  } else if (values_state == DML_VALUES_AFTER_VALUES ||
             values_state == DML_VALUES_AFTER_ROW_KEYWORD ||
             values_state == DML_VALUES_AFTER_COMMA) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete DML VALUES row list");
  } else if (values_state == DML_VALUES_IN_ROW) {
    if (!dml_values_unclosed_string_fragment(values_row_last_token_id,
                                             values_row_last_token)) {
      mylite_parser_reject(ctx, pending_token,
                           "incomplete DML VALUES row list");
    }
  } else if (values_state == DML_VALUES_AFTER_AS ||
             values_state == DML_VALUES_AFTER_ALIAS_LP ||
             values_state == DML_VALUES_AFTER_ALIAS_COMMA) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete DML VALUES row alias");
  } else if (set_alias_state == DML_SET_ALIAS_AFTER_AS ||
             set_alias_state == DML_SET_ALIAS_AFTER_LP ||
             set_alias_state == DML_SET_ALIAS_AFTER_COMMA) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete INSERT SET row alias");
  } else if (where_state == DML_WHERE_AFTER_WHERE) {
    mylite_parser_reject(ctx, pending_token, "incomplete DML WHERE clause");
  } else if (order_state == DML_ORDER_AFTER_ORDER ||
             order_state == DML_ORDER_AFTER_BY) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete DML ORDER BY clause");
  } else if (limit_state == DML_LIMIT_AFTER_LIMIT) {
    mylite_parser_reject(ctx, pending_token, "incomplete DML LIMIT clause");
  }
}

void mylite_parser_validate_handler_statement(MyliteParseContext *ctx,
                                              MyliteToken start) {
  enum {
    HANDLER_WHERE_NONE,
    HANDLER_WHERE_AFTER_WHERE,
    HANDLER_WHERE_STARTED
  };
  enum {
    HANDLER_LIMIT_NONE,
    HANDLER_LIMIT_AFTER_LIMIT,
    HANDLER_LIMIT_AFTER_VALUE,
    HANDLER_LIMIT_AFTER_COMMA,
    HANDLER_LIMIT_AFTER_OFFSET,
    HANDLER_LIMIT_AFTER_FINAL_VALUE
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int saw_read = 0;
  int depth = 0;
  int seen_where = 0;
  int seen_limit = 0;
  int where_state = HANDLER_WHERE_NONE;
  int limit_state = HANDLER_LIMIT_NONE;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      } else {
        continue;
      }
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
      }
      continue;
    }

    if (!saw_read) {
      if (token_id == ML_READ) {
        saw_read = 1;
      }
      if (token_id == ML_SEMI) {
        break;
      }
      continue;
    }

    if (where_state == HANDLER_WHERE_AFTER_WHERE) {
      if (token_id == ML_LIMIT || token_id == ML_ORDER ||
          token_id == ML_SEMI || token_id == ML_WHERE) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete HANDLER WHERE clause");
        return;
      }
      where_state = HANDLER_WHERE_STARTED;
      if (token_opens_nested_expression(token_id)) {
        depth++;
      }
      continue;
    }

    if (where_state == HANDLER_WHERE_STARTED) {
      if (token_id == ML_ORDER || token_id == ML_WHERE) {
        mylite_parser_reject(ctx, token,
                             "malformed HANDLER READ clause");
        return;
      }
      if (token_id == ML_LIMIT) {
        if (seen_limit) {
          mylite_parser_reject(ctx, token,
                               "malformed HANDLER READ clause");
          return;
        }
        seen_limit = 1;
        where_state = HANDLER_WHERE_NONE;
        limit_state = HANDLER_LIMIT_AFTER_LIMIT;
        pending_token = token;
        continue;
      }
      if (token_id == ML_SEMI) {
        break;
      }
      if (token_opens_nested_expression(token_id)) {
        depth++;
      }
      continue;
    }

    if (limit_state == HANDLER_LIMIT_AFTER_LIMIT ||
        limit_state == HANDLER_LIMIT_AFTER_COMMA ||
        limit_state == HANDLER_LIMIT_AFTER_OFFSET) {
      if (!dml_limit_option_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete HANDLER LIMIT clause");
        return;
      }
      if (limit_state == HANDLER_LIMIT_AFTER_LIMIT) {
        limit_state = HANDLER_LIMIT_AFTER_VALUE;
      } else {
        limit_state = HANDLER_LIMIT_AFTER_FINAL_VALUE;
      }
      continue;
    }
    if (limit_state == HANDLER_LIMIT_AFTER_VALUE) {
      if (token_id == ML_COMMA) {
        limit_state = HANDLER_LIMIT_AFTER_COMMA;
        pending_token = token;
        continue;
      }
      if (token_id == ML_OFFSET) {
        limit_state = HANDLER_LIMIT_AFTER_OFFSET;
        pending_token = token;
        continue;
      }
      if (token_id == ML_SEMI) {
        break;
      }
      mylite_parser_reject(ctx, pending_token,
                           "malformed HANDLER LIMIT clause");
      return;
    }
    if (limit_state == HANDLER_LIMIT_AFTER_FINAL_VALUE) {
      if (token_id == ML_SEMI) {
        break;
      }
      mylite_parser_reject(ctx, pending_token,
                           "malformed HANDLER LIMIT clause");
      return;
    }

    if (token_id == ML_SEMI) {
      break;
    }

    if (token_id == ML_WHERE) {
      if (seen_where || seen_limit) {
        mylite_parser_reject(ctx, token,
                             "malformed HANDLER READ clause");
        return;
      }
      seen_where = 1;
      where_state = HANDLER_WHERE_AFTER_WHERE;
      pending_token = token;
      continue;
    }

    if (token_id == ML_LIMIT) {
      if (seen_limit) {
        mylite_parser_reject(ctx, token,
                             "malformed HANDLER READ clause");
        return;
      }
      seen_limit = 1;
      limit_state = HANDLER_LIMIT_AFTER_LIMIT;
      pending_token = token;
      continue;
    }

    if (token_opens_nested_expression(token_id)) {
      depth++;
    }
  }

  if (where_state == HANDLER_WHERE_AFTER_WHERE) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete HANDLER WHERE clause");
  } else if (limit_state == HANDLER_LIMIT_AFTER_LIMIT ||
             limit_state == HANDLER_LIMIT_AFTER_COMMA ||
             limit_state == HANDLER_LIMIT_AFTER_OFFSET) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete HANDLER LIMIT clause");
  }
}

void mylite_parser_validate_do_statement(MyliteParseContext *ctx,
                                          MyliteToken start) {
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int depth = 0;
  int need_expression = 1;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
        continue;
      } else {
        continue;
      }
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
      }
      continue;
    }

    if (token_id == ML_SEMI) {
      if (need_expression) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DO expression list");
      }
      break;
    }

    if (token_id == ML_COMMA) {
      if (need_expression) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DO expression list");
        return;
      }
      need_expression = 1;
      pending_token = token;
      continue;
    }

    if (need_expression) {
      if (do_clause_boundary(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete DO expression list");
        return;
      }
      need_expression = 0;
    } else if (do_clause_boundary(token_id)) {
      mylite_parser_reject(ctx, token, "malformed DO expression list");
      return;
    }

    if (token_opens_nested_expression(token_id)) {
      depth++;
    }
  }

  if (need_expression) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete DO expression list");
  }
}

void mylite_parser_validate_kill_statement(MyliteParseContext *ctx,
                                           MyliteToken start) {
  enum {
    KILL_AFTER_KILL,
    KILL_AFTER_MODE,
    KILL_AFTER_AT_SIGN,
    KILL_AFTER_TARGET,
    KILL_IN_CALL
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int depth = 0;
  int state = KILL_AFTER_KILL;
  int target_allows_call = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
        continue;
      } else {
        continue;
      }
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
        if (depth == 0 && state == KILL_IN_CALL) {
          state = KILL_AFTER_TARGET;
        }
      }
      continue;
    }

    if (token_id == ML_SEMI) {
      if (state == KILL_AFTER_KILL || state == KILL_AFTER_MODE ||
          state == KILL_AFTER_AT_SIGN) {
        mylite_parser_reject(ctx, pending_token, "incomplete KILL target");
      }
      break;
    }

    if (state == KILL_AFTER_KILL &&
        (token_id == ML_CONNECTION || token_id == ML_QUERY)) {
      state = KILL_AFTER_MODE;
      pending_token = token;
      continue;
    }

    if (state == KILL_AFTER_KILL || state == KILL_AFTER_MODE) {
      if (token_id == ML_AT_SIGN) {
        state = KILL_AFTER_AT_SIGN;
        target_allows_call = 0;
        pending_token = token;
        continue;
      }
      if (!kill_target_token(token_id)) {
        mylite_parser_reject(ctx, pending_token, "incomplete KILL target");
        return;
      }
      state = KILL_AFTER_TARGET;
      target_allows_call = kill_target_allows_call(token_id);
      continue;
    }

    if (state == KILL_AFTER_AT_SIGN) {
      if (!kill_at_sign_target_token(token_id)) {
        mylite_parser_reject(ctx, pending_token, "incomplete KILL target");
        return;
      }
      state = KILL_AFTER_TARGET;
      continue;
    }

    if (state == KILL_AFTER_TARGET && token_id == ML_LP &&
        target_allows_call) {
      state = KILL_IN_CALL;
      depth = 1;
      target_allows_call = 0;
      pending_token = token;
      continue;
    }

    mylite_parser_reject(ctx, token, "malformed KILL target");
    return;
  }

  if (state == KILL_AFTER_KILL || state == KILL_AFTER_MODE ||
      state == KILL_AFTER_AT_SIGN) {
    mylite_parser_reject(ctx, pending_token, "incomplete KILL target");
  } else if (state == KILL_IN_CALL) {
    mylite_parser_reject(ctx, pending_token, "incomplete KILL target");
  }
}

void mylite_parser_validate_create_table_statement(MyliteParseContext *ctx,
                                                    MyliteToken start) {
  enum {
    CREATE_TABLE_FIND_BODY,
    CREATE_TABLE_BODY_START,
    CREATE_TABLE_IN_DEFINITION
  };
  enum {
    CREATE_TABLE_FK_NONE,
    CREATE_TABLE_FK_AFTER_FOREIGN,
    CREATE_TABLE_FK_BEFORE_CHILD_LIST,
    CREATE_TABLE_FK_FIND_REFERENCES,
    CREATE_TABLE_FK_FIND_PARENT_LIST,
    CREATE_TABLE_FK_AFTER_PARENT_LIST,
    CREATE_TABLE_FK_AFTER_MATCH,
    CREATE_TABLE_FK_AFTER_ON,
    CREATE_TABLE_FK_AFTER_ON_ACTION,
    CREATE_TABLE_FK_AFTER_SET,
    CREATE_TABLE_FK_AFTER_NO
  };
  enum {
    CREATE_TABLE_CHECK_NONE,
    CREATE_TABLE_CHECK_READY,
    CREATE_TABLE_CHECK_AFTER_NOT,
    CREATE_TABLE_CHECK_DONE
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int state = CREATE_TABLE_FIND_BODY;
  int depth = 0;
  int index_candidate = 0;
  int foreign_state = CREATE_TABLE_FK_NONE;
  int check_pending = 0;
  int check_table_level = 0;
  int check_tail_state = CREATE_TABLE_CHECK_NONE;
  int column_needs_type = 0;
  int element_start = 0;
  int constraint_prefix = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
      }
      continue;
    }

    if (state == CREATE_TABLE_FIND_BODY) {
      if (token_id == ML_LP) {
        state = CREATE_TABLE_BODY_START;
        depth = 1;
        pending_token = token;
        continue;
      }
      if (token_id == ML_SEMI || create_table_query_body_start(token_id)) {
        break;
      }
      continue;
    }

    if (depth > 1) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
      }
      continue;
    }

    if (state == CREATE_TABLE_BODY_START) {
      if (create_table_query_body_start(token_id) || token_id == ML_LP) {
        return;
      }
      state = CREATE_TABLE_IN_DEFINITION;
      element_start = 1;
    }

    if (column_needs_type) {
      if (!create_table_column_type_start(token_id, token)) {
        mylite_parser_reject(ctx, token, "invalid CREATE TABLE column type");
        return;
      }
      column_needs_type = 0;
      continue;
    }

    if (check_pending) {
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE CHECK constraint");
        return;
      }
      if (!validate_parenthesized_nonempty_body(
              ctx, &lexer, token,
              "incomplete CREATE TABLE CHECK constraint")) {
        return;
      }
      check_pending = 0;
      if (check_table_level) {
        check_tail_state = CREATE_TABLE_CHECK_READY;
      }
      element_start = 0;
      constraint_prefix = 0;
      continue;
    }

    if (check_tail_state != CREATE_TABLE_CHECK_NONE) {
      if (token_id == ML_COMMA || token_id == ML_RP) {
        if (check_tail_state == CREATE_TABLE_CHECK_AFTER_NOT) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE CHECK constraint");
          return;
        }
        check_tail_state = CREATE_TABLE_CHECK_NONE;
        check_table_level = 0;
        if (token_id == ML_RP) {
          return;
        }
        element_start = 1;
        constraint_prefix = 0;
        continue;
      }

      if (check_tail_state == CREATE_TABLE_CHECK_READY) {
        if (token_id == ML_ENFORCED) {
          check_tail_state = CREATE_TABLE_CHECK_DONE;
          continue;
        }
        if (token_id == ML_NOT) {
          check_tail_state = CREATE_TABLE_CHECK_AFTER_NOT;
          pending_token = token;
          continue;
        }
      } else if (check_tail_state == CREATE_TABLE_CHECK_AFTER_NOT) {
        if (token_id == ML_ENFORCED) {
          check_tail_state = CREATE_TABLE_CHECK_DONE;
          continue;
        }
      }

      mylite_parser_reject(ctx, token,
                           "malformed CREATE TABLE CHECK constraint");
      return;
    }

    if (foreign_state != CREATE_TABLE_FK_NONE) {
      if (token_id == ML_COMMA || token_id == ML_RP) {
        if (foreign_state != CREATE_TABLE_FK_AFTER_PARENT_LIST) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE foreign key");
          return;
        }
        if (token_id == ML_RP) {
          return;
        }
        foreign_state = CREATE_TABLE_FK_NONE;
        element_start = 1;
        constraint_prefix = 0;
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_FOREIGN) {
        if (token_id != ML_KEY) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE foreign key");
          return;
        }
        foreign_state = CREATE_TABLE_FK_BEFORE_CHILD_LIST;
        pending_token = token;
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_BEFORE_CHILD_LIST) {
        if (token_id == ML_LP) {
          if (!validate_parenthesized_identifier_list(
                  ctx, &lexer, token,
                  "incomplete CREATE TABLE foreign key column list")) {
            return;
          }
          foreign_state = CREATE_TABLE_FK_FIND_REFERENCES;
          pending_token = token;
          continue;
        }
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_FIND_REFERENCES) {
        if (token_ascii_equal(token, "references")) {
          foreign_state = CREATE_TABLE_FK_FIND_PARENT_LIST;
          pending_token = token;
        }
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_FIND_PARENT_LIST) {
        if (token_id == ML_LP) {
          if (!validate_parenthesized_identifier_list(
                  ctx, &lexer, token,
                  "incomplete CREATE TABLE foreign key reference list")) {
            return;
          }
          foreign_state = CREATE_TABLE_FK_AFTER_PARENT_LIST;
        }
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_PARENT_LIST) {
        if (token_ascii_equal(token, "match")) {
          foreign_state = CREATE_TABLE_FK_AFTER_MATCH;
          pending_token = token;
          continue;
        }
        if (token_id == ML_ON) {
          foreign_state = CREATE_TABLE_FK_AFTER_ON;
          pending_token = token;
          continue;
        }
        mylite_parser_reject(ctx, token,
                             "malformed CREATE TABLE foreign key option");
        return;
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_MATCH) {
        if (!foreign_key_match_option(token_id, token)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE foreign key MATCH");
          return;
        }
        foreign_state = CREATE_TABLE_FK_AFTER_PARENT_LIST;
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_ON) {
        if (token_id != ML_DELETE && token_id != ML_UPDATE) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE foreign key option");
          return;
        }
        foreign_state = CREATE_TABLE_FK_AFTER_ON_ACTION;
        pending_token = token;
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_ON_ACTION) {
        if (foreign_key_reference_action_token(token_id)) {
          foreign_state = CREATE_TABLE_FK_AFTER_PARENT_LIST;
          continue;
        }
        if (token_id == ML_SET) {
          foreign_state = CREATE_TABLE_FK_AFTER_SET;
          pending_token = token;
          continue;
        }
        if (token_id == ML_NO) {
          foreign_state = CREATE_TABLE_FK_AFTER_NO;
          pending_token = token;
          continue;
        }
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE foreign key option");
        return;
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_SET) {
        if (token_id != ML_DEFAULT && token_id != ML_NULL) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE foreign key option");
          return;
        }
        foreign_state = CREATE_TABLE_FK_AFTER_PARENT_LIST;
        continue;
      }

      if (foreign_state == CREATE_TABLE_FK_AFTER_NO) {
        if (!token_ascii_equal(token, "action")) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete CREATE TABLE foreign key option");
          return;
        }
        foreign_state = CREATE_TABLE_FK_AFTER_PARENT_LIST;
        continue;
      }

      continue;
    }

    if (token_id == ML_RP) {
      if (index_candidate) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE index key part");
      }
      return;
    }

    if (token_id == ML_COMMA) {
      if (index_candidate) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE index key part");
        return;
      }
      element_start = 1;
      constraint_prefix = 0;
      check_pending = 0;
      check_table_level = 0;
      check_tail_state = CREATE_TABLE_CHECK_NONE;
      column_needs_type = 0;
      continue;
    }

    if (token_id == ML_LP) {
      if (index_candidate) {
        if (!validate_create_table_index_key_list(ctx, &lexer, token)) {
          return;
        }
        index_candidate = 0;
        constraint_prefix = 0;
        element_start = 0;
      } else {
        depth = 2;
      }
      continue;
    }

    if (token_id == ML_CHECK) {
      check_pending = 1;
      check_table_level = element_start || constraint_prefix > 0;
      element_start = 0;
      constraint_prefix = 0;
      pending_token = token;
      continue;
    }

    if (!element_start && constraint_prefix == 0 && !index_candidate) {
      continue;
    }

    if (element_start && token_id == ML_CONSTRAINT) {
      constraint_prefix = 1;
      element_start = 0;
      pending_token = token;
      continue;
    }

    if (constraint_prefix > 0) {
      if (alter_table_add_index_marker(token_id)) {
        index_candidate = 1;
        constraint_prefix = 0;
        pending_token = token;
        continue;
      }
      if (token_id == ML_FOREIGN) {
        foreign_state = CREATE_TABLE_FK_AFTER_FOREIGN;
        constraint_prefix = 0;
        pending_token = token;
        continue;
      }
      if (constraint_prefix == 1) {
        constraint_prefix = 2;
        continue;
      }
      constraint_prefix = 0;
      continue;
    }

    if (element_start && alter_table_add_index_marker(token_id)) {
      index_candidate = 1;
      element_start = 0;
      pending_token = token;
      continue;
    }

    if (element_start && token_id == ML_FOREIGN) {
      foreign_state = CREATE_TABLE_FK_AFTER_FOREIGN;
      element_start = 0;
      pending_token = token;
      continue;
    }

    if (element_start) {
      column_needs_type =
          create_table_column_name_needs_type_check(token_id, token);
      element_start = 0;
      pending_token = token;
      continue;
    }

    element_start = 0;
  }

  if (foreign_state != CREATE_TABLE_FK_NONE &&
      foreign_state != CREATE_TABLE_FK_AFTER_PARENT_LIST) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE TABLE foreign key");
  } else if (check_pending) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE TABLE CHECK constraint");
  } else if (check_tail_state == CREATE_TABLE_CHECK_AFTER_NOT) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE TABLE CHECK constraint");
  } else if (column_needs_type) {
    mylite_parser_reject(ctx, pending_token,
                         "invalid CREATE TABLE column type");
  } else if (index_candidate) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE TABLE index key part");
  }
}

void mylite_parser_validate_create_index_statement(MyliteParseContext *ctx,
                                                    MyliteToken start) {
  enum {
    INDEX_KEY_NEED_PART,
    INDEX_KEY_AFTER_NAME,
    INDEX_KEY_AFTER_DOT,
    INDEX_KEY_PREFIX_VALUE,
    INDEX_KEY_PREFIX_AFTER_VALUE,
    INDEX_KEY_AFTER_PART,
    INDEX_KEY_AFTER_DIRECTION,
    INDEX_KEY_IN_FUNCTION
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int saw_on = 0;
  int in_key_list = 0;
  int depth = 0;
  int key_state = INDEX_KEY_NEED_PART;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
        continue;
      } else {
        continue;
      }
    }

    if (!saw_on) {
      if (token_id == ML_ON) {
        saw_on = 1;
      }
      if (token_id == ML_SEMI) {
        break;
      }
      continue;
    }

    if (!in_key_list) {
      if (token_id == ML_LP) {
        in_key_list = 1;
        depth = 1;
        pending_token = token;
      }
      if (token_id == ML_SEMI) {
        break;
      }
      continue;
    }

    if (depth > 1) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
        if (depth == 1 && key_state == INDEX_KEY_IN_FUNCTION) {
          key_state = INDEX_KEY_AFTER_PART;
        }
      }
      continue;
    }

    if (key_state == INDEX_KEY_PREFIX_VALUE) {
      if (!create_index_prefix_length_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE INDEX key part");
        return;
      }
      key_state = INDEX_KEY_PREFIX_AFTER_VALUE;
      continue;
    }

    if (key_state == INDEX_KEY_PREFIX_AFTER_VALUE) {
      if (token_id != ML_RP) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE INDEX key part");
        return;
      }
      key_state = INDEX_KEY_AFTER_PART;
      continue;
    }

    if (token_id == ML_LP) {
      if (key_state == INDEX_KEY_NEED_PART) {
        key_state = INDEX_KEY_IN_FUNCTION;
        depth = 2;
        pending_token = token;
        continue;
      }
      if (key_state == INDEX_KEY_AFTER_NAME) {
        key_state = INDEX_KEY_PREFIX_VALUE;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token, "malformed CREATE INDEX key part");
      return;
    }

    if (token_id == ML_RP) {
      if (key_state == INDEX_KEY_NEED_PART ||
          key_state == INDEX_KEY_AFTER_DOT ||
          key_state == INDEX_KEY_IN_FUNCTION) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE INDEX key part");
      }
      return;
    }

    if (token_id == ML_COMMA) {
      if (key_state != INDEX_KEY_AFTER_NAME &&
          key_state != INDEX_KEY_AFTER_PART &&
          key_state != INDEX_KEY_AFTER_DIRECTION) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE INDEX key part");
        return;
      }
      key_state = INDEX_KEY_NEED_PART;
      pending_token = token;
      continue;
    }

    if (token_id == ML_ASC || token_id == ML_DESC) {
      if (key_state != INDEX_KEY_AFTER_NAME &&
          key_state != INDEX_KEY_AFTER_PART) {
        mylite_parser_reject(ctx, token, "malformed CREATE INDEX key part");
        return;
      }
      key_state = INDEX_KEY_AFTER_DIRECTION;
      continue;
    }

    if (token_id == ML_DOT) {
      if (key_state != INDEX_KEY_AFTER_NAME) {
        mylite_parser_reject(ctx, token, "malformed CREATE INDEX key part");
        return;
      }
      key_state = INDEX_KEY_AFTER_DOT;
      pending_token = token;
      continue;
    }

    if (key_state == INDEX_KEY_NEED_PART ||
        key_state == INDEX_KEY_AFTER_DOT) {
      if (!dml_row_alias_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE INDEX key part");
        return;
      }
      key_state = INDEX_KEY_AFTER_NAME;
      continue;
    }

    mylite_parser_reject(ctx, token, "malformed CREATE INDEX key part");
    return;
  }

  if (in_key_list) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete CREATE INDEX key part");
  }
}

void mylite_parser_validate_alter_table_statement(MyliteParseContext *ctx,
                                                   MyliteToken start) {
  enum {
    ALTER_INDEX_KEY_NEED_PART,
    ALTER_INDEX_KEY_AFTER_NAME,
    ALTER_INDEX_KEY_AFTER_DOT,
    ALTER_INDEX_KEY_PREFIX_VALUE,
    ALTER_INDEX_KEY_PREFIX_AFTER_VALUE,
    ALTER_INDEX_KEY_AFTER_PART,
    ALTER_INDEX_KEY_AFTER_DIRECTION,
    ALTER_INDEX_KEY_IN_FUNCTION
  };
  enum {
    ALTER_FK_NONE,
    ALTER_FK_AFTER_FOREIGN,
    ALTER_FK_BEFORE_CHILD_LIST,
    ALTER_FK_FIND_REFERENCES,
    ALTER_FK_FIND_PARENT_LIST,
    ALTER_FK_AFTER_PARENT_LIST,
    ALTER_FK_AFTER_MATCH,
    ALTER_FK_AFTER_ON,
    ALTER_FK_AFTER_ON_ACTION,
    ALTER_FK_AFTER_SET,
    ALTER_FK_AFTER_NO
  };
  enum {
    ALTER_CHECK_NONE,
    ALTER_CHECK_READY,
    ALTER_CHECK_AFTER_NOT,
    ALTER_CHECK_DONE
  };
  enum {
    ALTER_COLUMN_NONE,
    ALTER_COLUMN_MODIFY_EXPECT_NAME,
    ALTER_COLUMN_CHANGE_EXPECT_OLD_NAME,
    ALTER_COLUMN_CHANGE_EXPECT_NEW_NAME,
    ALTER_COLUMN_EXPECT_TYPE,
    ALTER_COLUMN_IN_DEFINITION
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int saw_table = 0;
  int table_ref_done = 0;
  int table_name_parts = 0;
  int table_dot_pending = 0;
  int add_scan = 0;
  int add_index_candidate = 0;
  int add_foreign_state = ALTER_FK_NONE;
  int check_pending = 0;
  int check_tail_state = ALTER_CHECK_NONE;
  int add_column_expect_name = 0;
  int add_column_type_pending = 0;
  int add_column_in_definition = 0;
  int add_constraint_prefix = 0;
  int alter_column_state = ALTER_COLUMN_NONE;
  int alter_column_optional_keyword = 0;
  ColumnDefinitionTailState column_tail_state =
      COLUMN_DEFINITION_TAIL_READY;
  int validate_key_list = 0;
  int depth = 0;
  int key_state = ALTER_INDEX_KEY_NEED_PART;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
        continue;
      } else {
        continue;
      }
    }

    if (!saw_table) {
      if (token_id != ML_TABLE) {
        return;
      }
      saw_table = 1;
      continue;
    }

    if (!table_ref_done) {
      if (token_id == ML_SEMI) {
        return;
      }
      if (table_dot_pending) {
        table_name_parts++;
        table_dot_pending = 0;
        continue;
      }
      if (table_name_parts == 0) {
        table_name_parts = 1;
        continue;
      }
      if (token_id == ML_DOT && table_name_parts == 1) {
        table_dot_pending = 1;
        continue;
      }
      table_ref_done = 1;
    }

    if (validate_key_list) {
      if (depth > 1) {
        if (token_opens_nested_expression(token_id)) {
          depth++;
        } else if (token_closes_nested_expression(token_id)) {
          depth--;
          if (depth == 1 &&
              key_state == ALTER_INDEX_KEY_IN_FUNCTION) {
            key_state = ALTER_INDEX_KEY_AFTER_PART;
          }
        }
        continue;
      }

      if (key_state == ALTER_INDEX_KEY_PREFIX_VALUE) {
        if (!create_index_prefix_length_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE index key part");
          return;
        }
        key_state = ALTER_INDEX_KEY_PREFIX_AFTER_VALUE;
        continue;
      }

      if (key_state == ALTER_INDEX_KEY_PREFIX_AFTER_VALUE) {
        if (token_id != ML_RP) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE index key part");
          return;
        }
        key_state = ALTER_INDEX_KEY_AFTER_PART;
        continue;
      }

      if (token_id == ML_LP) {
        if (key_state == ALTER_INDEX_KEY_NEED_PART) {
          key_state = ALTER_INDEX_KEY_IN_FUNCTION;
          depth = 2;
          pending_token = token;
          continue;
        }
        if (key_state == ALTER_INDEX_KEY_AFTER_NAME) {
          key_state = ALTER_INDEX_KEY_PREFIX_VALUE;
          pending_token = token;
          continue;
        }
        mylite_parser_reject(ctx, token,
                             "malformed ALTER TABLE index key part");
        return;
      }

      if (token_id == ML_RP) {
        if (key_state == ALTER_INDEX_KEY_NEED_PART ||
            key_state == ALTER_INDEX_KEY_AFTER_DOT ||
            key_state == ALTER_INDEX_KEY_IN_FUNCTION) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE index key part");
          return;
        }
        validate_key_list = 0;
        add_scan = 0;
        add_index_candidate = 0;
        continue;
      }

      if (token_id == ML_COMMA) {
        if (key_state != ALTER_INDEX_KEY_AFTER_NAME &&
            key_state != ALTER_INDEX_KEY_AFTER_PART &&
            key_state != ALTER_INDEX_KEY_AFTER_DIRECTION) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE index key part");
          return;
        }
        key_state = ALTER_INDEX_KEY_NEED_PART;
        pending_token = token;
        continue;
      }

      if (token_id == ML_ASC || token_id == ML_DESC) {
        if (key_state != ALTER_INDEX_KEY_AFTER_NAME &&
            key_state != ALTER_INDEX_KEY_AFTER_PART) {
          mylite_parser_reject(ctx, token,
                               "malformed ALTER TABLE index key part");
          return;
        }
        key_state = ALTER_INDEX_KEY_AFTER_DIRECTION;
        continue;
      }

      if (token_id == ML_DOT) {
        if (key_state != ALTER_INDEX_KEY_AFTER_NAME) {
          mylite_parser_reject(ctx, token,
                               "malformed ALTER TABLE index key part");
          return;
        }
        key_state = ALTER_INDEX_KEY_AFTER_DOT;
        pending_token = token;
        continue;
      }

      if (key_state == ALTER_INDEX_KEY_NEED_PART ||
          key_state == ALTER_INDEX_KEY_AFTER_DOT) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE index key part");
          return;
        }
        key_state = ALTER_INDEX_KEY_AFTER_NAME;
        continue;
      }

      mylite_parser_reject(ctx, token,
                           "malformed ALTER TABLE index key part");
      return;
    }

    if (check_pending) {
      if (token_id != ML_LP) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete ALTER TABLE CHECK constraint");
        return;
      }
      if (!validate_parenthesized_nonempty_body(
              ctx, &lexer, token,
              "incomplete ALTER TABLE CHECK constraint")) {
        return;
      }
      check_pending = 0;
      check_tail_state = ALTER_CHECK_READY;
      add_scan = 0;
      add_index_candidate = 0;
      continue;
    }

    if (check_tail_state != ALTER_CHECK_NONE) {
      if (token_id == ML_COMMA || token_id == ML_SEMI) {
        if (check_tail_state == ALTER_CHECK_AFTER_NOT) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE CHECK constraint");
          return;
        }
        check_tail_state = ALTER_CHECK_NONE;
        add_column_in_definition = 0;
        alter_column_state = ALTER_COLUMN_NONE;
        column_tail_state = COLUMN_DEFINITION_TAIL_READY;
        if (token_id == ML_SEMI) {
          break;
        }
        add_scan = 0;
        add_index_candidate = 0;
        continue;
      }

      if (check_tail_state == ALTER_CHECK_READY) {
        if (token_id == ML_ENFORCED) {
          check_tail_state = ALTER_CHECK_DONE;
          continue;
        }
        if (token_id == ML_NOT) {
          check_tail_state = ALTER_CHECK_AFTER_NOT;
          pending_token = token;
          continue;
        }
      } else if (check_tail_state == ALTER_CHECK_AFTER_NOT) {
        if (token_id == ML_ENFORCED) {
          check_tail_state = ALTER_CHECK_DONE;
          continue;
        }
      }

      mylite_parser_reject(ctx, token,
                           "malformed ALTER TABLE CHECK constraint");
      return;
    }

    if (add_foreign_state != ALTER_FK_NONE) {
      if (token_id == ML_COMMA || token_id == ML_SEMI) {
        if (add_foreign_state != ALTER_FK_AFTER_PARENT_LIST) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE foreign key");
          return;
        }
        add_foreign_state = ALTER_FK_NONE;
        add_scan = 0;
        add_index_candidate = 0;
        if (token_id == ML_SEMI) {
          break;
        }
        continue;
      }

      if (add_foreign_state == ALTER_FK_AFTER_FOREIGN) {
        if (token_id != ML_KEY) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE foreign key");
          return;
        }
        add_foreign_state = ALTER_FK_BEFORE_CHILD_LIST;
        pending_token = token;
        continue;
      }

      if (add_foreign_state == ALTER_FK_BEFORE_CHILD_LIST) {
        if (token_id == ML_LP) {
          if (!validate_parenthesized_identifier_list(
                  ctx, &lexer, token,
                  "incomplete ALTER TABLE foreign key column list")) {
            return;
          }
          add_foreign_state = ALTER_FK_FIND_REFERENCES;
          pending_token = token;
          continue;
        }
        continue;
      }

      if (add_foreign_state == ALTER_FK_FIND_REFERENCES) {
        if (token_ascii_equal(token, "references")) {
          add_foreign_state = ALTER_FK_FIND_PARENT_LIST;
          pending_token = token;
        }
        continue;
      }

      if (add_foreign_state == ALTER_FK_FIND_PARENT_LIST) {
        if (token_id == ML_LP) {
          if (!validate_parenthesized_identifier_list(
                  ctx, &lexer, token,
                  "incomplete ALTER TABLE foreign key reference list")) {
            return;
          }
          add_foreign_state = ALTER_FK_AFTER_PARENT_LIST;
        }
        continue;
      }

      if (add_foreign_state == ALTER_FK_AFTER_PARENT_LIST) {
        if (token_ascii_equal(token, "match")) {
          add_foreign_state = ALTER_FK_AFTER_MATCH;
          pending_token = token;
          continue;
        }
        if (token_id == ML_ON) {
          add_foreign_state = ALTER_FK_AFTER_ON;
          pending_token = token;
          continue;
        }
        if (token_id == ML_ALGORITHM || token_id == ML_LOCK ||
            token_id == ML_PARTITION || token_id == ML_REMOVE) {
          add_foreign_state = ALTER_FK_NONE;
          add_scan = 0;
          add_index_candidate = 0;
          continue;
        }
        mylite_parser_reject(ctx, token,
                             "malformed ALTER TABLE foreign key option");
        return;
      }

      if (add_foreign_state == ALTER_FK_AFTER_MATCH) {
        if (!foreign_key_match_option(token_id, token)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE foreign key MATCH");
          return;
        }
        add_foreign_state = ALTER_FK_AFTER_PARENT_LIST;
        continue;
      }

      if (add_foreign_state == ALTER_FK_AFTER_ON) {
        if (token_id != ML_DELETE && token_id != ML_UPDATE) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE foreign key option");
          return;
        }
        add_foreign_state = ALTER_FK_AFTER_ON_ACTION;
        pending_token = token;
        continue;
      }

      if (add_foreign_state == ALTER_FK_AFTER_ON_ACTION) {
        if (foreign_key_reference_action_token(token_id)) {
          add_foreign_state = ALTER_FK_AFTER_PARENT_LIST;
          continue;
        }
        if (token_id == ML_SET) {
          add_foreign_state = ALTER_FK_AFTER_SET;
          pending_token = token;
          continue;
        }
        if (token_id == ML_NO) {
          add_foreign_state = ALTER_FK_AFTER_NO;
          pending_token = token;
          continue;
        }
        mylite_parser_reject(ctx, pending_token,
                             "incomplete ALTER TABLE foreign key option");
        return;
      }

      if (add_foreign_state == ALTER_FK_AFTER_SET) {
        if (token_id != ML_DEFAULT && token_id != ML_NULL) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE foreign key option");
          return;
        }
        add_foreign_state = ALTER_FK_AFTER_PARENT_LIST;
        continue;
      }

      if (add_foreign_state == ALTER_FK_AFTER_NO) {
        if (!token_ascii_equal(token, "action")) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE foreign key option");
          return;
        }
        add_foreign_state = ALTER_FK_AFTER_PARENT_LIST;
        continue;
      }

      continue;
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
      }
      continue;
    }

    if (token_id == ML_SEMI) {
      break;
    }

    if (token_id == ML_COMMA) {
      if ((add_column_in_definition ||
           alter_column_state == ALTER_COLUMN_IN_DEFINITION) &&
          !column_definition_tail_complete(column_tail_state)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete ALTER TABLE column definition");
        return;
      }
      add_scan = 0;
      add_index_candidate = 0;
      add_foreign_state = ALTER_FK_NONE;
      check_pending = 0;
      check_tail_state = ALTER_CHECK_NONE;
      add_column_expect_name = 0;
      add_column_type_pending = 0;
      add_column_in_definition = 0;
      add_constraint_prefix = 0;
      alter_column_state = ALTER_COLUMN_NONE;
      alter_column_optional_keyword = 0;
      column_tail_state = COLUMN_DEFINITION_TAIL_READY;
      continue;
    }

    if (alter_column_state != ALTER_COLUMN_NONE) {
      if (alter_column_state == ALTER_COLUMN_MODIFY_EXPECT_NAME ||
          alter_column_state == ALTER_COLUMN_CHANGE_EXPECT_OLD_NAME) {
        if (alter_column_optional_keyword && token_id == ML_COLUMN) {
          alter_column_optional_keyword = 0;
          continue;
        }
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE column definition");
          return;
        }
        alter_column_optional_keyword = 0;
        if (alter_column_state == ALTER_COLUMN_MODIFY_EXPECT_NAME) {
          alter_column_state =
              create_table_column_name_needs_type_check(token_id, token)
                  ? ALTER_COLUMN_EXPECT_TYPE
                  : ALTER_COLUMN_IN_DEFINITION;
        } else {
          alter_column_state = ALTER_COLUMN_CHANGE_EXPECT_NEW_NAME;
        }
        pending_token = token;
        continue;
      }

      if (alter_column_state == ALTER_COLUMN_CHANGE_EXPECT_NEW_NAME) {
        if (!dml_row_alias_token(token_id)) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete ALTER TABLE column definition");
          return;
        }
        alter_column_state =
            create_table_column_name_needs_type_check(token_id, token)
                ? ALTER_COLUMN_EXPECT_TYPE
                : ALTER_COLUMN_IN_DEFINITION;
        pending_token = token;
        continue;
      }

      if (alter_column_state == ALTER_COLUMN_EXPECT_TYPE) {
        if (!create_table_column_type_start(token_id, token)) {
          mylite_parser_reject(ctx, token, "invalid ALTER TABLE column type");
          return;
        }
        alter_column_state = ALTER_COLUMN_IN_DEFINITION;
        column_tail_state = COLUMN_DEFINITION_TAIL_READY;
        continue;
      }

      if (alter_column_state == ALTER_COLUMN_IN_DEFINITION) {
        if (column_definition_tail_complete(column_tail_state) &&
            (token_id == ML_PARTITION || token_id == ML_REMOVE)) {
          alter_column_state = ALTER_COLUMN_NONE;
          add_scan = 0;
          add_index_candidate = 0;
          column_tail_state = COLUMN_DEFINITION_TAIL_READY;
          continue;
        }
        if (!column_definition_tail_token(
                ctx, token_id, token, &column_tail_state, &depth,
                &check_pending, &pending_token,
                "malformed ALTER TABLE column definition")) {
          return;
        }
        continue;
      }
    }

    if (token_id == ML_ADD) {
      add_scan = 1;
      add_index_candidate = 0;
      add_foreign_state = ALTER_FK_NONE;
      check_pending = 0;
      check_tail_state = ALTER_CHECK_NONE;
      add_column_expect_name = 0;
      add_column_type_pending = 0;
      add_column_in_definition = 0;
      add_constraint_prefix = 0;
      alter_column_state = ALTER_COLUMN_NONE;
      alter_column_optional_keyword = 0;
      column_tail_state = COLUMN_DEFINITION_TAIL_READY;
      pending_token = token;
      continue;
    }

    if (!add_scan && token_id == ML_MODIFY) {
      alter_column_state = ALTER_COLUMN_MODIFY_EXPECT_NAME;
      alter_column_optional_keyword = 1;
      pending_token = token;
      continue;
    }

    if (!add_scan && token_id == ML_CHANGE) {
      alter_column_state = ALTER_COLUMN_CHANGE_EXPECT_OLD_NAME;
      alter_column_optional_keyword = 1;
      pending_token = token;
      continue;
    }

    if (!add_scan) {
      continue;
    }

    if (add_column_type_pending) {
      if (!create_table_column_type_start(token_id, token)) {
        mylite_parser_reject(ctx, token, "invalid ALTER TABLE column type");
        return;
      }
      add_column_type_pending = 0;
      add_column_in_definition = 1;
      column_tail_state = COLUMN_DEFINITION_TAIL_READY;
      continue;
    }

    if (add_column_in_definition) {
      if (column_definition_tail_complete(column_tail_state) &&
          (token_id == ML_PARTITION || token_id == ML_REMOVE)) {
        add_column_in_definition = 0;
        add_scan = 0;
        add_index_candidate = 0;
        column_tail_state = COLUMN_DEFINITION_TAIL_READY;
        continue;
      }
      if (!column_definition_tail_token(
              ctx, token_id, token, &column_tail_state, &depth,
              &check_pending, &pending_token,
              "malformed ALTER TABLE column definition")) {
        return;
      }
      continue;
    }

    if (token_id == ML_CONSTRAINT) {
      add_constraint_prefix = 1;
      pending_token = token;
      continue;
    }

    if (add_constraint_prefix > 0) {
      if (alter_table_add_index_marker(token_id)) {
        add_index_candidate = 1;
        add_constraint_prefix = 0;
        continue;
      }
      if (token_id == ML_FOREIGN) {
        add_foreign_state = ALTER_FK_AFTER_FOREIGN;
        add_constraint_prefix = 0;
        pending_token = token;
        continue;
      }
      if (token_id == ML_CHECK) {
        check_pending = 1;
        add_constraint_prefix = 0;
        pending_token = token;
        continue;
      }
      if (add_constraint_prefix == 1) {
        add_constraint_prefix = 2;
      }
      continue;
    }

    if (token_id == ML_COLUMN) {
      add_column_expect_name = 1;
      continue;
    }

    if (add_column_expect_name && token_id == ML_LP) {
      depth = 1;
      add_scan = 0;
      add_column_expect_name = 0;
      continue;
    }

    if (token_id == ML_FOREIGN) {
      add_foreign_state = ALTER_FK_AFTER_FOREIGN;
      pending_token = token;
      continue;
    }

    if (token_id == ML_CHECK) {
      check_pending = 1;
      pending_token = token;
      continue;
    }

    if (alter_table_add_non_index_marker(token_id)) {
      add_scan = 0;
      add_index_candidate = 0;
      continue;
    }

    if (alter_table_add_index_marker(token_id)) {
      add_index_candidate = 1;
      continue;
    }

    if (!add_index_candidate && token_id != ML_LP) {
      add_column_type_pending =
          create_table_column_name_needs_type_check(token_id, token);
      add_column_expect_name = 0;
      pending_token = token;
      continue;
    }

    if (token_id == ML_LP) {
      if (add_index_candidate) {
        validate_key_list = 1;
        depth = 1;
        key_state = ALTER_INDEX_KEY_NEED_PART;
        pending_token = token;
      } else {
        depth = 1;
      }
    }
  }

  if (add_foreign_state != ALTER_FK_NONE &&
      add_foreign_state != ALTER_FK_AFTER_PARENT_LIST) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete ALTER TABLE foreign key");
  } else if (check_pending) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete ALTER TABLE CHECK constraint");
  } else if (check_tail_state == ALTER_CHECK_AFTER_NOT) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete ALTER TABLE CHECK constraint");
  } else if (add_column_type_pending) {
    mylite_parser_reject(ctx, pending_token,
                         "invalid ALTER TABLE column type");
  } else if (alter_column_state != ALTER_COLUMN_NONE &&
             alter_column_state != ALTER_COLUMN_IN_DEFINITION) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete ALTER TABLE column definition");
  } else if ((add_column_in_definition ||
              alter_column_state == ALTER_COLUMN_IN_DEFINITION) &&
             !column_definition_tail_complete(column_tail_state)) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete ALTER TABLE column definition");
  } else if (validate_key_list) {
    mylite_parser_reject(ctx, pending_token,
                         "incomplete ALTER TABLE index key part");
  }
}

void mylite_parser_validate_event_statement(MyliteParseContext *ctx,
                                             MyliteToken start) {
  enum {
    EVENT_FIND_ON,
    EVENT_AFTER_ON,
    EVENT_AFTER_SCHEDULE,
    EVENT_AT_TIMESTAMP,
    EVENT_AT_AFTER_PLUS,
    EVENT_AT_AFTER_INTERVAL,
    EVENT_AT_INTERVAL_VALUE,
    EVENT_AT_INTERVAL_DONE,
    EVENT_EVERY_VALUE,
    EVENT_EVERY_DONE,
    EVENT_OPTION_TIMESTAMP,
    EVENT_OPTION_AFTER_PLUS,
    EVENT_OPTION_AFTER_INTERVAL,
    EVENT_OPTION_INTERVAL_VALUE,
    EVENT_OPTION_INTERVAL_DONE
  };
  MyliteLexer lexer;
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int saw_statement = 0;
  int depth = 0;
  int state = EVENT_FIND_ON;
  int value_started = 0;

  mylite_lexer_init(&lexer, ctx->sql, ctx->length, ctx->result);
  while ((token_id = mylite_lexer_next(&lexer, &token)) > 0) {
    if (!saw_statement) {
      if (token.offset == start.offset) {
        saw_statement = 1;
        continue;
      } else {
        continue;
      }
    }

    if (depth > 0) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
      }
      continue;
    }

    if (state == EVENT_FIND_ON) {
      if (token_id == ML_ON) {
        state = EVENT_AFTER_ON;
        pending_token = token;
      } else if (token_id == ML_SEMI) {
        break;
      }
      continue;
    }

    if (state == EVENT_AFTER_ON) {
      if (token_id == ML_SCHEDULE) {
        state = EVENT_AFTER_SCHEDULE;
        pending_token = token;
      } else {
        state = EVENT_FIND_ON;
      }
      continue;
    }

    if (state == EVENT_AFTER_SCHEDULE) {
      if (token_id == ML_AT) {
        state = EVENT_AT_TIMESTAMP;
        value_started = 0;
        pending_token = token;
        continue;
      }
      if (token_id == ML_EVERY) {
        state = EVENT_EVERY_VALUE;
        value_started = 0;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, pending_token,
                           "incomplete EVENT schedule");
      return;
    }

    if (event_schedule_boundary(token_id)) {
      if (state == EVENT_AT_TIMESTAMP ||
          state == EVENT_OPTION_TIMESTAMP) {
        if (value_started) {
          return;
        }
      } else if (state == EVENT_AT_INTERVAL_DONE ||
                 state == EVENT_EVERY_DONE ||
                 state == EVENT_OPTION_INTERVAL_DONE) {
        return;
      }
      mylite_parser_reject(ctx, pending_token,
                           "incomplete EVENT schedule");
      return;
    }

    if (state == EVENT_EVERY_DONE &&
        event_schedule_option_start(token)) {
      state = EVENT_OPTION_TIMESTAMP;
      value_started = 0;
      pending_token = token;
      continue;
    }

    if ((state == EVENT_OPTION_TIMESTAMP ||
         state == EVENT_OPTION_INTERVAL_DONE) &&
        event_schedule_option_start(token)) {
      if (state == EVENT_OPTION_TIMESTAMP && !value_started) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete EVENT schedule");
        return;
      }
      state = EVENT_OPTION_TIMESTAMP;
      value_started = 0;
      pending_token = token;
      continue;
    }

    if (state == EVENT_AT_TIMESTAMP ||
        state == EVENT_OPTION_TIMESTAMP) {
      if (token_is_plus(token) && value_started) {
        state = state == EVENT_AT_TIMESTAMP ? EVENT_AT_AFTER_PLUS
                                            : EVENT_OPTION_AFTER_PLUS;
        pending_token = token;
        continue;
      }
      value_started = 1;
      if (token_opens_nested_expression(token_id)) {
        depth++;
      }
      continue;
    }

    if (state == EVENT_AT_AFTER_PLUS ||
        state == EVENT_OPTION_AFTER_PLUS) {
      if (token_id != ML_INTERVAL) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete EVENT schedule interval");
        return;
      }
      state = state == EVENT_AT_AFTER_PLUS ? EVENT_AT_AFTER_INTERVAL
                                           : EVENT_OPTION_AFTER_INTERVAL;
      pending_token = token;
      continue;
    }

    if (state == EVENT_AT_AFTER_INTERVAL ||
        state == EVENT_OPTION_AFTER_INTERVAL) {
      if (event_interval_unit_token(token)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete EVENT schedule interval");
        return;
      }
      state = state == EVENT_AT_AFTER_INTERVAL
                  ? EVENT_AT_INTERVAL_VALUE
                  : EVENT_OPTION_INTERVAL_VALUE;
      value_started = 1;
      if (token_opens_nested_expression(token_id)) {
        depth++;
      }
      continue;
    }

    if (state == EVENT_AT_INTERVAL_VALUE ||
        state == EVENT_OPTION_INTERVAL_VALUE) {
      if (event_interval_unit_token(token)) {
        state = state == EVENT_AT_INTERVAL_VALUE
                    ? EVENT_AT_INTERVAL_DONE
                    : EVENT_OPTION_INTERVAL_DONE;
        continue;
      }
      if (token_opens_nested_expression(token_id)) {
        depth++;
      }
      continue;
    }

    if (state == EVENT_AT_INTERVAL_DONE ||
        state == EVENT_OPTION_INTERVAL_DONE) {
      if (token_is_plus(token)) {
        state = state == EVENT_AT_INTERVAL_DONE ? EVENT_AT_AFTER_PLUS
                                                : EVENT_OPTION_AFTER_PLUS;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token, "malformed EVENT schedule");
      return;
    }

    if (state == EVENT_EVERY_VALUE) {
      if (event_interval_unit_token(token)) {
        if (!value_started) {
          mylite_parser_reject(ctx, pending_token,
                               "incomplete EVENT schedule interval");
          return;
        }
        state = EVENT_EVERY_DONE;
        continue;
      }
      value_started = 1;
      if (token_opens_nested_expression(token_id)) {
        depth++;
      }
      continue;
    }

    if (state == EVENT_EVERY_DONE) {
      mylite_parser_reject(ctx, token, "malformed EVENT schedule");
      return;
    }
  }

  if (state != EVENT_FIND_ON) {
    if (state == EVENT_AT_TIMESTAMP ||
        state == EVENT_OPTION_TIMESTAMP) {
      if (value_started) {
        return;
      }
    } else if (state == EVENT_AT_INTERVAL_DONE ||
               state == EVENT_EVERY_DONE ||
               state == EVENT_OPTION_INTERVAL_DONE) {
      return;
    }
    mylite_parser_reject(ctx, pending_token,
                         "incomplete EVENT schedule");
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

static int do_clause_boundary(int token_id) {
  return token_id == ML_FROM || token_id == ML_GROUP || token_id == ML_HAVING ||
         token_id == ML_INTO || token_id == ML_LIMIT || token_id == ML_ORDER ||
         token_id == ML_WHERE;
}

static int kill_at_sign_target_token(int token_id) {
  return token_id == ML_ATOM || token_id == ML_DOUBLE_QUOTED_STRING ||
         token_id == ML_QUOTED_ID || token_id == ML_STRING_LITERAL;
}

static int kill_target_allows_call(int token_id) {
  return token_id != ML_AT_EMPTY && token_id != ML_AT_HOST &&
         token_id != ML_BOOLEAN_NUMBER && token_id != ML_FACTOR_NUMBER &&
         token_id != ML_NUMBER_LITERAL;
}

static int kill_target_token(int token_id) {
  return token_id == ML_AT_HOST || token_id == ML_BOOLEAN_NUMBER ||
         token_id == ML_FACTOR_NUMBER || token_id == ML_NUMBER_LITERAL ||
         dml_row_alias_token(token_id);
}

static int create_table_query_body_start(int token_id) {
  return token_id == ML_AS || token_id == ML_LIKE || token_id == ML_SELECT ||
         token_id == ML_TABLE || token_id == ML_VALUES || token_id == ML_WITH;
}

static int create_table_column_name_needs_type_check(int token_id,
                                                     MyliteToken token) {
  size_t i;

  if (token_id == ML_BOOLEAN_NUMBER || token_id == ML_FACTOR_NUMBER ||
      token_id == ML_NUMBER_LITERAL) {
    return 0;
  }

  for (i = 0; i < token.length; i++) {
    if (token.start[i] == '$') {
      return 0;
    }
  }

  return 1;
}

static int create_table_column_type_start(int token_id, MyliteToken token) {
  return token_id == ML_BINARY || token_id == ML_CHARACTER ||
         token_id == ML_DECIMAL || token_id == ML_INT ||
         token_id == ML_INTEGER || token_id == ML_JSON || token_id == ML_REAL ||
         token_id == ML_SET || token_ascii_equal(token, "bigint") ||
         token_ascii_equal(token, "bit") || token_ascii_equal(token, "blob") ||
         token_ascii_equal(token, "bool") ||
         token_ascii_equal(token, "boolean") ||
         token_ascii_equal(token, "char") || token_ascii_equal(token, "date") ||
         token_ascii_equal(token, "datetime") ||
         token_ascii_equal(token, "dec") || token_ascii_equal(token, "double") ||
         token_ascii_equal(token, "enum") || token_ascii_equal(token, "fixed") ||
         token_ascii_equal(token, "float") ||
         token_ascii_equal(token, "float4") ||
         token_ascii_equal(token, "float8") ||
         token_ascii_equal(token, "geometry") ||
         token_ascii_equal(token, "geometrycollection") ||
         token_ascii_equal(token, "geomcollection") ||
         token_ascii_equal(token, "int1") || token_ascii_equal(token, "int2") ||
         token_ascii_equal(token, "int3") || token_ascii_equal(token, "int4") ||
         token_ascii_equal(token, "int8") ||
         token_ascii_equal(token, "linestring") ||
         token_ascii_equal(token, "long") ||
         token_ascii_equal(token, "longblob") ||
         token_ascii_equal(token, "longtext") ||
         token_ascii_equal(token, "mediumblob") ||
         token_ascii_equal(token, "mediumint") ||
         token_ascii_equal(token, "mediumtext") ||
         token_ascii_equal(token, "middleint") ||
         token_ascii_equal(token, "multilinestring") ||
         token_ascii_equal(token, "multipoint") ||
         token_ascii_equal(token, "multipolygon") ||
         token_ascii_equal(token, "national") ||
         token_ascii_equal(token, "nchar") ||
         token_ascii_equal(token, "numeric") ||
         token_ascii_equal(token, "nvarchar") ||
         token_ascii_equal(token, "point") ||
         token_ascii_equal(token, "polygon") ||
         token_ascii_equal(token, "serial") ||
         token_ascii_equal(token, "smallint") ||
         token_ascii_equal(token, "text") || token_ascii_equal(token, "time") ||
         token_ascii_equal(token, "timestamp") ||
         token_ascii_equal(token, "tinyblob") ||
         token_ascii_equal(token, "tinyint") ||
         token_ascii_equal(token, "tinytext") ||
         token_ascii_equal(token, "varbinary") ||
         token_ascii_equal(token, "varchar") ||
         token_ascii_equal(token, "year");
}

static int column_definition_tail_token(
    MyliteParseContext *ctx, int token_id, MyliteToken token,
    ColumnDefinitionTailState *state, int *depth, int *check_pending,
    MyliteToken *pending_token, const char *message) {
  if (token_id == ML_LP) {
    if (*state == COLUMN_DEFINITION_TAIL_AFTER_COMMENT ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_COLLATE ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_CHARACTER ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_CHARSET ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_COLUMN_FORMAT ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_STORAGE ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_SRID ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_AFTER ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_ON ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_REFERENCES ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_CONSTRAINT ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_CONSTRAINT_NAME ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_GENERATED ||
        *state == COLUMN_DEFINITION_TAIL_AFTER_ALWAYS) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    *depth = 1;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_NOT) {
    if (token_id != ML_NULL && !token_ascii_equal(token, "secondary")) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_PRIMARY) {
    if (token_id != ML_KEY) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_DEFAULT) {
    if (token_id == ML_VALUE) {
      *state = COLUMN_DEFINITION_TAIL_AFTER_DEFAULT_VALUE;
      *pending_token = token;
      return 1;
    }
    if (token_id == ML_MINUS) {
      *state = COLUMN_DEFINITION_TAIL_AFTER_VALUE_SIGN;
      *pending_token = token;
      return 1;
    }
    if (!column_definition_value_token(token_id, token)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_DEFAULT_VALUE) {
    if (column_definition_attribute_start(token_id, token)) {
      *state = COLUMN_DEFINITION_TAIL_READY;
    } else {
      if (token_id == ML_MINUS) {
        *state = COLUMN_DEFINITION_TAIL_AFTER_VALUE_SIGN;
        *pending_token = token;
        return 1;
      }
      if (!column_definition_value_token(token_id, token)) {
        mylite_parser_reject(ctx, *pending_token, message);
        return 0;
      }
      *state = COLUMN_DEFINITION_TAIL_READY;
      return 1;
    }
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_VALUE_SIGN) {
    if (token_id != ML_BOOLEAN_NUMBER && token_id != ML_FACTOR_NUMBER &&
        token_id != ML_NUMBER_LITERAL) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_COMMENT) {
    if (token_id != ML_DOUBLE_QUOTED_STRING &&
        token_id != ML_STRING_LITERAL) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_COLLATE ||
      *state == COLUMN_DEFINITION_TAIL_AFTER_CHARSET) {
    if (!column_definition_charset_name_token(token_id, token)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_CHARACTER) {
    if (token_id != ML_SET) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_AFTER_CHARSET;
    *pending_token = token;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_COLUMN_FORMAT) {
    if (token_id != ML_DEFAULT && !token_ascii_equal(token, "dynamic") &&
        !token_ascii_equal(token, "fixed")) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_STORAGE) {
    if (token_id != ML_DEFAULT && token_id != ML_MEMORY &&
        !token_ascii_equal(token, "disk")) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_SRID) {
    if (token_id != ML_BOOLEAN_NUMBER && token_id != ML_FACTOR_NUMBER &&
        token_id != ML_NUMBER_LITERAL) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_AFTER ||
      *state == COLUMN_DEFINITION_TAIL_AFTER_REFERENCES) {
    if (!dml_row_alias_token(token_id)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_ON) {
    if (token_id != ML_UPDATE) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_AFTER_ON_UPDATE;
    *pending_token = token;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_ON_UPDATE) {
    if (token_id == ML_MINUS) {
      *state = COLUMN_DEFINITION_TAIL_AFTER_VALUE_SIGN;
      *pending_token = token;
      return 1;
    }
    if (!column_definition_value_token(token_id, token)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_CONSTRAINT) {
    if (token_id == ML_CHECK) {
      *check_pending = 1;
      *state = COLUMN_DEFINITION_TAIL_READY;
      *pending_token = token;
      return 1;
    }
    if (column_definition_attribute_start(token_id, token)) {
      *state = COLUMN_DEFINITION_TAIL_READY;
    } else if (dml_row_alias_token(token_id)) {
      *state = COLUMN_DEFINITION_TAIL_AFTER_CONSTRAINT_NAME;
    } else {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *pending_token = token;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_CONSTRAINT_NAME) {
    if (token_id == ML_CHECK) {
      *check_pending = 1;
      *state = COLUMN_DEFINITION_TAIL_READY;
      *pending_token = token;
      return 1;
    }
    if (!column_definition_attribute_start(token_id, token)) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_READY;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_GENERATED) {
    if (token_ascii_equal(token, "always")) {
      *state = COLUMN_DEFINITION_TAIL_AFTER_ALWAYS;
      *pending_token = token;
      return 1;
    }
    if (token_id != ML_AS) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_AFTER_AS;
    *pending_token = token;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_ALWAYS) {
    if (token_id != ML_AS) {
      mylite_parser_reject(ctx, *pending_token, message);
      return 0;
    }
    *state = COLUMN_DEFINITION_TAIL_AFTER_AS;
    *pending_token = token;
    return 1;
  }

  if (*state == COLUMN_DEFINITION_TAIL_AFTER_AS) {
    mylite_parser_reject(ctx, *pending_token, message);
    return 0;
  }

  if (token_id == ML_CHECK) {
    *check_pending = 1;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_NOT) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_NOT;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_PRIMARY) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_PRIMARY;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_DEFAULT) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_DEFAULT;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_COMMENT) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_COMMENT;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_COLLATE) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_COLLATE;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_CHARACTER) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_CHARACTER;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_CHARSET) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_CHARSET;
    *pending_token = token;
    return 1;
  }

  if (token_ascii_equal(token, "column_format")) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_COLUMN_FORMAT;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_STORAGE) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_STORAGE;
    *pending_token = token;
    return 1;
  }

  if (token_ascii_equal(token, "srid")) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_SRID;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_AFTER) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_AFTER;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_ON) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_ON;
    *pending_token = token;
    return 1;
  }

  if (token_ascii_equal(token, "references")) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_REFERENCES;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_CONSTRAINT) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_CONSTRAINT;
    *pending_token = token;
    return 1;
  }

  if (token_ascii_equal(token, "generated")) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_GENERATED;
    *pending_token = token;
    return 1;
  }

  if (token_ascii_equal(token, "always")) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_ALWAYS;
    *pending_token = token;
    return 1;
  }

  if (token_id == ML_AS) {
    *state = COLUMN_DEFINITION_TAIL_AFTER_AS;
    *pending_token = token;
    return 1;
  }

  if (column_definition_type_modifier(token_id, token) ||
      create_table_column_type_start(token_id, token) ||
      token_id == ML_AUTO_INCREMENT || token_id == ML_FIRST ||
      token_id == ML_INVISIBLE || token_id == ML_KEY || token_id == ML_NULL ||
      token_id == ML_UNIQUE || token_id == ML_VISIBLE ||
      token_ascii_equal(token, "stored") || token_ascii_equal(token, "virtual")) {
    *state = COLUMN_DEFINITION_TAIL_READY;
    return 1;
  }

  mylite_parser_reject(ctx, token, message);
  return 0;
}

static int column_definition_tail_complete(ColumnDefinitionTailState state) {
  return state == COLUMN_DEFINITION_TAIL_READY ||
         state == COLUMN_DEFINITION_TAIL_AFTER_DEFAULT_VALUE;
}

static int column_definition_value_token(int token_id, MyliteToken token) {
  return token_id == ML_ATOM || token_id == ML_BOOLEAN_NUMBER ||
         token_id == ML_DOUBLE_QUOTED_STRING ||
         token_id == ML_FACTOR_NUMBER || token_id == ML_LABEL ||
         token_id == ML_NULL || token_id == ML_NUMBER_LITERAL ||
         token_id == ML_QUOTED_ID || token_id == ML_STRING_LITERAL ||
         token_ascii_equal(token, "false") || token_ascii_equal(token, "true");
}

static int column_definition_charset_name_token(int token_id,
                                                MyliteToken token) {
  return token_id == ML_BINARY || token_id == ML_DEFAULT ||
         token_id == ML_DOUBLE_QUOTED_STRING ||
         token_id == ML_STRING_LITERAL ||
         dml_row_alias_token(token_id) || token_ascii_equal(token, "ascii") ||
         token_ascii_equal(token, "unicode");
}

static int column_definition_attribute_start(int token_id, MyliteToken token) {
  return token_id == ML_AFTER || token_id == ML_AS ||
         token_id == ML_AUTO_INCREMENT || token_id == ML_CHARACTER ||
         token_id == ML_CHARSET || token_id == ML_CHECK ||
         token_id == ML_COLLATE || token_id == ML_COMMENT ||
         token_id == ML_CONSTRAINT || token_id == ML_DEFAULT ||
         token_id == ML_FIRST || token_id == ML_INVISIBLE ||
         token_id == ML_KEY || token_id == ML_NOT || token_id == ML_NULL ||
         token_id == ML_ON || token_id == ML_PRIMARY ||
         token_id == ML_STORAGE || token_id == ML_UNIQUE ||
         token_id == ML_VISIBLE || token_ascii_equal(token, "always") ||
         token_ascii_equal(token, "column_format") ||
         token_ascii_equal(token, "generated") ||
         token_ascii_equal(token, "references") ||
         token_ascii_equal(token, "srid") || token_ascii_equal(token, "stored") ||
         token_ascii_equal(token, "virtual");
}

static int column_definition_type_modifier(int token_id, MyliteToken token) {
  return token_id == ML_BINARY || token_id == ML_VALUE ||
         token_ascii_equal(token, "ascii") || token_ascii_equal(token, "byte") ||
         token_ascii_equal(token, "precision") ||
         token_ascii_equal(token, "signed") ||
         token_ascii_equal(token, "unicode") ||
         token_ascii_equal(token, "unsigned") ||
         token_ascii_equal(token, "varying") ||
         token_ascii_equal(token, "zerofill");
}

static int foreign_key_match_option(int token_id, MyliteToken token) {
  return token_id == ML_FULL || token_ascii_equal(token, "partial") ||
         token_ascii_equal(token, "simple");
}

static int foreign_key_reference_action_token(int token_id) {
  return token_id == ML_CASCADE || token_id == ML_RESTRICT;
}

static int validate_parenthesized_identifier_list(MyliteParseContext *ctx,
                                                  MyliteLexer *lexer,
                                                  MyliteToken start,
                                                  const char *message) {
  enum {
    CREATE_TABLE_IDENTIFIER_NEED_NAME,
    CREATE_TABLE_IDENTIFIER_AFTER_NAME
  };
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int state = CREATE_TABLE_IDENTIFIER_NEED_NAME;

  while ((token_id = mylite_lexer_next(lexer, &token)) > 0) {
    if (token_id == ML_RP) {
      if (state != CREATE_TABLE_IDENTIFIER_AFTER_NAME) {
        mylite_parser_reject(ctx, pending_token, message);
        return 0;
      }
      return 1;
    }

    if (token_id == ML_COMMA) {
      if (state != CREATE_TABLE_IDENTIFIER_AFTER_NAME) {
        mylite_parser_reject(ctx, pending_token, message);
        return 0;
      }
      state = CREATE_TABLE_IDENTIFIER_NEED_NAME;
      pending_token = token;
      continue;
    }

    if (state != CREATE_TABLE_IDENTIFIER_NEED_NAME ||
        !dml_row_alias_token(token_id)) {
      mylite_parser_reject(ctx, token, message);
      return 0;
    }

    state = CREATE_TABLE_IDENTIFIER_AFTER_NAME;
  }

  mylite_parser_reject(ctx, pending_token, message);
  return 0;
}

static int validate_parenthesized_nonempty_body(MyliteParseContext *ctx,
                                                MyliteLexer *lexer,
                                                MyliteToken start,
                                                const char *message) {
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int depth = 1;
  int saw_token = 0;

  while ((token_id = mylite_lexer_next(lexer, &token)) > 0) {
    if (depth == 1 && token_id == ML_RP) {
      if (!saw_token) {
        mylite_parser_reject(ctx, pending_token, message);
        return 0;
      }
      return 1;
    }

    saw_token = 1;
    if (token_opens_nested_expression(token_id)) {
      depth++;
    } else if (token_closes_nested_expression(token_id)) {
      depth--;
    }
  }

  mylite_parser_reject(ctx, pending_token, message);
  return 0;
}

static int validate_create_table_index_key_list(MyliteParseContext *ctx,
                                                MyliteLexer *lexer,
                                                MyliteToken start) {
  enum {
    CREATE_TABLE_KEY_NEED_PART,
    CREATE_TABLE_KEY_AFTER_NAME,
    CREATE_TABLE_KEY_AFTER_DOT,
    CREATE_TABLE_KEY_PREFIX_VALUE,
    CREATE_TABLE_KEY_PREFIX_AFTER_VALUE,
    CREATE_TABLE_KEY_AFTER_PART,
    CREATE_TABLE_KEY_AFTER_DIRECTION,
    CREATE_TABLE_KEY_IN_FUNCTION
  };
  MyliteToken token;
  MyliteToken pending_token = start;
  int token_id;
  int depth = 1;
  int key_state = CREATE_TABLE_KEY_NEED_PART;

  while ((token_id = mylite_lexer_next(lexer, &token)) > 0) {
    if (depth > 1) {
      if (token_opens_nested_expression(token_id)) {
        depth++;
      } else if (token_closes_nested_expression(token_id)) {
        depth--;
        if (depth == 1 && key_state == CREATE_TABLE_KEY_IN_FUNCTION) {
          key_state = CREATE_TABLE_KEY_AFTER_PART;
        }
      }
      continue;
    }

    if (key_state == CREATE_TABLE_KEY_PREFIX_VALUE) {
      if (!create_index_prefix_length_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE index key part");
        return 0;
      }
      key_state = CREATE_TABLE_KEY_PREFIX_AFTER_VALUE;
      continue;
    }

    if (key_state == CREATE_TABLE_KEY_PREFIX_AFTER_VALUE) {
      if (token_id != ML_RP) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE index key part");
        return 0;
      }
      key_state = CREATE_TABLE_KEY_AFTER_PART;
      continue;
    }

    if (token_id == ML_LP) {
      if (key_state == CREATE_TABLE_KEY_NEED_PART) {
        key_state = CREATE_TABLE_KEY_IN_FUNCTION;
        depth = 2;
        pending_token = token;
        continue;
      }
      if (key_state == CREATE_TABLE_KEY_AFTER_NAME) {
        key_state = CREATE_TABLE_KEY_PREFIX_VALUE;
        pending_token = token;
        continue;
      }
      mylite_parser_reject(ctx, token,
                           "malformed CREATE TABLE index key part");
      return 0;
    }

    if (token_id == ML_RP) {
      if (key_state == CREATE_TABLE_KEY_NEED_PART ||
          key_state == CREATE_TABLE_KEY_AFTER_DOT ||
          key_state == CREATE_TABLE_KEY_IN_FUNCTION) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE index key part");
        return 0;
      }
      return 1;
    }

    if (token_id == ML_COMMA) {
      if (key_state != CREATE_TABLE_KEY_AFTER_NAME &&
          key_state != CREATE_TABLE_KEY_AFTER_PART &&
          key_state != CREATE_TABLE_KEY_AFTER_DIRECTION) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE index key part");
        return 0;
      }
      key_state = CREATE_TABLE_KEY_NEED_PART;
      pending_token = token;
      continue;
    }

    if (token_id == ML_ASC || token_id == ML_DESC) {
      if (key_state != CREATE_TABLE_KEY_AFTER_NAME &&
          key_state != CREATE_TABLE_KEY_AFTER_PART) {
        mylite_parser_reject(ctx, token,
                             "malformed CREATE TABLE index key part");
        return 0;
      }
      key_state = CREATE_TABLE_KEY_AFTER_DIRECTION;
      continue;
    }

    if (token_id == ML_DOT) {
      if (key_state != CREATE_TABLE_KEY_AFTER_NAME) {
        mylite_parser_reject(ctx, token,
                             "malformed CREATE TABLE index key part");
        return 0;
      }
      key_state = CREATE_TABLE_KEY_AFTER_DOT;
      pending_token = token;
      continue;
    }

    if (key_state == CREATE_TABLE_KEY_NEED_PART ||
        key_state == CREATE_TABLE_KEY_AFTER_DOT) {
      if (!dml_row_alias_token(token_id)) {
        mylite_parser_reject(ctx, pending_token,
                             "incomplete CREATE TABLE index key part");
        return 0;
      }
      key_state = CREATE_TABLE_KEY_AFTER_NAME;
      continue;
    }

    mylite_parser_reject(ctx, token, "malformed CREATE TABLE index key part");
    return 0;
  }

  mylite_parser_reject(ctx, pending_token,
                       "incomplete CREATE TABLE index key part");
  return 0;
}

static int create_index_prefix_length_token(int token_id) {
  return token_id == ML_BOOLEAN_NUMBER || token_id == ML_FACTOR_NUMBER ||
         token_id == ML_NUMBER_LITERAL;
}

static int alter_table_add_index_marker(int token_id) {
  return token_id == ML_FULLTEXT || token_id == ML_INDEX ||
         token_id == ML_KEY || token_id == ML_PRIMARY ||
         token_id == ML_SPATIAL || token_id == ML_UNIQUE;
}

static int alter_table_add_non_index_marker(int token_id) {
  return token_id == ML_CHECK || token_id == ML_COLUMN ||
         token_id == ML_FOREIGN || token_id == ML_PARTITION;
}

static int event_interval_unit_token(MyliteToken token) {
  return token_ascii_equal(token, "year") ||
         token_ascii_equal(token, "quarter") ||
         token_ascii_equal(token, "month") ||
         token_ascii_equal(token, "week") ||
         token_ascii_equal(token, "day") ||
         token_ascii_equal(token, "hour") ||
         token_ascii_equal(token, "minute") ||
         token_ascii_equal(token, "second") ||
         token_ascii_equal(token, "microsecond") ||
         token_ascii_equal(token, "year_month") ||
         token_ascii_equal(token, "day_hour") ||
         token_ascii_equal(token, "day_minute") ||
         token_ascii_equal(token, "day_second") ||
         token_ascii_equal(token, "hour_minute") ||
         token_ascii_equal(token, "hour_second") ||
         token_ascii_equal(token, "minute_second") ||
         token_ascii_equal(token, "second_microsecond") ||
         token_ascii_equal(token, "minute_microsecond") ||
         token_ascii_equal(token, "hour_microsecond") ||
         token_ascii_equal(token, "day_microsecond");
}

static int event_schedule_boundary(int token_id) {
  return token_id == ML_COMMENT || token_id == ML_DISABLE ||
         token_id == ML_DO || token_id == ML_ENABLE || token_id == ML_ON ||
         token_id == ML_RENAME || token_id == ML_SEMI;
}

static int event_schedule_option_start(MyliteToken token) {
  return token_ascii_equal(token, "starts") || token_ascii_equal(token, "ends");
}

static int token_is_plus(MyliteToken token) {
  return token.length == 1 && token.start[0] == '+';
}

static int dml_assignment_boundary(int mode, int token_id) {
  if (mode == DML_ASSIGNMENT_UPDATE) {
    return token_id == ML_LIMIT || token_id == ML_ORDER || token_id == ML_WHERE;
  }
  if (mode == DML_ASSIGNMENT_INSERT_SET) {
    return token_id == ML_AS || token_id == ML_ON;
  }
  if (mode == DML_ASSIGNMENT_REPLACE_SET) {
    return token_id == ML_AS;
  }

  return 0;
}

static int dml_assignment_operator(int token_id) {
  return token_id == ML_ASSIGN || token_id == ML_EQUALS;
}

static int dml_assignment_target_token(int token_id) {
  return token_id != ML_ASSIGN && token_id != ML_COMMA &&
         token_id != ML_DOT && token_id != ML_DUPLICATE &&
         token_id != ML_EQUALS && token_id != ML_KEY &&
         token_id != ML_LB && token_id != ML_LC && token_id != ML_LIMIT &&
         token_id != ML_LP && token_id != ML_ON && token_id != ML_ORDER &&
         token_id != ML_RB && token_id != ML_RC && token_id != ML_RP &&
         token_id != ML_SEMI && token_id != ML_UPDATE &&
         token_id != ML_WHERE;
}

static int dml_clause_operand_boundary(int token_id) {
  return token_id == ML_LIMIT || token_id == ML_ORDER ||
         token_id == ML_SEMI || token_id == ML_WHERE;
}

static int dml_limit_option_token(int token_id) {
  return token_id == ML_ATOM || token_id == ML_BOOLEAN_NUMBER ||
         token_id == ML_FACTOR_NUMBER || token_id == ML_NUMBER_LITERAL ||
         token_id == ML_QUOTED_ID;
}

static int dml_row_alias_token(int token_id) {
  return token_id != ML_ASSIGN && token_id != ML_BOOLEAN_NUMBER &&
         token_id != ML_COMMA && token_id != ML_DOT &&
         token_id != ML_DOUBLE_QUOTED_STRING && token_id != ML_EQUALS &&
         token_id != ML_FACTOR_NUMBER && token_id != ML_LB &&
         token_id != ML_LC && token_id != ML_LP &&
         token_id != ML_NUMBER_LITERAL && token_id != ML_RB &&
         token_id != ML_RC && token_id != ML_RP && token_id != ML_SEMI &&
         token_id != ML_STRING_LITERAL;
}

static int dml_values_unclosed_string_fragment(int token_id,
                                               MyliteToken token) {
  char quote;
  size_t i;

  if (token_id != ML_DOUBLE_QUOTED_STRING && token_id != ML_STRING_LITERAL) {
    return 0;
  }
  if (token.length == 0) {
    return 0;
  }

  quote = token.start[0];
  if (quote != '\'' && quote != '"') {
    if (token.length < 2 || token.start[1] != '\'') {
      return 0;
    }
    quote = '\'';
    i = 2;
  } else {
    i = 1;
  }

  while (i < token.length) {
    char ch = token.start[i++];
    if (ch == '\\' && i < token.length) {
      i++;
      continue;
    }
    if (ch == quote) {
      if (i < token.length && token.start[i] == quote) {
        i++;
        continue;
      }
      return 0;
    }
  }

  return 1;
}

static int token_opens_nested_expression(int token_id) {
  return token_id == ML_LP || token_id == ML_LB || token_id == ML_LC;
}

static int token_closes_nested_expression(int token_id) {
  return token_id == ML_RP || token_id == ML_RB || token_id == ML_RC;
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
