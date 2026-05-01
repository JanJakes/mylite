#ifndef MYLITE_PARSER_INTERNAL_H
#define MYLITE_PARSER_INTERNAL_H

#include "mylite/parser.h"

#include <stddef.h>

typedef struct MyliteParserState {
  MyliteParseResult *result;
  MyliteAst *ast;
  MyliteAstNode *root;
  size_t token_offset;
  int build_ast;
  int accepted;
  int failed;
  int reported_error;
} MyliteParserState;

MyliteAstNode *mylite_parser_state_token(MyliteParserState *state, int token,
                                         size_t offset, size_t length);
MyliteAstNode *mylite_parser_state_reduce(MyliteParserState *state, unsigned rule_id,
                                          const char *symbol_name, size_t child_count,
                                          MyliteAstNode *const *children);
void mylite_parser_state_root(MyliteParserState *state, MyliteAstNode *root);
void mylite_parser_state_recognized_root(MyliteParserState *state, size_t length);
void mylite_parser_state_accept(MyliteParserState *state);
void mylite_parser_state_failure(MyliteParserState *state);
void mylite_parser_state_syntax_error(MyliteParserState *state, int token);
void mylite_parser_state_stack_overflow(MyliteParserState *state);

#endif
