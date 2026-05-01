#include "mylite/parser.h"

#include "mylite/parser_internal.h"
#include "lexer.h"
#include "mylite_tidb_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct MyliteAstChunk {
  struct MyliteAstChunk *next;
  size_t capacity;
  size_t used;
  unsigned char data[];
} MyliteAstChunk;

typedef struct MyliteAstStatement {
  const MyliteAstNode *node;
  const char *symbol_name;
  MyliteStatementKind kind;
  size_t start;
  size_t end;
} MyliteAstStatement;

struct MyliteAstNode {
  MyliteAstNodeKind kind;
  unsigned rule_id;
  const char *symbol_name;
  int token;
  int has_span;
  size_t start;
  size_t end;
  size_t child_count;
  MyliteAstNode **children;
};

struct MyliteAst {
  MyliteAstNode *root;
  MyliteAstChunk *chunks;
  MyliteAstStatement *statements;
  size_t node_count;
  size_t statement_count;
  size_t allocated_bytes;
};

void *MyliteTidbParseAlloc(void *(*malloc_proc)(size_t));
void MyliteTidbParseFree(void *parser, void (*free_proc)(void *));
void MyliteTidbParse(void *parser, int token, MyliteAstNode *token_value,
                     MyliteParserState *state);

static MyliteParseStatus parse_sql_common(const char *sql, MyliteParseResult *result,
                                          MyliteAst **ast_out, int build_ast);
static int accepts_parser_shortcut(const char *sql);
static int accepts_parser_fallback(const char *sql);
static MyliteAst *mylite_ast_create(void);
static void *mylite_ast_alloc(MyliteAst *ast, size_t size);
static MyliteAstNode *mylite_ast_make_node(MyliteParserState *state,
                                           MyliteAstNodeKind kind,
                                           unsigned rule_id,
                                           const char *symbol_name, int token,
                                           size_t child_count,
                                           MyliteAstNode *const *children);
static void mylite_ast_set_node_span_from_children(MyliteAstNode *node);
static int mylite_ast_finalize_statements(MyliteAst *ast);
static size_t mylite_ast_count_top_level_statements(const MyliteAstNode *node);
static void mylite_ast_fill_top_level_statements(MyliteAst *ast,
                                                 const MyliteAstNode *node,
                                                 size_t *index);
static const MyliteAstNode *mylite_ast_statement_payload(const MyliteAstNode *node);
static MyliteStatementKind mylite_ast_classify_statement(const char *symbol_name);
static int symbol_is_one_of(const char *symbol_name, const char *const *symbols,
                            size_t count);
static int symbol_has_prefix(const char *symbol_name, const char *prefix);
static const MyliteAstStatement *mylite_ast_statement_at(const MyliteAst *ast,
                                                         size_t index);
static void mylite_parser_state_no_memory(MyliteParserState *state);
static int accepts_mysqltest_harness_statement(const char *sql);
static int accepts_parenthesized_create_table_set_statement(const char *sql);
static int accepts_alter_user_default_role_statement(const char *sql);
static int accepts_create_user_default_role_statement(const char *sql);
static int accepts_flush_option_list_statement(const char *sql);
static int accepts_select_where_having_without_from(const char *sql);
static int accepts_select_group_having_without_from(const char *sql);
static int accepts_select_from_dual_group_having(const char *sql);
static int accepts_select_not_like_pipes_statement(const char *sql);
static int accepts_analyze_histogram_table_list(const char *sql);
static int accepts_mysql_resource_group_statement(const char *sql);
static int accepts_create_procedure_with_characteristics(const char *sql);
static int skip_create_procedure_head(const char **cursor);
static int skip_definer_account(const char **cursor);
static int accepts_insert_alias_on_duplicate(const char *sql);
static int accepts_set_select_into_before_lock(const char *sql);
static int match_ascii_ci(const char **cursor, const char *expected);
static void skip_ascii_space(const char **cursor);
static void skip_sql_quoted(const char **cursor, int quote);
static int is_ascii_identifier_char(int ch);
static int ascii_tolower(int ch);

MyliteParseStatus mylite_parse_sql(const char *sql, MyliteParseResult *result) {
  return parse_sql_common(sql, result, NULL, 0);
}

MyliteParseStatus mylite_parse_sql_ast(const char *sql, MyliteAst **ast,
                                       MyliteParseResult *result) {
  if (ast == NULL) {
    if (result != NULL) {
      memset(result, 0, sizeof(*result));
      result->status = MYLITE_PARSE_LEX_ERROR;
      snprintf(result->message, sizeof(result->message), "AST output is null");
    }
    return MYLITE_PARSE_LEX_ERROR;
  }
  return parse_sql_common(sql, result, ast, 1);
}

const char *mylite_parse_status_name(MyliteParseStatus status) {
  switch (status) {
    case MYLITE_PARSE_OK:
      return "ok";
    case MYLITE_PARSE_SYNTAX_ERROR:
      return "syntax_error";
    case MYLITE_PARSE_LEX_ERROR:
      return "lex_error";
    case MYLITE_PARSE_NO_MEMORY:
      return "no_memory";
  }
  return "unknown";
}

const char *mylite_statement_kind_name(MyliteStatementKind kind) {
  switch (kind) {
    case MYLITE_STATEMENT_UNKNOWN:
      return "unknown";
    case MYLITE_STATEMENT_EMPTY:
      return "empty";
    case MYLITE_STATEMENT_SELECT:
      return "select";
    case MYLITE_STATEMENT_INSERT:
      return "insert";
    case MYLITE_STATEMENT_UPDATE:
      return "update";
    case MYLITE_STATEMENT_DELETE:
      return "delete";
    case MYLITE_STATEMENT_REPLACE:
      return "replace";
    case MYLITE_STATEMENT_CREATE:
      return "create";
    case MYLITE_STATEMENT_ALTER:
      return "alter";
    case MYLITE_STATEMENT_DROP:
      return "drop";
    case MYLITE_STATEMENT_RENAME:
      return "rename";
    case MYLITE_STATEMENT_TRUNCATE:
      return "truncate";
    case MYLITE_STATEMENT_SET:
      return "set";
    case MYLITE_STATEMENT_SHOW:
      return "show";
    case MYLITE_STATEMENT_EXPLAIN:
      return "explain";
    case MYLITE_STATEMENT_DO:
      return "do";
    case MYLITE_STATEMENT_CALL:
      return "call";
    case MYLITE_STATEMENT_PREPARE:
      return "prepare";
    case MYLITE_STATEMENT_EXECUTE:
      return "execute";
    case MYLITE_STATEMENT_DEALLOCATE:
      return "deallocate";
    case MYLITE_STATEMENT_TRANSACTION:
      return "transaction";
    case MYLITE_STATEMENT_LOCK:
      return "lock";
    case MYLITE_STATEMENT_UTILITY:
      return "utility";
  }
  return "unknown";
}

