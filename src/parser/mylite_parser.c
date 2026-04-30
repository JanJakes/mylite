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
static int token_ascii_matches_any(const MyliteToken *token,
                                   const char *const *texts,
                                   size_t text_count);
static int token_ascii_starts_with(const MyliteToken *token,
                                   const char *prefix);
static int token_ascii_equals(const MyliteToken *token, const char *text);

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

void mylite_parser_require_token_text(MyliteParseContext *ctx,
                                      MyliteToken token,
                                      const char *text) {
  if (ctx->failed || token_ascii_equals(&token, text)) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_token_text_any(MyliteParseContext *ctx,
                                          MyliteToken token,
                                          const char *first,
                                          const char *second) {
  if (ctx->failed || token_ascii_equals(&token, first) ||
      token_ascii_equals(&token, second)) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_create_view_algorithm(MyliteParseContext *ctx,
                                                 MyliteToken token) {
  static const char *const algorithms[] = {
      "MERGE",
      "TEMPTABLE",
      "UNDEFINED",
  };

  if (ctx->failed ||
      token_ascii_matches_any(&token, algorithms,
                              sizeof(algorithms) / sizeof(algorithms[0]))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_srs_attribute(MyliteParseContext *ctx,
                                         MyliteToken token) {
  static const char *const attributes[] = {
      "DEFINITION",
      "DESCRIPTION",
      "NAME",
      "ORGANIZATION",
  };

  if (ctx->failed ||
      token_ascii_matches_any(&token, attributes,
                              sizeof(attributes) / sizeof(attributes[0]))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_resource_group_action(MyliteParseContext *ctx,
                                                 MyliteToken token) {
  static const char *const actions[] = {
      "DISABLE",
      "ENABLE",
      "FORCE",
      "THREAD_PRIORITY",
      "VCPU",
  };

  if (ctx->failed ||
      token_ascii_matches_any(&token, actions,
                              sizeof(actions) / sizeof(actions[0]))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_udf_return_type(MyliteParseContext *ctx,
                                           MyliteToken token) {
  static const char *const types[] = {
      "DECIMAL",
      "INT",
      "INTEGER",
      "REAL",
      "STRING",
  };

  if (ctx->failed ||
      token_ascii_matches_any(&token, types, sizeof(types) / sizeof(types[0]))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_event_atom_action(MyliteParseContext *ctx,
                                             MyliteToken token) {
  static const char *const actions[] = {
      "COMMENT",
      "DISABLE",
      "ENABLE",
  };

  if (ctx->failed ||
      token_ascii_matches_any(&token, actions,
                              sizeof(actions) / sizeof(actions[0]))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_create_database_option_start(MyliteParseContext *ctx,
                                                        MyliteToken token) {
  static const char *const starters[] = {
      "CHARACTER",
      "CHARSET",
      "COLLATE",
      "DEFAULT",
      "ENCRYPTION",
      "READ",
  };

  if (ctx->failed ||
      token_ascii_matches_any(&token, starters,
                              sizeof(starters) / sizeof(starters[0]))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_create_table_tail_atom(MyliteParseContext *ctx,
                                                  MyliteToken token) {
  static const char *const starters[] = {
      "AS",
      "AUTO_INCREMENT",
      "AVG_ROW_LENGTH",
      "CHARACTER",
      "CHARSET",
      "CHECKSUM",
      "COLLATE",
      "COMMENT",
      "COMPRESSION",
      "CONNECTION",
      "DATA",
      "DELAY_KEY_WRITE",
      "ENCRYPTION",
      "ENGINE",
      "ENGINE_ATTRIBUTE",
      "INDEX",
      "INSERT_METHOD",
      "KEY_BLOCK_SIZE",
      "LIKE",
      "MAX_ROWS",
      "MIN_ROWS",
      "PACK_KEYS",
      "PASSWORD",
      "PARTITION",
      "ROW_FORMAT",
      "SECONDARY_ENGINE_ATTRIBUTE",
      "STATS_AUTO_RECALC",
      "STATS_PERSISTENT",
      "STATS_SAMPLE_PAGES",
      "STORAGE",
      "TABLESPACE",
      "UNION",
  };

  if (ctx->failed ||
      token_ascii_matches_any(&token, starters,
                              sizeof(starters) / sizeof(starters[0]))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_create_tablespace_tail_atom(MyliteParseContext *ctx,
                                                       MyliteToken token) {
  static const char *const starters[] = {
      "ADD",
      "ENCRYPTION",
      "ENGINE_ATTRIBUTE",
      "FILE_BLOCK_SIZE",
  };

  if (ctx->failed ||
      token_ascii_matches_any(&token, starters,
                              sizeof(starters) / sizeof(starters[0]))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_alter_table_action_start(MyliteParseContext *ctx,
                                                    MyliteToken token) {
  static const char *const starters[] = {
      "ADD",
      "ALGORITHM",
      "ANALYZE",
      "AUTO_INCREMENT",
      "AVG_ROW_LENGTH",
      "CHANGE",
      "CHECK",
      "CHECKSUM",
      "COALESCE",
      "COMMENT",
      "CONVERT",
      "DISABLE",
      "DISCARD",
      "ENABLE",
      "ENCRYPTION",
      "EXCHANGE",
      "FORCE",
      "IMPORT",
      "INSERT_METHOD",
      "KEY_BLOCK_SIZE",
      "LOCK",
      "MAX_ROWS",
      "MIN_ROWS",
      "MODIFY",
      "OPTIMIZE",
      "ORDER",
      "PACK_KEYS",
      "PARTITION",
      "REBUILD",
      "REMOVE",
      "REORGANIZE",
      "REPAIR",
      "ROW_FORMAT",
      "SECONDARY_ENGINE",
      "STORAGE",
      "TRUNCATE",
  };

  if (ctx->failed ||
      token_ascii_matches_any(&token, starters,
                              sizeof(starters) / sizeof(starters[0]))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_dml_write_atom_start(MyliteParseContext *ctx,
                                                MyliteToken token) {
  static const char *const starters[] = {
      "PARTITION",
      "VALUE",
  };

  if (ctx->failed ||
      token_ascii_matches_any(&token, starters,
                              sizeof(starters) / sizeof(starters[0]))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_token_prefix(MyliteParseContext *ctx,
                                        MyliteToken token,
                                        const char *prefix) {
  if (ctx->failed || token_ascii_starts_with(&token, prefix)) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_profile_type(MyliteParseContext *ctx,
                                        MyliteToken first,
                                        MyliteToken second) {
  static const char *const single_word_types[] = {
      "CPU",
      "IPC",
      "MEMORY",
      "SOURCE",
      "SWAPS",
  };

  if (ctx->failed) {
    return;
  }

  if (second.length == 0) {
    if (token_ascii_matches_any(
            &first, single_word_types,
            sizeof(single_word_types) / sizeof(single_word_types[0]))) {
      return;
    }
    ctx->failed = 1;
    format_near_token(ctx, 0, &first);
    return;
  }

  if ((token_ascii_equals(&first, "BLOCK") &&
       token_ascii_equals(&second, "IO")) ||
      (token_ascii_equals(&first, "CONTEXT") &&
       token_ascii_equals(&second, "SWITCHES")) ||
      (token_ascii_equals(&first, "PAGE") &&
       token_ascii_equals(&second, "FAULTS"))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &first);
}

void mylite_parser_require_start_until_log_pair(MyliteParseContext *ctx,
                                                MyliteToken file,
                                                MyliteToken pos) {
  if (ctx->failed ||
      (token_ascii_equals(&file, "SOURCE_LOG_FILE") &&
       token_ascii_equals(&pos, "SOURCE_LOG_POS")) ||
      (token_ascii_equals(&file, "RELAY_LOG_FILE") &&
       token_ascii_equals(&pos, "RELAY_LOG_POS"))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &file);
}

void mylite_parser_require_check_table_option(MyliteParseContext *ctx,
                                              MyliteToken token) {
  static const char *const options[] = {
      "CHANGED",
      "FAST",
      "MEDIUM",
  };

  if (ctx->failed ||
      token_ascii_matches_any(&token, options,
                              sizeof(options) / sizeof(options[0]))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_explain_format(MyliteParseContext *ctx,
                                          MyliteToken token) {
  static const char *const formats[] = {
      "JSON",
      "TRADITIONAL",
      "TREE",
  };

  if (ctx->failed ||
      token_ascii_matches_any(&token, formats,
                              sizeof(formats) / sizeof(formats[0]))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_diagnostics_statement_item(MyliteParseContext *ctx,
                                                      MyliteToken token) {
  static const char *const items[] = {
      "NUMBER",
      "ROW_COUNT",
  };

  if (ctx->failed || token_ascii_matches_any(&token, items,
                                             sizeof(items) / sizeof(items[0]))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_diagnostics_condition_item(MyliteParseContext *ctx,
                                                      MyliteToken token) {
  static const char *const items[] = {
      "CATALOG_NAME",
      "CLASS_ORIGIN",
      "COLUMN_NAME",
      "CONSTRAINT_CATALOG",
      "CONSTRAINT_NAME",
      "CONSTRAINT_SCHEMA",
      "CURSOR_NAME",
      "MESSAGE_TEXT",
      "MYSQL_ERRNO",
      "RETURNED_SQLSTATE",
      "SCHEMA_NAME",
      "SUBCLASS_ORIGIN",
      "TABLE_NAME",
  };

  if (ctx->failed || token_ascii_matches_any(&token, items,
                                             sizeof(items) / sizeof(items[0]))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
}

void mylite_parser_require_signal_condition_item(MyliteParseContext *ctx,
                                                 MyliteToken token) {
  static const char *const items[] = {
      "CATALOG_NAME",
      "CLASS_ORIGIN",
      "COLUMN_NAME",
      "CONSTRAINT_CATALOG",
      "CONSTRAINT_NAME",
      "CONSTRAINT_SCHEMA",
      "CURSOR_NAME",
      "MESSAGE_TEXT",
      "MYSQL_ERRNO",
      "SCHEMA_NAME",
      "SUBCLASS_ORIGIN",
      "TABLE_NAME",
  };

  if (ctx->failed || token_ascii_matches_any(&token, items,
                                             sizeof(items) / sizeof(items[0]))) {
    return;
  }

  ctx->failed = 1;
  format_near_token(ctx, 0, &token);
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

static int token_ascii_matches_any(const MyliteToken *token,
                                   const char *const *texts,
                                   size_t text_count) {
  size_t i;

  for (i = 0; i < text_count; i++) {
    if (token_ascii_equals(token, texts[i])) {
      return 1;
    }
  }

  return 0;
}

static int token_ascii_starts_with(const MyliteToken *token,
                                   const char *prefix) {
  size_t i;

  for (i = 0; i < token->length && prefix[i] != '\0'; i++) {
    unsigned char a = (unsigned char) token->start[i];
    unsigned char b = (unsigned char) prefix[i];

    if (a >= 'a' && a <= 'z') {
      a = (unsigned char) (a - 'a' + 'A');
    }
    if (b >= 'a' && b <= 'z') {
      b = (unsigned char) (b - 'a' + 'A');
    }
    if (a != b) {
      return 0;
    }
  }

  return prefix[i] == '\0';
}

static int token_ascii_equals(const MyliteToken *token, const char *text) {
  size_t i;

  for (i = 0; i < token->length && text[i] != '\0'; i++) {
    unsigned char a = (unsigned char) token->start[i];
    unsigned char b = (unsigned char) text[i];

    if (a >= 'a' && a <= 'z') {
      a = (unsigned char) (a - 'a' + 'A');
    }
    if (b >= 'a' && b <= 'z') {
      b = (unsigned char) (b - 'a' + 'A');
    }
    if (a != b) {
      return 0;
    }
  }

  return i == token->length && text[i] == '\0';
}
