#ifndef MYLITE_LEXER_H
#define MYLITE_LEXER_H

#include <stddef.h>

typedef struct MyliteToken {
  int type;
  size_t offset;
} MyliteToken;

typedef struct MyliteLexer {
  const char *sql;
  size_t length;
  size_t offset;
  size_t event_body_start;
  size_t event_body_end;
  int previous_token;
  int skip_interval_quantity_rparen;
  int interval_quantity_depth;
  int fetch_statement_pending;
  int fetch_into_targets;
  int prepare_statement_pending;
  int prepare_from_source;
  int tablespace_name_pending;
  int set_statement_pending;
  int set_persist_assignment_list;
  int set_persist_optional_prefix;
  int event_body_available;
  int event_body_emitted;
  int lex_error;
  size_t lex_error_offset;
} MyliteLexer;

void mylite_lexer_init(MyliteLexer *lexer, const char *sql);
MyliteToken mylite_lexer_next(MyliteLexer *lexer);

#endif