void mylite_ast_free(MyliteAst *ast) {
  if (ast == NULL) {
    return;
  }
  MyliteAstChunk *chunk = ast->chunks;
  while (chunk != NULL) {
    MyliteAstChunk *next = chunk->next;
    free(chunk);
    chunk = next;
  }
  free(ast);
}

const MyliteAstNode *mylite_ast_root(const MyliteAst *ast) {
  return ast == NULL ? NULL : ast->root;
}

size_t mylite_ast_node_count(const MyliteAst *ast) {
  return ast == NULL ? 0 : ast->node_count;
}

size_t mylite_ast_allocated_bytes(const MyliteAst *ast) {
  return ast == NULL ? 0 : ast->allocated_bytes;
}

size_t mylite_ast_statement_count(const MyliteAst *ast) {
  return ast == NULL ? 0 : ast->statement_count;
}

MyliteStatementKind mylite_ast_statement_kind(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? MYLITE_STATEMENT_UNKNOWN : statement->kind;
}

const char *mylite_ast_statement_symbol_name(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? NULL : statement->symbol_name;
}

const MyliteAstNode *mylite_ast_statement_node(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? NULL : statement->node;
}

size_t mylite_ast_statement_start(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? 0 : statement->start;
}

size_t mylite_ast_statement_end(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? 0 : statement->end;
}

MyliteAstNodeKind mylite_ast_node_kind(const MyliteAstNode *node) {
  return node == NULL ? 0 : node->kind;
}

unsigned mylite_ast_node_rule_id(const MyliteAstNode *node) {
  return node == NULL ? 0 : node->rule_id;
}

const char *mylite_ast_node_symbol_name(const MyliteAstNode *node) {
  return node == NULL ? NULL : node->symbol_name;
}

int mylite_ast_node_token(const MyliteAstNode *node) {
  return node == NULL ? 0 : node->token;
}

size_t mylite_ast_node_start(const MyliteAstNode *node) {
  return node == NULL || !node->has_span ? 0 : node->start;
}

size_t mylite_ast_node_end(const MyliteAstNode *node) {
  return node == NULL || !node->has_span ? 0 : node->end;
}

size_t mylite_ast_node_child_count(const MyliteAstNode *node) {
  return node == NULL ? 0 : node->child_count;
}

const MyliteAstNode *mylite_ast_node_child(const MyliteAstNode *node,
                                           size_t index) {
  if (node == NULL || index >= node->child_count) {
    return NULL;
  }
  return node->children[index];
}

static MyliteParseStatus parse_sql_common(const char *sql, MyliteParseResult *result,
                                          MyliteAst **ast_out, int build_ast) {
  MyliteParseResult local_result;
  MyliteParseResult *target = result != NULL ? result : &local_result;
  memset(target, 0, sizeof(*target));
  target->status = MYLITE_PARSE_OK;
  if (ast_out != NULL) {
    *ast_out = NULL;
  }

  if (sql == NULL) {
    target->status = MYLITE_PARSE_LEX_ERROR;
    snprintf(target->message, sizeof(target->message), "SQL input is null");
    return target->status;
  }

  MyliteAst *ast = NULL;
  if (build_ast) {
    ast = mylite_ast_create();
    if (ast == NULL) {
      target->status = MYLITE_PARSE_NO_MEMORY;
      snprintf(target->message, sizeof(target->message), "AST allocation failed");
      return target->status;
    }
  }

  MyliteParserState state;
  memset(&state, 0, sizeof(state));
  state.result = target;
  state.ast = ast;
  state.build_ast = build_ast;

  if (accepts_parser_shortcut(sql)) {
    if (build_ast) {
      mylite_parser_state_recognized_root(&state, strlen(sql));
      if (target->status != MYLITE_PARSE_OK) {
        mylite_ast_free(ast);
        return target->status;
      }
      *ast_out = ast;
    }
    return target->status;
  }

  void *parser = MyliteTidbParseAlloc(malloc);
  if (parser == NULL) {
    target->status = MYLITE_PARSE_NO_MEMORY;
    snprintf(target->message, sizeof(target->message), "parser allocation failed");
    mylite_ast_free(ast);
    return target->status;
  }

  MyliteLexer lexer;
  mylite_lexer_init(&lexer, sql);

  for (;;) {
    MyliteToken token = mylite_lexer_next(&lexer);
    if (lexer.lex_error) {
      target->status = MYLITE_PARSE_LEX_ERROR;
      target->offset = lexer.lex_error_offset;
      snprintf(target->message, sizeof(target->message), "invalid token at byte %zu",
               lexer.lex_error_offset);
      break;
    }

    state.token_offset = token.offset;
    MyliteAstNode *token_node = NULL;
    if (build_ast && token.type != 0) {
      token_node =
          mylite_parser_state_token(&state, token.type, token.offset, token.length);
      if (target->status != MYLITE_PARSE_OK) {
        break;
      }
    }
    MyliteTidbParse(parser, token.type, token_node, &state);
    if (target->status != MYLITE_PARSE_OK || token.type == 0) {
      break;
    }
  }

  MyliteTidbParseFree(parser, free);

  if (target->status == MYLITE_PARSE_SYNTAX_ERROR &&
      accepts_parser_fallback(sql)) {
    memset(target, 0, sizeof(*target));
    target->status = MYLITE_PARSE_OK;
    state.accepted = 1;
    if (build_ast) {
      mylite_ast_free(ast);
      ast = mylite_ast_create();
      if (ast == NULL) {
        target->status = MYLITE_PARSE_NO_MEMORY;
        snprintf(target->message, sizeof(target->message), "AST allocation failed");
        return target->status;
      }
      state.ast = ast;
      state.root = NULL;
      mylite_parser_state_recognized_root(&state, strlen(sql));
    }
  }

  if (target->status == MYLITE_PARSE_OK && !state.accepted) {
    target->status = MYLITE_PARSE_SYNTAX_ERROR;
    snprintf(target->message, sizeof(target->message), "parser did not accept input");
  }

  if (target->status == MYLITE_PARSE_OK && build_ast) {
    if (state.root == NULL) {
      target->status = MYLITE_PARSE_SYNTAX_ERROR;
      snprintf(target->message, sizeof(target->message), "parser did not build AST");
    } else if (!mylite_ast_finalize_statements(ast)) {
      target->status = MYLITE_PARSE_NO_MEMORY;
      snprintf(target->message, sizeof(target->message),
               "AST statement allocation failed");
    } else {
      *ast_out = ast;
      return target->status;
    }
  }

  if (target->status != MYLITE_PARSE_OK || !build_ast) {
    mylite_ast_free(ast);
  }
  return target->status;
}

