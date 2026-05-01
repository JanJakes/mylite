#ifndef MYLITE_PARSER_H
#define MYLITE_PARSER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MyliteParseStatus {
  MYLITE_PARSE_OK = 0,
  MYLITE_PARSE_SYNTAX_ERROR = 1,
  MYLITE_PARSE_LEX_ERROR = 2,
  MYLITE_PARSE_NO_MEMORY = 3
} MyliteParseStatus;

typedef struct MyliteParseResult {
  MyliteParseStatus status;
  size_t offset;
  int token;
  char message[160];
} MyliteParseResult;

typedef struct MyliteAst MyliteAst;
typedef struct MyliteAstNode MyliteAstNode;

typedef enum MyliteAstNodeKind {
  MYLITE_AST_NODE_RULE = 1,
  MYLITE_AST_NODE_TOKEN = 2
} MyliteAstNodeKind;

typedef enum MyliteStatementKind {
  MYLITE_STATEMENT_UNKNOWN = 0,
  MYLITE_STATEMENT_EMPTY,
  MYLITE_STATEMENT_SELECT,
  MYLITE_STATEMENT_INSERT,
  MYLITE_STATEMENT_UPDATE,
  MYLITE_STATEMENT_DELETE,
  MYLITE_STATEMENT_REPLACE,
  MYLITE_STATEMENT_CREATE,
  MYLITE_STATEMENT_ALTER,
  MYLITE_STATEMENT_DROP,
  MYLITE_STATEMENT_RENAME,
  MYLITE_STATEMENT_TRUNCATE,
  MYLITE_STATEMENT_SET,
  MYLITE_STATEMENT_SHOW,
  MYLITE_STATEMENT_EXPLAIN,
  MYLITE_STATEMENT_DO,
  MYLITE_STATEMENT_CALL,
  MYLITE_STATEMENT_PREPARE,
  MYLITE_STATEMENT_EXECUTE,
  MYLITE_STATEMENT_DEALLOCATE,
  MYLITE_STATEMENT_TRANSACTION,
  MYLITE_STATEMENT_LOCK,
  MYLITE_STATEMENT_UTILITY
} MyliteStatementKind;

typedef enum MyliteStatementTargetKind {
  MYLITE_STATEMENT_TARGET_NONE = 0,
  MYLITE_STATEMENT_TARGET_TABLE,
  MYLITE_STATEMENT_TARGET_DATABASE,
  MYLITE_STATEMENT_TARGET_VIEW,
  MYLITE_STATEMENT_TARGET_ROUTINE,
  MYLITE_STATEMENT_TARGET_ACCOUNT,
  MYLITE_STATEMENT_TARGET_VARIABLE,
  MYLITE_STATEMENT_TARGET_UNKNOWN
} MyliteStatementTargetKind;

MyliteParseStatus mylite_parse_sql(const char *sql, MyliteParseResult *result);
MyliteParseStatus mylite_parse_sql_ast(const char *sql, MyliteAst **ast,
                                       MyliteParseResult *result);
const char *mylite_parse_status_name(MyliteParseStatus status);
const char *mylite_statement_kind_name(MyliteStatementKind kind);
const char *mylite_statement_target_kind_name(MyliteStatementTargetKind kind);

void mylite_ast_free(MyliteAst *ast);
const MyliteAstNode *mylite_ast_root(const MyliteAst *ast);
size_t mylite_ast_node_count(const MyliteAst *ast);
size_t mylite_ast_allocated_bytes(const MyliteAst *ast);
size_t mylite_ast_statement_count(const MyliteAst *ast);
MyliteStatementKind mylite_ast_statement_kind(const MyliteAst *ast, size_t index);
const char *mylite_ast_statement_symbol_name(const MyliteAst *ast, size_t index);
const MyliteAstNode *mylite_ast_statement_node(const MyliteAst *ast, size_t index);
size_t mylite_ast_statement_start(const MyliteAst *ast, size_t index);
size_t mylite_ast_statement_end(const MyliteAst *ast, size_t index);
MyliteStatementTargetKind mylite_ast_statement_target_kind(const MyliteAst *ast,
                                                           size_t index);
size_t mylite_ast_statement_target_start(const MyliteAst *ast, size_t index);
size_t mylite_ast_statement_target_end(const MyliteAst *ast, size_t index);
size_t mylite_ast_statement_target_schema_start(const MyliteAst *ast, size_t index);
size_t mylite_ast_statement_target_schema_end(const MyliteAst *ast, size_t index);
size_t mylite_ast_statement_target_name_start(const MyliteAst *ast, size_t index);
size_t mylite_ast_statement_target_name_end(const MyliteAst *ast, size_t index);
MyliteAstNodeKind mylite_ast_node_kind(const MyliteAstNode *node);
unsigned mylite_ast_node_rule_id(const MyliteAstNode *node);
const char *mylite_ast_node_symbol_name(const MyliteAstNode *node);
int mylite_ast_node_token(const MyliteAstNode *node);
size_t mylite_ast_node_start(const MyliteAstNode *node);
size_t mylite_ast_node_end(const MyliteAstNode *node);
size_t mylite_ast_node_child_count(const MyliteAstNode *node);
const MyliteAstNode *mylite_ast_node_child(const MyliteAstNode *node,
                                           size_t index);

#ifdef __cplusplus
}
#endif

#endif
