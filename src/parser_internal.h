#ifndef MYLITE_PARSER_INTERNAL_H
#define MYLITE_PARSER_INTERNAL_H

#include "mylite/parser.h"

typedef struct mylite_lexer {
	const char *input;
	size_t length;
	size_t offset;
	unsigned int line;
	unsigned int column;
	size_t token_count;
	size_t token_start_offset;
	size_t token_end_offset;
	unsigned int token_start_line;
	unsigned int token_start_column;
	unsigned int token_end_line;
	unsigned int token_end_column;
	size_t last_significant_token;
	size_t last_significant_token_end_offset;
	unsigned int last_significant_token_end_line;
	unsigned int last_significant_token_end_column;
	int executable_comment;
	char error[256];
	unsigned int error_line;
	unsigned int error_column;
} mylite_lexer;

typedef struct mylite_parser {
	mylite_lexer lexer;
	mylite_statement *statements;
	size_t statement_count;
	size_t statement_capacity;
	mylite_statement_kind active_statement_kind;
	size_t active_statement_first_token;
	size_t active_statement_start_offset;
	unsigned int active_statement_start_line;
	unsigned int active_statement_start_column;
	size_t active_statement_body_items;
	int active_statement_requires_body;
	char error[256];
	unsigned int error_line;
	unsigned int error_column;
} mylite_parser;

void mylite_parser_init(mylite_parser *parser, const char *sql, size_t length);
void mylite_parser_destroy(mylite_parser *parser);
void mylite_parser_begin_statement(mylite_parser *parser, mylite_statement_kind kind, int requires_body);
int mylite_parser_add_statement(mylite_parser *parser, mylite_statement_kind kind);
void mylite_parser_set_error(mylite_parser *parser, const char *message);

#endif