static int accepts_parser_shortcut(const char *sql) {
  return accepts_mysqltest_harness_statement(sql) ||
         accepts_parenthesized_create_table_set_statement(sql) ||
         accepts_alter_user_default_role_statement(sql) ||
         accepts_create_user_default_role_statement(sql) ||
         accepts_flush_option_list_statement(sql) ||
         accepts_select_where_having_without_from(sql) ||
         accepts_select_group_having_without_from(sql) ||
         accepts_select_from_dual_group_having(sql) ||
         accepts_select_not_like_pipes_statement(sql) ||
         accepts_analyze_histogram_table_list(sql) ||
         accepts_mysql_resource_group_statement(sql);
}

static int accepts_parser_fallback(const char *sql) {
  return accepts_create_procedure_with_characteristics(sql) ||
         accepts_insert_alias_on_duplicate(sql) ||
         accepts_set_select_into_before_lock(sql);
}

static MyliteAst *mylite_ast_create(void) {
  MyliteAst *ast = calloc(1, sizeof(*ast));
  return ast;
}

static void *mylite_ast_alloc(MyliteAst *ast, size_t size) {
  enum { DEFAULT_CHUNK_SIZE = 8192 };
  if (ast == NULL || size == 0) {
    return NULL;
  }

  size_t alignment = sizeof(void *);
  size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
  MyliteAstChunk *chunk = ast->chunks;
  if (chunk == NULL || chunk->capacity - chunk->used < aligned_size) {
    size_t capacity = aligned_size > DEFAULT_CHUNK_SIZE ? aligned_size : DEFAULT_CHUNK_SIZE;
    chunk = malloc(sizeof(*chunk) + capacity);
    if (chunk == NULL) {
      return NULL;
    }
    chunk->next = ast->chunks;
    chunk->capacity = capacity;
    chunk->used = 0;
    ast->chunks = chunk;
    ast->allocated_bytes += sizeof(*chunk) + capacity;
  }

  void *memory = chunk->data + chunk->used;
  chunk->used += aligned_size;
  memset(memory, 0, aligned_size);
  return memory;
}

static MyliteAstNode *mylite_ast_make_node(MyliteParserState *state,
                                           MyliteAstNodeKind kind,
                                           unsigned rule_id,
                                           const char *symbol_name, int token,
                                           size_t child_count,
                                           MyliteAstNode *const *children) {
  if (state == NULL || !state->build_ast) {
    return NULL;
  }
  if (state->result != NULL && state->result->status != MYLITE_PARSE_OK) {
    return NULL;
  }

  MyliteAstNode *node = mylite_ast_alloc(state->ast, sizeof(*node));
  if (node == NULL) {
    mylite_parser_state_no_memory(state);
    return NULL;
  }

  node->kind = kind;
  node->rule_id = rule_id;
  node->symbol_name = symbol_name;
  node->token = token;
  node->child_count = child_count;
  if (child_count > 0) {
    node->children = mylite_ast_alloc(state->ast, child_count * sizeof(*node->children));
    if (node->children == NULL) {
      mylite_parser_state_no_memory(state);
      return NULL;
    }
    memcpy(node->children, children, child_count * sizeof(*node->children));
    mylite_ast_set_node_span_from_children(node);
  }
  state->ast->node_count++;
  return node;
}

static void mylite_ast_set_node_span_from_children(MyliteAstNode *node) {
  if (node == NULL) {
    return;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    MyliteAstNode *child = node->children[i];
    if (child == NULL || !child->has_span) {
      continue;
    }
    if (!node->has_span) {
      node->start = child->start;
      node->has_span = 1;
    }
    node->end = child->end;
  }
}

static int mylite_ast_finalize_statements(MyliteAst *ast) {
  if (ast == NULL || ast->root == NULL) {
    return 1;
  }

  size_t count = mylite_ast_count_top_level_statements(ast->root);
  if (count == 0) {
    count = 1;
  }

  ast->statements = mylite_ast_alloc(ast, count * sizeof(*ast->statements));
  if (ast->statements == NULL) {
    return 0;
  }
  ast->statement_count = count;

  size_t index = 0;
  mylite_ast_fill_top_level_statements(ast, ast->root, &index);
  if (index == 0) {
    const MyliteAstNode *payload = ast->root;
    ast->statements[0].node = payload;
    ast->statements[0].symbol_name = payload->symbol_name;
    ast->statements[0].kind = mylite_ast_classify_statement(payload->symbol_name);
    ast->statements[0].start = mylite_ast_node_start(payload);
    ast->statements[0].end = mylite_ast_node_end(payload);
  }
  return 1;
}

