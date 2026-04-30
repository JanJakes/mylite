#ifndef MYLITE_LEXER_H
#define MYLITE_LEXER_H

#include <stddef.h>

#include "mylite_parser_internal.h"

typedef struct MyliteLexer {
  const char *sql;
  size_t length;
  size_t offset;
  size_t line;
  size_t column;
  int create_scan;
  int in_compound_definition;
  int compound_depth;
  MyliteParseResult *result;
} MyliteLexer;

void mylite_lexer_init(MyliteLexer *lexer, const char *sql, size_t length,
                       MyliteParseResult *result);
int mylite_lexer_next(MyliteLexer *lexer, MyliteToken *token);

#endif
