#ifndef MYLITE_LEXER_H
#define MYLITE_LEXER_H

#include "parser_internal.h"

void mylite_lexer_init(mylite_lexer *lexer, const char *input, size_t length);
int mylite_lexer_next(mylite_parser *parser);

#endif