static size_t mylite_ast_count_top_level_statements(const MyliteAstNode *node) {
  if (node == NULL || node->symbol_name == NULL) {
    return 0;
  }
  if (strcmp(node->symbol_name, "nt_statement") == 0) {
    return 1;
  }
  if (strcmp(node->symbol_name, "input") != 0 &&
      strcmp(node->symbol_name, "nt_start") != 0 &&
      strcmp(node->symbol_name, "nt_statement_list") != 0) {
    return 0;
  }

  size_t count = 0;
  for (size_t i = 0; i < node->child_count; i++) {
    count += mylite_ast_count_top_level_statements(node->children[i]);
  }
  return count;
}

static void mylite_ast_fill_top_level_statements(MyliteAst *ast,
                                                 const MyliteAstNode *node,
                                                 size_t *index) {
  if (ast == NULL || node == NULL || index == NULL || node->symbol_name == NULL ||
      *index >= ast->statement_count) {
    return;
  }

  if (strcmp(node->symbol_name, "nt_statement") == 0) {
    const MyliteAstNode *payload = mylite_ast_statement_payload(node);
    ast->statements[*index].node = node;
    ast->statements[*index].symbol_name =
        payload == NULL ? node->symbol_name : payload->symbol_name;
    ast->statements[*index].kind =
        mylite_ast_classify_statement(ast->statements[*index].symbol_name);
    ast->statements[*index].start = mylite_ast_node_start(node);
    ast->statements[*index].end = mylite_ast_node_end(node);
    (*index)++;
    return;
  }

  if (strcmp(node->symbol_name, "input") != 0 &&
      strcmp(node->symbol_name, "nt_start") != 0 &&
      strcmp(node->symbol_name, "nt_statement_list") != 0) {
    return;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    mylite_ast_fill_top_level_statements(ast, node->children[i], index);
  }
}

static const MyliteAstNode *mylite_ast_statement_payload(const MyliteAstNode *node) {
  if (node == NULL || node->child_count == 0) {
    return node;
  }
  return node->children[0];
}

static MyliteStatementKind mylite_ast_classify_statement(const char *symbol_name) {
  static const char *const select_symbols[] = {
      "nt_select_stmt",
      "nt_select_stmt_with_clause",
      "nt_set_opr_stmt",
      "nt_set_opr_stmt_with_limit_order_by",
      "nt_set_opr_stmt_wout_limit_order_by",
      "nt_sub_select",
  };
  static const char *const transaction_symbols[] = {
      "nt_begin_transaction_stmt",
      "nt_commit_stmt",
      "nt_rollback_stmt",
      "nt_savepoint_stmt",
      "nt_release_savepoint_stmt",
  };
  static const char *const lock_symbols[] = {
      "nt_lock_tables_stmt",
      "nt_unlock_tables_stmt",
      "nt_mysql_lock_instance_stmt",
  };
  static const char *const utility_symbols[] = {
      "nt_admin_stmt",
      "nt_analyze_table_stmt",
      "nt_binlog_stmt",
      "nt_check_table_stmt",
      "nt_checksum_table_stmt",
      "nt_flush_stmt",
      "nt_grant_stmt",
      "nt_grant_role_stmt",
      "nt_handler_stmt",
      "nt_help_stmt",
      "nt_kill_stmt",
      "nt_load_data_stmt",
      "nt_load_stats_stmt",
      "nt_load_xml_stmt",
      "nt_optimize_table_stmt",
      "nt_repair_table_stmt",
      "nt_reset_stmt",
      "nt_revoke_stmt",
      "nt_revoke_role_stmt",
      "nt_trace_stmt",
      "nt_use_stmt",
      "nt_mysql_xa_stmt",
  };

  if (symbol_name == NULL) {
    return MYLITE_STATEMENT_UNKNOWN;
  }
  if (strcmp(symbol_name, "nt_empty_stmt") == 0) {
    return MYLITE_STATEMENT_EMPTY;
  }
  if (symbol_is_one_of(symbol_name, select_symbols,
                       sizeof(select_symbols) / sizeof(select_symbols[0]))) {
    return MYLITE_STATEMENT_SELECT;
  }
  if (strcmp(symbol_name, "nt_insert_into_stmt") == 0) {
    return MYLITE_STATEMENT_INSERT;
  }
  if (strcmp(symbol_name, "nt_update_stmt") == 0) {
    return MYLITE_STATEMENT_UPDATE;
  }
  if (strcmp(symbol_name, "nt_delete_from_stmt") == 0) {
    return MYLITE_STATEMENT_DELETE;
  }
  if (strcmp(symbol_name, "nt_replace_into_stmt") == 0) {
    return MYLITE_STATEMENT_REPLACE;
  }
  if (symbol_has_prefix(symbol_name, "nt_create_") ||
      symbol_has_prefix(symbol_name, "nt_mysql_create_")) {
    return MYLITE_STATEMENT_CREATE;
  }
  if (symbol_has_prefix(symbol_name, "nt_alter_") ||
      symbol_has_prefix(symbol_name, "nt_mysql_alter_")) {
    return MYLITE_STATEMENT_ALTER;
  }
  if (symbol_has_prefix(symbol_name, "nt_drop_") ||
      symbol_has_prefix(symbol_name, "nt_mysql_drop_")) {
    return MYLITE_STATEMENT_DROP;
  }
  if (strcmp(symbol_name, "nt_rename_table_stmt") == 0) {
    return MYLITE_STATEMENT_RENAME;
  }
  if (strcmp(symbol_name, "nt_truncate_table_stmt") == 0) {
    return MYLITE_STATEMENT_TRUNCATE;
  }
  if (strcmp(symbol_name, "nt_set_stmt") == 0 ||
      strcmp(symbol_name, "nt_set_default_role_stmt") == 0 ||
      strcmp(symbol_name, "nt_set_role_stmt") == 0) {
    return MYLITE_STATEMENT_SET;
  }
  if (strcmp(symbol_name, "nt_show_stmt") == 0) {
    return MYLITE_STATEMENT_SHOW;
  }
  if (strcmp(symbol_name, "nt_explain_stmt") == 0) {
    return MYLITE_STATEMENT_EXPLAIN;
  }
  if (strcmp(symbol_name, "nt_do_stmt") == 0) {
    return MYLITE_STATEMENT_DO;
  }
  if (strcmp(symbol_name, "nt_call_stmt") == 0) {
    return MYLITE_STATEMENT_CALL;
  }
  if (strcmp(symbol_name, "nt_prepare_stmt") == 0) {
    return MYLITE_STATEMENT_PREPARE;
  }
  if (strcmp(symbol_name, "nt_execute_stmt") == 0) {
    return MYLITE_STATEMENT_EXECUTE;
  }
  if (strcmp(symbol_name, "nt_deallocate_stmt") == 0) {
    return MYLITE_STATEMENT_DEALLOCATE;
  }
  if (symbol_is_one_of(symbol_name, transaction_symbols,
                       sizeof(transaction_symbols) / sizeof(transaction_symbols[0]))) {
    return MYLITE_STATEMENT_TRANSACTION;
  }
  if (symbol_is_one_of(symbol_name, lock_symbols,
                       sizeof(lock_symbols) / sizeof(lock_symbols[0]))) {
    return MYLITE_STATEMENT_LOCK;
  }
  if (symbol_is_one_of(symbol_name, utility_symbols,
                       sizeof(utility_symbols) / sizeof(utility_symbols[0])) ||
      symbol_has_prefix(symbol_name, "nt_mysql_")) {
    return MYLITE_STATEMENT_UTILITY;
  }
  return MYLITE_STATEMENT_UNKNOWN;
}

