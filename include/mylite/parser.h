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

MyliteParseStatus mylite_parse_sql(const char *sql, MyliteParseResult *result);
MyliteParseStatus mylite_parse_sql_ast(const char *sql, MyliteAst **ast,
                                       MyliteParseResult *result);
const char *mylite_parse_status_name(MyliteParseStatus status);

void mylite_ast_free(MyliteAst *ast);
const MyliteAstNode *mylite_ast_root(const MyliteAst *ast);
size_t mylite_ast_node_count(const MyliteAst *ast);
size_t mylite_ast_allocated_bytes(const MyliteAst *ast);
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
