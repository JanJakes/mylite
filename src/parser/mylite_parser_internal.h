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
  int accepted;
  int failed;
  MyliteParseResult *result;
} MyliteParseContext;

void mylite_parser_accept(MyliteParseContext *ctx);
void mylite_parser_failure(MyliteParseContext *ctx);
void mylite_parser_syntax_error(MyliteParseContext *ctx, int token_id,
                                MyliteToken token);

void *MyLiteLemonAlloc(void *(*malloc_proc)(size_t));
void MyLiteLemon(void *parser, int token_id, MyliteToken token,
                 MyliteParseContext *ctx);
void MyLiteLemonFree(void *parser, void (*free_proc)(void *));

#endif