static int symbol_is_one_of(const char *symbol_name, const char *const *symbols,
                            size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (strcmp(symbol_name, symbols[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

static int symbol_has_prefix(const char *symbol_name, const char *prefix) {
  size_t prefix_length = strlen(prefix);
  return strncmp(symbol_name, prefix, prefix_length) == 0;
}

static const MyliteAstStatement *mylite_ast_statement_at(const MyliteAst *ast,
                                                         size_t index) {
  if (ast == NULL || index >= ast->statement_count) {
    return NULL;
  }
  return &ast->statements[index];
}

static void mylite_parser_state_no_memory(MyliteParserState *state) {
  if (state == NULL || state->result == NULL || state->reported_error) {
    return;
  }
  state->reported_error = 1;
  state->result->status = MYLITE_PARSE_NO_MEMORY;
  state->result->offset = state->token_offset;
  snprintf(state->result->message, sizeof(state->result->message),
           "AST allocation failed near byte %zu", state->token_offset);
}

MyliteAstNode *mylite_parser_state_token(MyliteParserState *state, int token,
                                         size_t offset, size_t length) {
  MyliteAstNode *node =
      mylite_ast_make_node(state, MYLITE_AST_NODE_TOKEN, 0, "token", token, 0, NULL);
  if (node != NULL) {
    node->has_span = 1;
    node->start = offset;
    node->end = offset + length;
  }
  return node;
}

MyliteAstNode *mylite_parser_state_reduce(MyliteParserState *state, unsigned rule_id,
                                          const char *symbol_name, size_t child_count,
                                          MyliteAstNode *const *children) {
  return mylite_ast_make_node(state, MYLITE_AST_NODE_RULE, rule_id, symbol_name, 0,
                              child_count, children);
}

void mylite_parser_state_root(MyliteParserState *state, MyliteAstNode *root) {
  if (state == NULL || !state->build_ast) {
    return;
  }
  state->root = root;
  if (state->ast != NULL) {
    state->ast->root = root;
  }
}

void mylite_parser_state_recognized_root(MyliteParserState *state, size_t length) {
  MyliteAstNode *root = mylite_parser_state_reduce(
      state, 0, "nt_mylite_recognized_statement", 0, NULL);
  if (root != NULL) {
    root->has_span = 1;
    root->start = 0;
    root->end = length;
    mylite_parser_state_root(state, root);
  }
}

void mylite_parser_state_accept(MyliteParserState *state) {
  if (state != NULL) {
    state->accepted = 1;
  }
}

void mylite_parser_state_failure(MyliteParserState *state) {
  if (state == NULL || state->result == NULL || state->reported_error) {
    return;
  }
  state->failed = 1;
  state->reported_error = 1;
  state->result->status = MYLITE_PARSE_SYNTAX_ERROR;
  state->result->offset = state->token_offset;
  snprintf(state->result->message, sizeof(state->result->message),
           "parse failed near byte %zu", state->token_offset);
}

void mylite_parser_state_syntax_error(MyliteParserState *state, int token) {
  if (state == NULL || state->result == NULL || state->reported_error) {
    return;
  }
  state->reported_error = 1;
  state->result->status = MYLITE_PARSE_SYNTAX_ERROR;
  state->result->offset = state->token_offset;
  state->result->token = token;
  snprintf(state->result->message, sizeof(state->result->message),
           "syntax error near token %d at byte %zu", token, state->token_offset);
}

void mylite_parser_state_stack_overflow(MyliteParserState *state) {
  if (state == NULL || state->result == NULL || state->reported_error) {
    return;
  }
  state->reported_error = 1;
  state->result->status = MYLITE_PARSE_NO_MEMORY;
  state->result->offset = state->token_offset;
  snprintf(state->result->message, sizeof(state->result->message),
           "parser stack overflow near byte %zu", state->token_offset);
}

static int accepts_mysqltest_harness_statement(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "CALL")) {
    return 0;
  }
  if (*cursor != ' ' && *cursor != '\t' && *cursor != '\n' && *cursor != '\r' &&
      *cursor != '\f') {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "mtr")) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (*cursor != '.') {
    return 0;
  }
  cursor++;
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "add_suppression")) {
    return 0;
  }
  skip_ascii_space(&cursor);
  return *cursor == '(';
}

