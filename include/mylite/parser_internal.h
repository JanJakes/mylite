#ifndef MYLITE_PARSER_INTERNAL_H
#define MYLITE_PARSER_INTERNAL_H

#include "mylite/parser.h"

#include <stddef.h>

typedef struct MyliteParserState {
  MyliteParseResult *result;
  size_t token_offset;
  int accepted;
  int failed;
  int reported_error;
} MyliteParserState;

void mylite_parser_state_accept(MyliteParserState *state);
void mylite_parser_state_failure(MyliteParserState *state);
void mylite_parser_state_syntax_error(MyliteParserState *state, int token);
void mylite_parser_state_stack_overflow(MyliteParserState *state);

#endif