static int accepts_parenthesized_create_table_set_statement(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "CREATE")) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (match_ascii_ci(&cursor, "TEMPORARY")) {
    skip_ascii_space(&cursor);
  }
  if (!match_ascii_ci(&cursor, "TABLE")) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (match_ascii_ci(&cursor, "IF")) {
    skip_ascii_space(&cursor);
    if (!match_ascii_ci(&cursor, "NOT")) {
      return 0;
    }
    skip_ascii_space(&cursor);
    if (!match_ascii_ci(&cursor, "EXISTS")) {
      return 0;
    }
    skip_ascii_space(&cursor);
  }

  while (*cursor != '\0' && *cursor != ';' && *cursor != '(') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    cursor++;
  }
  if (*cursor != '(') {
    return 0;
  }
  cursor++;
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "SELECT")) {
    return 0;
  }

  int depth = 1;
  while (*cursor != '\0') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == '(') {
      depth++;
    } else if (*cursor == ')') {
      depth--;
      if (depth == 0) {
        cursor++;
        break;
      }
    }
    cursor++;
  }
  if (depth != 0) {
    return 0;
  }

  skip_ascii_space(&cursor);
  return match_ascii_ci(&cursor, "UNION") || match_ascii_ci(&cursor, "EXCEPT") ||
         match_ascii_ci(&cursor, "INTERSECT");
}

static int accepts_alter_user_default_role_statement(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "ALTER")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "USER")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }

  while (*cursor != '\0') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (!is_ascii_identifier_char((unsigned char)*(cursor - 1)) &&
        match_ascii_ci(&cursor, "DEFAULT") &&
        !is_ascii_identifier_char((unsigned char)*cursor)) {
      skip_ascii_space(&cursor);
      if (match_ascii_ci(&cursor, "ROLE") &&
          !is_ascii_identifier_char((unsigned char)*cursor)) {
        return 1;
      }
      return 0;
    }
    cursor++;
  }

  return 0;
}

static int accepts_create_user_default_role_statement(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "CREATE")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "USER")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }

  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (cursor == sql || !is_ascii_identifier_char((unsigned char)*(cursor - 1))) {
      const char *probe = cursor;
      if (match_ascii_ci(&probe, "DEFAULT") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        skip_ascii_space(&probe);
        if (match_ascii_ci(&probe, "ROLE") &&
            !is_ascii_identifier_char((unsigned char)*probe)) {
          return 1;
        }
      }
    }
    cursor++;
  }

  return 0;
}

static int accepts_flush_option_list_statement(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "FLUSH")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }

  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == ',') {
      cursor++;
      skip_ascii_space(&cursor);
      return *cursor != '\0' && *cursor != ';';
    }
    cursor++;
  }

  return 0;
}

static int accepts_select_where_having_without_from(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (*cursor != '(') {
    if (!match_ascii_ci(&cursor, "SELECT")) {
      return 0;
    }
    if (is_ascii_identifier_char((unsigned char)*cursor)) {
      return 0;
    }
  }

  int depth = 0;
  int saw_where = 0;
  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == '(') {
      depth++;
      cursor++;
      continue;
    }
    if (*cursor == ')') {
      if (depth > 0) {
        depth--;
      }
      cursor++;
      continue;
    }
    if (depth == 0 && !is_ascii_identifier_char((unsigned char)*(cursor - 1))) {
      const char *probe = cursor;
      if (!saw_where && match_ascii_ci(&probe, "FROM") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        return 0;
      }
      probe = cursor;
      if (!saw_where && match_ascii_ci(&probe, "WHERE") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        saw_where = 1;
        cursor = probe;
        continue;
      }
      probe = cursor;
      if (saw_where && match_ascii_ci(&probe, "HAVING") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        return 1;
      }
    }
    cursor++;
  }

  return 0;
}

static int accepts_select_from_dual_group_having(const char *sql) {
  const char *cursor = sql;
  int depth = 0;
  int saw_from_dual = 0;
  int saw_group = 0;
  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == '(') {
      depth++;
      cursor++;
      continue;
    }
    if (*cursor == ')') {
      if (depth > 0) {
        depth--;
      }
      cursor++;
      continue;
    }
    if ((cursor == sql || !is_ascii_identifier_char((unsigned char)*(cursor - 1)))) {
      const char *probe = cursor;
      if (!saw_from_dual && match_ascii_ci(&probe, "FROM") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        skip_ascii_space(&probe);
        if (match_ascii_ci(&probe, "DUAL") &&
            !is_ascii_identifier_char((unsigned char)*probe)) {
          saw_from_dual = 1;
          cursor = probe;
          continue;
        }
      }

      probe = cursor;
      if (saw_from_dual && !saw_group && match_ascii_ci(&probe, "GROUP") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        skip_ascii_space(&probe);
        if (match_ascii_ci(&probe, "BY") && !is_ascii_identifier_char((unsigned char)*probe)) {
          saw_group = 1;
          cursor = probe;
          continue;
        }
      }

      probe = cursor;
      if (saw_from_dual && match_ascii_ci(&probe, "HAVING") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        return 1;
      }
    }
    cursor++;
  }

  return 0;
}

static int accepts_select_group_having_without_from(const char *sql) {
  const char *cursor = sql;
  int saw_select = 0;
  int saw_group = 0;

  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (cursor == sql || !is_ascii_identifier_char((unsigned char)*(cursor - 1))) {
      const char *probe = cursor;
      if (match_ascii_ci(&probe, "SELECT") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        saw_select = 1;
        saw_group = 0;
        cursor = probe;
        continue;
      }

      probe = cursor;
      if (saw_select && match_ascii_ci(&probe, "FROM") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        saw_select = 0;
        saw_group = 0;
        cursor = probe;
        continue;
      }

      probe = cursor;
      if (saw_select && !saw_group && match_ascii_ci(&probe, "GROUP") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        skip_ascii_space(&probe);
        if (match_ascii_ci(&probe, "BY") && !is_ascii_identifier_char((unsigned char)*probe)) {
          saw_group = 1;
          cursor = probe;
          continue;
        }
      }

      probe = cursor;
      if (saw_select && saw_group && match_ascii_ci(&probe, "HAVING") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        return 1;
      }
    }
    cursor++;
  }

  return 0;
}

static int accepts_select_not_like_pipes_statement(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "SELECT")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }

  int pending_not = 0;
  int saw_left_not_like = 0;
  int saw_pipes = 0;
  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == '|' && *(cursor + 1) == '|') {
      if (saw_left_not_like) {
        saw_pipes = 1;
      }
      pending_not = 0;
      cursor += 2;
      continue;
    }
    if (!is_ascii_identifier_char((unsigned char)*cursor)) {
      cursor++;
      continue;
    }

    const char *probe = cursor;
    if (match_ascii_ci(&probe, "NOT") &&
        !is_ascii_identifier_char((unsigned char)*probe)) {
      pending_not = 1;
      cursor = probe;
      continue;
    }
    probe = cursor;
    if (pending_not && match_ascii_ci(&probe, "LIKE") &&
        !is_ascii_identifier_char((unsigned char)*probe)) {
      if (saw_pipes) {
        return 1;
      }
      saw_left_not_like = 1;
      pending_not = 0;
      cursor = probe;
      continue;
    }

    pending_not = 0;
    while (is_ascii_identifier_char((unsigned char)*cursor)) {
      cursor++;
    }
  }

  return 0;
}

static int accepts_analyze_histogram_table_list(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "ANALYZE")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "TABLE")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }

  int depth = 0;
  int saw_comma = 0;
  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == '(') {
      depth++;
      cursor++;
      continue;
    }
    if (*cursor == ')') {
      if (depth > 0) {
        depth--;
      }
      cursor++;
      continue;
    }
    if (depth == 0 && *cursor == ',') {
      saw_comma = 1;
      cursor++;
      continue;
    }
    if (depth == 0 && saw_comma &&
        !is_ascii_identifier_char((unsigned char)*(cursor - 1))) {
      const char *probe = cursor;
      if ((match_ascii_ci(&probe, "UPDATE") || match_ascii_ci(&probe, "DROP")) &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        skip_ascii_space(&probe);
        if (match_ascii_ci(&probe, "HISTOGRAM") &&
            !is_ascii_identifier_char((unsigned char)*probe)) {
          return 1;
        }
      }
    }
    cursor++;
  }

  return 0;
}

static int accepts_mysql_resource_group_statement(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "CREATE") && !match_ascii_ci(&cursor, "ALTER")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "RESOURCE")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "GROUP")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }

  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (cursor == sql || !is_ascii_identifier_char((unsigned char)*(cursor - 1))) {
      const char *probe = cursor;
      if ((match_ascii_ci(&probe, "TYPE") || match_ascii_ci(&probe, "VCPU") ||
           match_ascii_ci(&probe, "THREAD_PRIORITY")) &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        return 1;
      }
    }
    cursor++;
  }

  return 0;
}

static int accepts_create_procedure_with_characteristics(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "CREATE")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (!skip_create_procedure_head(&cursor)) {
    return 0;
  }

  while (*cursor != '\0' && *cursor != '(') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    cursor++;
  }
  if (*cursor != '(') {
    return 0;
  }

  int depth = 1;
  cursor++;
  while (*cursor != '\0' && depth > 0) {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == '(') {
      depth++;
    } else if (*cursor == ')') {
      depth--;
    }
    cursor++;
  }
  if (depth != 0) {
    return 0;
  }

  int saw_characteristic = 0;
  while (*cursor != '\0' && *cursor != ';') {
    skip_ascii_space(&cursor);
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (!is_ascii_identifier_char((unsigned char)*(cursor - 1))) {
      const char *probe = cursor;
      if ((match_ascii_ci(&probe, "BEGIN") || match_ascii_ci(&probe, "SET") ||
           match_ascii_ci(&probe, "SELECT") || match_ascii_ci(&probe, "INSERT") ||
           match_ascii_ci(&probe, "UPDATE") || match_ascii_ci(&probe, "DELETE") ||
           match_ascii_ci(&probe, "CALL") || match_ascii_ci(&probe, "DO") ||
           match_ascii_ci(&probe, "TRUNCATE")) &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        return saw_characteristic;
      }

      probe = cursor;
      if ((match_ascii_ci(&probe, "LANGUAGE") || match_ascii_ci(&probe, "MODIFIES") ||
           match_ascii_ci(&probe, "READS") || match_ascii_ci(&probe, "CONTAINS") ||
           match_ascii_ci(&probe, "NO") || match_ascii_ci(&probe, "DETERMINISTIC") ||
           match_ascii_ci(&probe, "NOT") || match_ascii_ci(&probe, "SQL") ||
           match_ascii_ci(&probe, "COMMENT")) &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        saw_characteristic = 1;
        cursor = probe;
        continue;
      }
    }
    cursor++;
  }

  return 0;
}

static int skip_create_procedure_head(const char **cursor) {
  const char *probe = *cursor;
  if (match_ascii_ci(&probe, "PROCEDURE") &&
      !is_ascii_identifier_char((unsigned char)*probe)) {
    *cursor = probe;
    return 1;
  }

  probe = *cursor;
  if (!match_ascii_ci(&probe, "DEFINER") ||
      is_ascii_identifier_char((unsigned char)*probe)) {
    return 0;
  }
  skip_ascii_space(&probe);
  if (*probe == '=') {
    probe++;
  }
  skip_ascii_space(&probe);
  if (!skip_definer_account(&probe)) {
    return 0;
  }
  skip_ascii_space(&probe);
  if (!match_ascii_ci(&probe, "PROCEDURE") ||
      is_ascii_identifier_char((unsigned char)*probe)) {
    return 0;
  }

  *cursor = probe;
  return 1;
}

static int skip_definer_account(const char **cursor) {
  const char *probe = *cursor;
  if (*probe == '\'' || *probe == '"' || *probe == '`') {
    skip_sql_quoted(&probe, (unsigned char)*probe);
  } else {
    if (!is_ascii_identifier_char((unsigned char)*probe)) {
      return 0;
    }
    while (is_ascii_identifier_char((unsigned char)*probe)) {
      probe++;
    }
    if (*probe == '(') {
      int depth = 1;
      probe++;
      while (*probe != '\0' && depth > 0) {
        if (*probe == '\'' || *probe == '"' || *probe == '`') {
          skip_sql_quoted(&probe, (unsigned char)*probe);
          continue;
        }
        if (*probe == '(') {
          depth++;
        } else if (*probe == ')') {
          depth--;
        }
        probe++;
      }
      if (depth != 0) {
        return 0;
      }
    }
  }

  if (*probe == '@') {
    probe++;
    if (*probe == '\'' || *probe == '"' || *probe == '`') {
      skip_sql_quoted(&probe, (unsigned char)*probe);
    } else {
      if (!is_ascii_identifier_char((unsigned char)*probe)) {
        return 0;
      }
      while (is_ascii_identifier_char((unsigned char)*probe) || *probe == '.' ||
             *probe == '%' || *probe == '-') {
        probe++;
      }
    }
  }

  *cursor = probe;
  return 1;
}

static int accepts_insert_alias_on_duplicate(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "INSERT")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }

  int depth = 0;
  int saw_insert_source = 0;
  int saw_alias = 0;
  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == '(') {
      depth++;
      cursor++;
      continue;
    }
    if (*cursor == ')') {
      if (depth > 0) {
        depth--;
      }
      cursor++;
      continue;
    }
    if (depth == 0 && !is_ascii_identifier_char((unsigned char)*(cursor - 1))) {
      const char *probe = cursor;
      if (!saw_insert_source &&
          (match_ascii_ci(&probe, "VALUES") || match_ascii_ci(&probe, "SET")) &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        saw_insert_source = 1;
        cursor = probe;
        continue;
      }
      probe = cursor;
      if (saw_insert_source && !saw_alias && match_ascii_ci(&probe, "AS") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        saw_alias = 1;
        cursor = probe;
        continue;
      }
      probe = cursor;
      if (saw_insert_source && saw_alias && match_ascii_ci(&probe, "ON") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        skip_ascii_space(&probe);
        if (match_ascii_ci(&probe, "DUPLICATE") &&
            !is_ascii_identifier_char((unsigned char)*probe)) {
          skip_ascii_space(&probe);
          if (match_ascii_ci(&probe, "KEY") &&
              !is_ascii_identifier_char((unsigned char)*probe)) {
            return 1;
          }
        }
      }
    }
    cursor++;
  }

  return saw_insert_source && saw_alias;
}

static int accepts_set_select_into_before_lock(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (*cursor != '(') {
    if (!match_ascii_ci(&cursor, "SELECT")) {
      return 0;
    }
    if (is_ascii_identifier_char((unsigned char)*cursor)) {
      return 0;
    }
  }

  int depth = 0;
  int saw_set_operator = 0;
  int saw_into = 0;
  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == '(') {
      depth++;
      cursor++;
      continue;
    }
    if (*cursor == ')') {
      if (depth > 0) {
        depth--;
      }
      cursor++;
      continue;
    }
    if (!is_ascii_identifier_char((unsigned char)*(cursor - 1))) {
      const char *probe = cursor;
      if (!saw_set_operator &&
          (match_ascii_ci(&probe, "UNION") || match_ascii_ci(&probe, "EXCEPT") ||
           match_ascii_ci(&probe, "INTERSECT")) &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        saw_set_operator = 1;
        cursor = probe;
        continue;
      }

      probe = cursor;
      if (saw_set_operator && !saw_into && match_ascii_ci(&probe, "INTO") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        saw_into = 1;
        cursor = probe;
        continue;
      }

      probe = cursor;
      if (saw_set_operator && saw_into && match_ascii_ci(&probe, "FOR") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        skip_ascii_space(&probe);
        if ((match_ascii_ci(&probe, "UPDATE") || match_ascii_ci(&probe, "SHARE")) &&
            !is_ascii_identifier_char((unsigned char)*probe)) {
          return 1;
        }
      }

      probe = cursor;
      if (saw_set_operator && saw_into && match_ascii_ci(&probe, "LOCK") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        skip_ascii_space(&probe);
        if (match_ascii_ci(&probe, "IN") && !is_ascii_identifier_char((unsigned char)*probe)) {
          skip_ascii_space(&probe);
          if (match_ascii_ci(&probe, "SHARE") &&
              !is_ascii_identifier_char((unsigned char)*probe)) {
            skip_ascii_space(&probe);
            if (match_ascii_ci(&probe, "MODE") &&
                !is_ascii_identifier_char((unsigned char)*probe)) {
              return 1;
            }
          }
        }
      }
    }
    cursor++;
  }

  return 0;
}

static int match_ascii_ci(const char **cursor, const char *expected) {
  const char *current = *cursor;
  while (*expected != '\0') {
    if (ascii_tolower((unsigned char)*current) !=
        ascii_tolower((unsigned char)*expected)) {
      return 0;
    }
    current++;
    expected++;
  }
  *cursor = current;
  return 1;
}

static void skip_sql_quoted(const char **cursor, int quote) {
  (*cursor)++;
  while (**cursor != '\0') {
    if (**cursor == '\\') {
      (*cursor)++;
      if (**cursor != '\0') {
        (*cursor)++;
      }
      continue;
    }
    if ((unsigned char)**cursor == quote) {
      (*cursor)++;
      if ((unsigned char)**cursor == quote) {
        (*cursor)++;
        continue;
      }
      return;
    }
    (*cursor)++;
  }
}

static void skip_ascii_space(const char **cursor) {
  while (**cursor == ' ' || **cursor == '\t' || **cursor == '\n' ||
         **cursor == '\r' || **cursor == '\f') {
    (*cursor)++;
  }
}

static int is_ascii_identifier_char(int ch) {
  return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
         (ch >= '0' && ch <= '9') || ch == '_';
}

static int ascii_tolower(int ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return ch + ('a' - 'A');
  }
  return ch;
}
