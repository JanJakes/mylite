#include "lexer.h"

#include "mylite_tidb_parser.h"
#include "mylite_tidb_tokens.inc"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

static MyliteToken next_token(MyliteLexer *lexer);
static void skip_ignored(MyliteLexer *lexer);
static int can_precede_time_zone_suffix(int token);
static int skip_time_zone_suffix(MyliteLexer *lexer);
static int skip_create_definer_prefix(MyliteLexer *lexer);
static int scan_executable_comment_paren(const MyliteLexer *lexer, size_t offset,
                                         int *token, size_t *end);
static int skip_block_comment(MyliteLexer *lexer);
static int find_event_body_range(const char *sql, size_t length, size_t *start, size_t *end);
static size_t find_event_body_end(const char *sql, size_t length, size_t start);
static int skip_raw_ignored(const char *sql, size_t length, size_t *offset);
static int skip_raw_comment(const char *sql, size_t length, size_t *offset);
static int skip_raw_quoted(const char *sql, size_t length, size_t *offset, char quote);
static int scan_raw_word_upper(const char *sql, size_t length, size_t *offset, char *buffer,
                               size_t buffer_size);
static int next_raw_word_upper(const char *sql, size_t length, size_t offset, char *buffer,
                               size_t buffer_size);
static int has_parenthesized_interval_quantity(const MyliteLexer *lexer, size_t offset);
static int is_interval_time_unit_at(const char *sql, size_t length, size_t offset);
static int is_quoted_identifier_context(const MyliteLexer *lexer);
static int is_quoted_identifier_before_column_type(const MyliteLexer *lexer, size_t offset,
                                                   char quote);
static int is_column_type_start_token(int token);
static int is_persist_system_variable_token(const MyliteLexer *lexer, size_t offset);
static int has_ascii_ci_prefix(const char *sql, size_t length, size_t offset,
                               const char *prefix);
static MyliteToken scan_identifier_or_keyword(MyliteLexer *lexer);
static int scan_compound_keyword(MyliteLexer *lexer, const char *first, int *token);
static int scan_word_upper(const char *sql, size_t length, size_t *offset, char *buffer,
                           size_t buffer_size);
static int keyword_token(const char *keyword, int builtin_context);
static int lookup_keyword_token(const MyliteTidbKeyword *keywords, size_t keyword_count,
                                const char *keyword);
static int is_keyword_context_identifier(const MyliteLexer *lexer);
static int is_table_name_keyword_identifier_context(const MyliteLexer *lexer, int token);
static int is_stored_program_label_identifier_context(const MyliteLexer *lexer,
                                                      size_t lookahead);
static int is_charset_introducer_context(const MyliteLexer *lexer);
static MyliteToken scan_number(MyliteLexer *lexer);
static MyliteToken scan_string(MyliteLexer *lexer, char quote, int token);
static void skip_timestamp_time_zone_suffix(MyliteLexer *lexer);
static MyliteToken scan_backtick_identifier(MyliteLexer *lexer);
static MyliteToken scan_quoted_identifier(MyliteLexer *lexer, char quote);
static MyliteToken scan_at_identifier(MyliteLexer *lexer);
static MyliteToken make_token(MyliteLexer *lexer, int type, size_t offset);
static void set_lex_error(MyliteLexer *lexer, size_t offset);
static int is_identifier_start(unsigned char ch);
static int is_identifier_continue(unsigned char ch);
static int is_hex_digit(unsigned char ch);
static int is_bit_digit(unsigned char ch);
static int is_delimiter(unsigned char ch);
static int peek(const MyliteLexer *lexer, size_t offset);

void mylite_lexer_init(MyliteLexer *lexer, const char *sql) {
  lexer->sql = sql;
  lexer->length = strlen(sql);
  lexer->offset = 0;
  lexer->event_body_start = 0;
  lexer->event_body_end = 0;
  lexer->previous_token = 0;
  lexer->skip_interval_quantity_rparen = 0;
  lexer->interval_quantity_depth = 0;
  lexer->fetch_statement_pending = 0;
  lexer->fetch_into_targets = 0;
  lexer->prepare_statement_pending = 0;
  lexer->prepare_from_source = 0;
  lexer->tablespace_name_pending = 0;
  lexer->set_statement_pending = 0;
  lexer->set_persist_assignment_list = 0;
  lexer->set_persist_optional_prefix = 0;
  lexer->event_body_available =
      find_event_body_range(sql, lexer->length, &lexer->event_body_start,
                            &lexer->event_body_end);
  lexer->event_body_emitted = 0;
  lexer->lex_error = 0;
  lexer->lex_error_offset = 0;
}

MyliteToken mylite_lexer_next(MyliteLexer *lexer) {
  MyliteToken token;
  for (;;) {
    token = next_token(lexer);
    if (lexer->set_persist_optional_prefix) {
      if (token.type == MYLITE_TOK_PERSIST || token.type == MYLITE_TOK_PERSIST_ONLY) {
        lexer->set_persist_optional_prefix = 0;
        continue;
      }
      lexer->set_persist_optional_prefix = 0;
    }
    break;
  }
  if (token.type != 0) {
    if (token.type == MYLITE_TOK_FETCH) {
      lexer->fetch_statement_pending = 1;
      lexer->fetch_into_targets = 0;
    } else if (lexer->fetch_statement_pending && token.type == MYLITE_TOK_INTO) {
      lexer->fetch_into_targets = 1;
    } else if (token.type == MYLITE_TOK_PREPARE) {
      lexer->prepare_statement_pending = 1;
      lexer->prepare_from_source = 0;
    } else if (lexer->prepare_statement_pending && token.type == MYLITE_TOK_FROM) {
      lexer->prepare_from_source = 1;
    } else if (lexer->prepare_from_source) {
      lexer->prepare_statement_pending = 0;
      lexer->prepare_from_source = 0;
    } else if (token.type == MYLITE_TOK_TABLESPACE) {
      lexer->tablespace_name_pending = 1;
    } else if (lexer->tablespace_name_pending && token.type == MYLITE_TOK_EQ) {
      lexer->tablespace_name_pending = 1;
    } else if (lexer->tablespace_name_pending) {
      lexer->tablespace_name_pending = 0;
    } else if (token.type == MYLITE_TOK_SET) {
      lexer->set_statement_pending = 1;
      lexer->set_persist_assignment_list = 1;
      lexer->set_persist_optional_prefix = 0;
    } else if (lexer->set_statement_pending &&
               (token.type == MYLITE_TOK_PERSIST ||
                token.type == MYLITE_TOK_PERSIST_ONLY)) {
      lexer->set_statement_pending = 0;
      lexer->set_persist_assignment_list = 1;
      lexer->set_persist_optional_prefix = 0;
    } else if (lexer->set_statement_pending &&
               token.type == MYLITE_TOK_DOUBLE_AT_IDENTIFIER &&
               is_persist_system_variable_token(lexer, token.offset)) {
      lexer->set_statement_pending = 0;
      lexer->set_persist_assignment_list = 1;
      lexer->set_persist_optional_prefix = 0;
    } else if (lexer->set_persist_assignment_list && token.type == MYLITE_TOK_COMMA) {
      lexer->set_persist_optional_prefix = 1;
    } else if (lexer->set_statement_pending) {
      lexer->set_statement_pending = 0;
    } else if (token.type == MYLITE_TOK_SEMICOLON) {
      lexer->fetch_statement_pending = 0;
      lexer->fetch_into_targets = 0;
      lexer->prepare_statement_pending = 0;
      lexer->prepare_from_source = 0;
      lexer->tablespace_name_pending = 0;
      lexer->set_statement_pending = 0;
      lexer->set_persist_assignment_list = 0;
      lexer->set_persist_optional_prefix = 0;
    }
    lexer->previous_token = token.type;
  }
  return token;
}

static MyliteToken next_token(MyliteLexer *lexer) {
  skip_ignored(lexer);
  if (can_precede_time_zone_suffix(lexer->previous_token) &&
      skip_time_zone_suffix(lexer)) {
    skip_ignored(lexer);
  }
  size_t start = lexer->offset;
  if (lexer->lex_error || start >= lexer->length) {
    return make_token(lexer, 0, start);
  }

  int executable_comment_token = 0;
  size_t executable_comment_end = 0;
  if (scan_executable_comment_paren(lexer, start, &executable_comment_token,
                                    &executable_comment_end)) {
    lexer->offset = executable_comment_end;
    return make_token(lexer, executable_comment_token, start);
  }

  if (lexer->previous_token == MYLITE_TOK_INTERVAL &&
      has_parenthesized_interval_quantity(lexer, start)) {
    lexer->skip_interval_quantity_rparen = 1;
    lexer->interval_quantity_depth = 0;
    lexer->offset++;
    return next_token(lexer);
  }

  if (lexer->event_body_available && !lexer->event_body_emitted &&
      start >= lexer->event_body_start) {
    lexer->event_body_emitted = 1;
    lexer->offset = lexer->event_body_end;
    return make_token(lexer, MYLITE_TOK_EVENT_BODY, start);
  }

  if (lexer->previous_token == MYLITE_TOK_CREATE && skip_create_definer_prefix(lexer)) {
    start = lexer->offset;
  }

  unsigned char ch = (unsigned char)lexer->sql[lexer->offset];
  unsigned char next = (unsigned char)peek(lexer, lexer->offset + 1);

  if ((ch == 'x' || ch == 'X') && next == '\'') {
    lexer->offset++;
    return scan_string(lexer, '\'', MYLITE_TOK_HEX_LIT);
  }
  if ((ch == 'b' || ch == 'B') && next == '\'') {
    lexer->offset++;
    return scan_string(lexer, '\'', MYLITE_TOK_BIT_LIT);
  }
  if ((ch == 'n' || ch == 'N') && next == '\'') {
    lexer->offset++;
    return scan_string(lexer, '\'', MYLITE_TOK_STRING_LIT);
  }

  if (is_identifier_start(ch)) {
    return scan_identifier_or_keyword(lexer);
  }
  if (isdigit(ch) ||
      (ch == '.' && isdigit(next) && lexer->previous_token != MYLITE_TOK_IDENTIFIER)) {
    return scan_number(lexer);
  }

  lexer->offset++;
  switch (ch) {
    case '\'':
      lexer->offset--;
      return scan_string(lexer, '\'', MYLITE_TOK_STRING_LIT);
    case '"':
      if (is_quoted_identifier_context(lexer) ||
          is_quoted_identifier_before_column_type(lexer, start, '"')) {
        lexer->offset--;
        return scan_quoted_identifier(lexer, '"');
      }
      lexer->offset--;
      return scan_string(lexer, '"', MYLITE_TOK_STRING_LIT);
    case '`':
      lexer->offset--;
      return scan_backtick_identifier(lexer);
    case '@':
      lexer->offset--;
      return scan_at_identifier(lexer);
    case '?':
      return make_token(lexer, MYLITE_TOK_PARAM_MARKER, start);
    case '(':
      if (lexer->skip_interval_quantity_rparen) {
        lexer->interval_quantity_depth++;
      }
      return make_token(lexer, MYLITE_TOK_LPAREN, start);
    case ')':
      if (lexer->skip_interval_quantity_rparen) {
        if (lexer->interval_quantity_depth > 0) {
          lexer->interval_quantity_depth--;
        } else if (is_interval_time_unit_at(lexer->sql, lexer->length, lexer->offset)) {
          lexer->skip_interval_quantity_rparen = 0;
          return next_token(lexer);
        }
      }
      return make_token(lexer, MYLITE_TOK_RPAREN, start);
    case '{':
      return make_token(lexer, MYLITE_TOK_LBRACE, start);
    case '}':
      return make_token(lexer, MYLITE_TOK_RBRACE, start);
    case ',':
      return make_token(lexer, MYLITE_TOK_COMMA, start);
    case '.':
      return make_token(lexer, MYLITE_TOK_DOT, start);
    case ';':
      return make_token(lexer, MYLITE_TOK_SEMICOLON, start);
    case '+':
      return make_token(lexer, MYLITE_TOK_PLUS, start);
    case '-':
      if (peek(lexer, lexer->offset) == '>') {
        lexer->offset++;
        if (peek(lexer, lexer->offset) == '>') {
          lexer->offset++;
          return make_token(lexer, MYLITE_TOK_JUSS, start);
        }
        return make_token(lexer, MYLITE_TOK_JSS, start);
      }
      return make_token(lexer, MYLITE_TOK_MINUS, start);
    case '*':
      return make_token(lexer, MYLITE_TOK_STAR, start);
    case '/':
      return make_token(lexer, MYLITE_TOK_SLASH, start);
    case '%':
      return make_token(lexer, MYLITE_TOK_PERCENT, start);
    case '^':
      return make_token(lexer, MYLITE_TOK_CARET, start);
    case '~':
      return make_token(lexer, MYLITE_TOK_TILDE, start);
    case ':':
      if (peek(lexer, lexer->offset) == '=') {
        lexer->offset++;
        return make_token(lexer, MYLITE_TOK_ASSIGNMENT_EQ, start);
      }
      return make_token(lexer, MYLITE_TOK_COLON, start);
    case '=':
      return make_token(lexer, MYLITE_TOK_EQ, start);
    case '!':
      if (peek(lexer, lexer->offset) == '=') {
        lexer->offset++;
        return make_token(lexer, MYLITE_TOK_NEQ, start);
      }
      return make_token(lexer, MYLITE_TOK_BANG, start);
    case '>':
      if (peek(lexer, lexer->offset) == '=') {
        lexer->offset++;
        return make_token(lexer, MYLITE_TOK_GE, start);
      }
      if (peek(lexer, lexer->offset) == '>') {
        lexer->offset++;
        return make_token(lexer, MYLITE_TOK_RSH, start);
      }
      return make_token(lexer, MYLITE_TOK_GT, start);
    case '<':
      if (peek(lexer, lexer->offset) == '=') {
        lexer->offset++;
        if (peek(lexer, lexer->offset) == '>') {
          lexer->offset++;
          return make_token(lexer, MYLITE_TOK_NULLEQ, start);
        }
        return make_token(lexer, MYLITE_TOK_LE, start);
      }
      if (peek(lexer, lexer->offset) == '>') {
        lexer->offset++;
        return make_token(lexer, MYLITE_TOK_NEQ_SYNONYM, start);
      }
      if (peek(lexer, lexer->offset) == '<') {
        lexer->offset++;
        return make_token(lexer, MYLITE_TOK_LSH, start);
      }
      return make_token(lexer, MYLITE_TOK_LT, start);
    case '&':
      if (peek(lexer, lexer->offset) == '&') {
        lexer->offset++;
        return make_token(lexer, MYLITE_TOK_ANDAND, start);
      }
      return make_token(lexer, MYLITE_TOK_AMP, start);
    case '|':
      if (peek(lexer, lexer->offset) == '|') {
        lexer->offset++;
        return make_token(lexer, MYLITE_TOK_PIPES, start);
      }
      return make_token(lexer, MYLITE_TOK_PIPECHAR, start);
  }

  set_lex_error(lexer, start);
  return make_token(lexer, 0, start);
}

static int can_precede_time_zone_suffix(int token) {
  switch (token) {
    case MYLITE_TOK_IDENTIFIER:
    case MYLITE_TOK_STRING_LIT:
    case MYLITE_TOK_INT_LIT:
    case MYLITE_TOK_DEC_LIT:
    case MYLITE_TOK_FLOAT_LIT:
    case MYLITE_TOK_HEX_LIT:
    case MYLITE_TOK_BIT_LIT:
    case MYLITE_TOK_NULL:
    case MYLITE_TOK_TRUE_KWD:
    case MYLITE_TOK_FALSE_KWD:
    case MYLITE_TOK_CURRENT_DATE:
    case MYLITE_TOK_CURRENT_TIME:
    case MYLITE_TOK_CURRENT_TS:
    case MYLITE_TOK_LOCAL_TIME:
    case MYLITE_TOK_LOCAL_TS:
    case MYLITE_TOK_UTC_TIMESTAMP:
    case MYLITE_TOK_SINGLE_AT_IDENTIFIER:
    case MYLITE_TOK_RPAREN:
      return 1;
  }
  return 0;
}

static int skip_time_zone_suffix(MyliteLexer *lexer) {
  size_t offset = lexer->offset;
  char word[128];

  if (!skip_raw_ignored(lexer->sql, lexer->length, &offset) ||
      !scan_raw_word_upper(lexer->sql, lexer->length, &offset, word, sizeof(word)) ||
      strcmp(word, "AT") != 0 ||
      !skip_raw_ignored(lexer->sql, lexer->length, &offset) ||
      !scan_raw_word_upper(lexer->sql, lexer->length, &offset, word, sizeof(word)) ||
      strcmp(word, "TIME") != 0 ||
      !skip_raw_ignored(lexer->sql, lexer->length, &offset) ||
      !scan_raw_word_upper(lexer->sql, lexer->length, &offset, word, sizeof(word)) ||
      strcmp(word, "ZONE") != 0 ||
      !skip_raw_ignored(lexer->sql, lexer->length, &offset)) {
    return 0;
  }

  if (offset < lexer->length &&
      (lexer->sql[offset] == '\'' || lexer->sql[offset] == '"' || lexer->sql[offset] == '`') &&
      skip_raw_quoted(lexer->sql, lexer->length, &offset, lexer->sql[offset])) {
    lexer->offset = offset;
    return 1;
  }
  if (scan_raw_word_upper(lexer->sql, lexer->length, &offset, word, sizeof(word))) {
    lexer->offset = offset;
    return 1;
  }

  return 0;
}

static int skip_create_definer_prefix(MyliteLexer *lexer) {
  size_t offset = lexer->offset;
  char word[128];

  if (!scan_raw_word_upper(lexer->sql, lexer->length, &offset, word, sizeof(word)) ||
      strcmp(word, "DEFINER") != 0) {
    return 0;
  }
  if (!skip_raw_ignored(lexer->sql, lexer->length, &offset) ||
      offset >= lexer->length || lexer->sql[offset] != '=') {
    return 0;
  }
  offset++;

  while (offset < lexer->length) {
    if (!skip_raw_ignored(lexer->sql, lexer->length, &offset)) {
      return 0;
    }
    if (offset >= lexer->length || lexer->sql[offset] == ';') {
      return 0;
    }

    unsigned char ch = (unsigned char)lexer->sql[offset];
    if (ch == '\'' || ch == '"' || ch == '`') {
      if (!skip_raw_quoted(lexer->sql, lexer->length, &offset, (char)ch)) {
        return 0;
      }
      continue;
    }
    if (is_identifier_start(ch)) {
      size_t word_start = offset;
      if (!scan_raw_word_upper(lexer->sql, lexer->length, &offset, word, sizeof(word))) {
        return 0;
      }
      if (strcmp(word, "PROCEDURE") == 0 || strcmp(word, "TRIGGER") == 0) {
        lexer->offset = word_start;
        return 1;
      }
      continue;
    }
    offset++;
  }

  return 0;
}

static void skip_ignored(MyliteLexer *lexer) {
  while (lexer->offset < lexer->length) {
    unsigned char ch = (unsigned char)lexer->sql[lexer->offset];
    if (isspace(ch)) {
      lexer->offset++;
      continue;
    }

    if (ch == '#') {
      while (lexer->offset < lexer->length && lexer->sql[lexer->offset] != '\n') {
        lexer->offset++;
      }
      continue;
    }

    if (ch == '-' && peek(lexer, lexer->offset + 1) == '-') {
      int after = peek(lexer, lexer->offset + 2);
      if (after == 0 || isspace((unsigned char)after)) {
        lexer->offset += 2;
        while (lexer->offset < lexer->length && lexer->sql[lexer->offset] != '\n') {
          lexer->offset++;
        }
        continue;
      }
    }

    if (ch == '/' && peek(lexer, lexer->offset + 1) == '*') {
      int executable_comment_token = 0;
      size_t executable_comment_end = 0;
      if (scan_executable_comment_paren(lexer, lexer->offset, &executable_comment_token,
                                        &executable_comment_end)) {
        return;
      }
      if (!skip_block_comment(lexer)) {
        return;
      }
      continue;
    }

    break;
  }
}

static int scan_executable_comment_paren(const MyliteLexer *lexer, size_t offset,
                                         int *token, size_t *end) {
  if (offset + 4 >= lexer->length || lexer->sql[offset] != '/' ||
      lexer->sql[offset + 1] != '*' || lexer->sql[offset + 2] != '!') {
    return 0;
  }

  size_t cursor = offset + 3;
  while (cursor < lexer->length && isdigit((unsigned char)lexer->sql[cursor])) {
    cursor++;
  }
  while (cursor < lexer->length && isspace((unsigned char)lexer->sql[cursor])) {
    cursor++;
  }
  if (cursor >= lexer->length ||
      (lexer->sql[cursor] != '(' && lexer->sql[cursor] != ')')) {
    return 0;
  }
  *token = lexer->sql[cursor] == '(' ? MYLITE_TOK_LPAREN : MYLITE_TOK_RPAREN;
  cursor++;
  while (cursor < lexer->length && isspace((unsigned char)lexer->sql[cursor])) {
    cursor++;
  }
  if (cursor + 1 >= lexer->length || lexer->sql[cursor] != '*' ||
      lexer->sql[cursor + 1] != '/') {
    return 0;
  }

  *end = cursor + 2;
  return 1;
}

static int skip_block_comment(MyliteLexer *lexer) {
  size_t start = lexer->offset;
  int allow_nested = peek(lexer, lexer->offset + 2) == '!';
  int depth = 1;
  lexer->offset += 2;
  while (lexer->offset + 1 < lexer->length) {
    if (allow_nested && lexer->sql[lexer->offset] == '/' &&
        lexer->sql[lexer->offset + 1] == '*') {
      lexer->offset += 2;
      depth++;
      continue;
    }
    if (lexer->sql[lexer->offset] == '*' && lexer->sql[lexer->offset + 1] == '/') {
      lexer->offset += 2;
      depth--;
      if (depth == 0) {
        return 1;
      }
      continue;
    }
    lexer->offset++;
  }
  set_lex_error(lexer, start);
  return 0;
}

static int find_event_body_range(const char *sql, size_t length, size_t *start, size_t *end) {
  size_t offset = 0;
  int saw_statement_start = 0;
  int saw_event = 0;
  char word[128];

  while (offset < length) {
    if (!skip_raw_ignored(sql, length, &offset)) {
      return 0;
    }
    if (offset >= length || sql[offset] == ';') {
      return 0;
    }

    unsigned char ch = (unsigned char)sql[offset];
    if (ch == '\'' || ch == '"' || ch == '`') {
      if (!skip_raw_quoted(sql, length, &offset, (char)ch)) {
        return 0;
      }
      continue;
    }

    if (is_identifier_start(ch)) {
      if (!scan_raw_word_upper(sql, length, &offset, word, sizeof(word))) {
        return 0;
      }

      if (!saw_statement_start) {
        if (strcmp(word, "CREATE") != 0 && strcmp(word, "ALTER") != 0) {
          return 0;
        }
        saw_statement_start = 1;
        continue;
      }

      if (strcmp(word, "EVENT") == 0) {
        saw_event = 1;
        continue;
      }
      if (saw_event && strcmp(word, "DO") == 0) {
        if (!skip_raw_ignored(sql, length, &offset)) {
          return 0;
        }
        *start = offset;
        *end = find_event_body_end(sql, length, offset);
        return *end > *start;
      }
      continue;
    }

    offset++;
  }

  return 0;
}

static size_t find_event_body_end(const char *sql, size_t length, size_t start) {
  size_t offset = start;
  char word[128];

  if (next_raw_word_upper(sql, length, offset, word, sizeof(word)) &&
      strcmp(word, "BEGIN") == 0) {
    int block_depth = 0;
    while (offset < length) {
      if (!skip_raw_ignored(sql, length, &offset)) {
        return length;
      }
      if (offset >= length) {
        return length;
      }

      unsigned char ch = (unsigned char)sql[offset];
      if (ch == '\'' || ch == '"' || ch == '`') {
        if (!skip_raw_quoted(sql, length, &offset, (char)ch)) {
          return length;
        }
        continue;
      }

      size_t word_offset = offset;
      if (is_identifier_start(ch) &&
          scan_raw_word_upper(sql, length, &offset, word, sizeof(word))) {
        if (strcmp(word, "BEGIN") == 0) {
          block_depth++;
        } else if (strcmp(word, "END") == 0) {
          char next_word[128];
          if (next_raw_word_upper(sql, length, offset, next_word, sizeof(next_word)) &&
              (strcmp(next_word, "CASE") == 0 || strcmp(next_word, "IF") == 0 ||
               strcmp(next_word, "LOOP") == 0 || strcmp(next_word, "REPEAT") == 0 ||
               strcmp(next_word, "WHILE") == 0)) {
            continue;
          }
          block_depth--;
          if (block_depth <= 0) {
            return offset;
          }
        }
        continue;
      }

      offset = word_offset + 1;
    }
    return length;
  }

  while (offset < length) {
    if (!skip_raw_ignored(sql, length, &offset)) {
      return length;
    }
    if (offset >= length || sql[offset] == ';') {
      return offset;
    }

    unsigned char ch = (unsigned char)sql[offset];
    if (ch == '\'' || ch == '"' || ch == '`') {
      if (!skip_raw_quoted(sql, length, &offset, (char)ch)) {
        return length;
      }
      continue;
    }
    offset++;
  }
  return length;
}

static int skip_raw_ignored(const char *sql, size_t length, size_t *offset) {
  while (*offset < length) {
    unsigned char ch = (unsigned char)sql[*offset];
    if (isspace(ch)) {
      (*offset)++;
      continue;
    }
    if (ch == '#') {
      while (*offset < length && sql[*offset] != '\n') {
        (*offset)++;
      }
      continue;
    }
    if (ch == '-' && *offset + 1 < length && sql[*offset + 1] == '-') {
      int after = *offset + 2 < length ? (unsigned char)sql[*offset + 2] : 0;
      if (after == 0 || isspace((unsigned char)after)) {
        *offset += 2;
        while (*offset < length && sql[*offset] != '\n') {
          (*offset)++;
        }
        continue;
      }
    }
    if (ch == '/' && *offset + 1 < length && sql[*offset + 1] == '*') {
      if (!skip_raw_comment(sql, length, offset)) {
        return 0;
      }
      continue;
    }
    break;
  }
  return 1;
}

static int skip_raw_comment(const char *sql, size_t length, size_t *offset) {
  int allow_nested = *offset + 2 < length && sql[*offset + 2] == '!';
  int depth = 1;
  *offset += 2;
  while (*offset + 1 < length) {
    if (allow_nested && sql[*offset] == '/' && sql[*offset + 1] == '*') {
      *offset += 2;
      depth++;
      continue;
    }
    if (sql[*offset] == '*' && sql[*offset + 1] == '/') {
      *offset += 2;
      depth--;
      if (depth == 0) {
        return 1;
      }
      continue;
    }
    (*offset)++;
  }
  return 0;
}

static int skip_raw_quoted(const char *sql, size_t length, size_t *offset, char quote) {
  (*offset)++;
  while (*offset < length) {
    char ch = sql[(*offset)++];
    if (ch == '\\') {
      if (*offset < length) {
        (*offset)++;
      }
      continue;
    }
    if (ch == quote) {
      if (*offset < length && sql[*offset] == quote) {
        (*offset)++;
        continue;
      }
      return 1;
    }
  }
  return 0;
}

static int scan_raw_word_upper(const char *sql, size_t length, size_t *offset, char *buffer,
                               size_t buffer_size) {
  size_t start = *offset;
  if (start >= length || !is_identifier_start((unsigned char)sql[start])) {
    return 0;
  }
  while (*offset < length && is_identifier_continue((unsigned char)sql[*offset])) {
    (*offset)++;
  }

  size_t word_length = *offset - start;
  size_t copy_length = word_length < buffer_size ? word_length : buffer_size - 1;
  for (size_t i = 0; i < copy_length; i++) {
    buffer[i] = (char)toupper((unsigned char)sql[start + i]);
  }
  buffer[copy_length] = '\0';
  return 1;
}

static int next_raw_word_upper(const char *sql, size_t length, size_t offset, char *buffer,
                               size_t buffer_size) {
  if (!skip_raw_ignored(sql, length, &offset)) {
    return 0;
  }
  return scan_raw_word_upper(sql, length, &offset, buffer, buffer_size);
}

static int has_parenthesized_interval_quantity(const MyliteLexer *lexer, size_t offset) {
  if (offset >= lexer->length || lexer->sql[offset] != '(') {
    return 0;
  }

  size_t cursor = offset + 1;
  int depth = 1;
  while (cursor < lexer->length) {
    unsigned char ch = (unsigned char)lexer->sql[cursor];
    if (ch == '\'' || ch == '"' || ch == '`') {
      if (!skip_raw_quoted(lexer->sql, lexer->length, &cursor, (char)ch)) {
        return 0;
      }
      continue;
    }
    if (ch == '/' && cursor + 1 < lexer->length && lexer->sql[cursor + 1] == '*') {
      if (!skip_raw_comment(lexer->sql, lexer->length, &cursor)) {
        return 0;
      }
      continue;
    }
    if (ch == '(') {
      depth++;
    } else if (ch == ')') {
      depth--;
      if (depth == 0) {
        return is_interval_time_unit_at(lexer->sql, lexer->length, cursor + 1);
      }
    }
    cursor++;
  }
  return 0;
}

static int is_interval_time_unit_at(const char *sql, size_t length, size_t offset) {
  char word[32];
  if (!next_raw_word_upper(sql, length, offset, word, sizeof(word))) {
    return 0;
  }

  return strcmp(word, "MICROSECOND") == 0 || strcmp(word, "SECOND") == 0 ||
         strcmp(word, "MINUTE") == 0 || strcmp(word, "HOUR") == 0 ||
         strcmp(word, "DAY") == 0 || strcmp(word, "WEEK") == 0 ||
         strcmp(word, "MONTH") == 0 || strcmp(word, "QUARTER") == 0 ||
         strcmp(word, "YEAR") == 0 || strcmp(word, "SECOND_MICROSECOND") == 0 ||
         strcmp(word, "MINUTE_MICROSECOND") == 0 || strcmp(word, "MINUTE_SECOND") == 0 ||
         strcmp(word, "HOUR_MICROSECOND") == 0 || strcmp(word, "HOUR_SECOND") == 0 ||
         strcmp(word, "HOUR_MINUTE") == 0 || strcmp(word, "DAY_MICROSECOND") == 0 ||
         strcmp(word, "DAY_SECOND") == 0 || strcmp(word, "DAY_MINUTE") == 0 ||
         strcmp(word, "DAY_HOUR") == 0 || strcmp(word, "YEAR_MONTH") == 0 ||
         strcmp(word, "SQL_TSI_SECOND") == 0 || strcmp(word, "SQL_TSI_MINUTE") == 0 ||
         strcmp(word, "SQL_TSI_HOUR") == 0 || strcmp(word, "SQL_TSI_DAY") == 0 ||
         strcmp(word, "SQL_TSI_WEEK") == 0 || strcmp(word, "SQL_TSI_MONTH") == 0 ||
         strcmp(word, "SQL_TSI_QUARTER") == 0 || strcmp(word, "SQL_TSI_YEAR") == 0;
}

static int is_quoted_identifier_context(const MyliteLexer *lexer) {
  switch (lexer->previous_token) {
    case MYLITE_TOK_DOT:
    case MYLITE_TOK_TABLE_KWD:
    case MYLITE_TOK_JOIN:
    case MYLITE_TOK_STRAIGHT_JOIN:
    case MYLITE_TOK_UPDATE:
    case MYLITE_TOK_INTO:
    case MYLITE_TOK_AS:
    case MYLITE_TOK_TABLESPACE:
      return 1;
    case MYLITE_TOK_EQ:
      return lexer->tablespace_name_pending;
    case MYLITE_TOK_FROM:
      return !lexer->prepare_from_source;
  }
  return 0;
}

static int is_quoted_identifier_before_column_type(const MyliteLexer *lexer, size_t offset,
                                                   char quote) {
  if (lexer->previous_token != MYLITE_TOK_LPAREN &&
      lexer->previous_token != MYLITE_TOK_COMMA) {
    return 0;
  }

  size_t cursor = offset;
  if (!skip_raw_quoted(lexer->sql, lexer->length, &cursor, quote)) {
    return 0;
  }
  if (!skip_raw_ignored(lexer->sql, lexer->length, &cursor)) {
    return 0;
  }

  char word[64];
  if (!scan_raw_word_upper(lexer->sql, lexer->length, &cursor, word, sizeof(word))) {
    return 0;
  }
  return is_column_type_start_token(keyword_token(word, 0));
}

static int is_column_type_start_token(int token) {
  return token == MYLITE_TOK_BINARY_TYPE || token == MYLITE_TOK_BIT_TYPE ||
         token == MYLITE_TOK_BLOB_TYPE || token == MYLITE_TOK_BOOL_TYPE ||
         token == MYLITE_TOK_BOOLEAN_TYPE || token == MYLITE_TOK_CHAR_TYPE ||
         token == MYLITE_TOK_DATE_TYPE || token == MYLITE_TOK_DATETIME_TYPE ||
         token == MYLITE_TOK_DECIMAL_TYPE || token == MYLITE_TOK_DOUBLE_TYPE ||
         token == MYLITE_TOK_ENUM || token == MYLITE_TOK_FIXED ||
         token == MYLITE_TOK_FLOAT_TYPE || token == MYLITE_TOK_FLOAT4_TYPE ||
         token == MYLITE_TOK_FLOAT8_TYPE || token == MYLITE_TOK_GEOMETRY ||
         token == MYLITE_TOK_GEOMETRYCOLLECTION || token == MYLITE_TOK_INT_TYPE ||
         token == MYLITE_TOK_INT1_TYPE || token == MYLITE_TOK_INT2_TYPE ||
         token == MYLITE_TOK_INT3_TYPE || token == MYLITE_TOK_INT4_TYPE ||
         token == MYLITE_TOK_INT8_TYPE || token == MYLITE_TOK_INTEGER_TYPE ||
         token == MYLITE_TOK_JSON_TYPE || token == MYLITE_TOK_LONGBLOB_TYPE ||
         token == MYLITE_TOK_LONGTEXT_TYPE || token == MYLITE_TOK_MEDIUMBLOB_TYPE ||
         token == MYLITE_TOK_MEDIUMTEXT_TYPE || token == MYLITE_TOK_NUMERIC_TYPE ||
         token == MYLITE_TOK_SET || token == MYLITE_TOK_TEXT_TYPE ||
         token == MYLITE_TOK_TIME_TYPE || token == MYLITE_TOK_TIMESTAMP_TYPE ||
         token == MYLITE_TOK_TINYBLOB_TYPE || token == MYLITE_TOK_VARBINARY_TYPE ||
         token == MYLITE_TOK_VARCHAR_TYPE || token == MYLITE_TOK_VARCHARACTER ||
         token == MYLITE_TOK_YEAR_TYPE;
}

static int is_persist_system_variable_token(const MyliteLexer *lexer, size_t offset) {
  return has_ascii_ci_prefix(lexer->sql, lexer->length, offset, "@@persist.") ||
         has_ascii_ci_prefix(lexer->sql, lexer->length, offset, "@@persist_only.");
}

static int has_ascii_ci_prefix(const char *sql, size_t length, size_t offset,
                               const char *prefix) {
  for (size_t i = 0; prefix[i] != '\0'; i++) {
    if (offset + i >= length ||
        tolower((unsigned char)sql[offset + i]) !=
            tolower((unsigned char)prefix[i])) {
      return 0;
    }
  }
  return 1;
}

static MyliteToken scan_identifier_or_keyword(MyliteLexer *lexer) {
  size_t start = lexer->offset;
  while (lexer->offset < lexer->length &&
         is_identifier_continue((unsigned char)lexer->sql[lexer->offset])) {
    lexer->offset++;
  }

  size_t length = lexer->offset - start;
  char upper[128];
  if (length >= sizeof(upper)) {
    return make_token(lexer, MYLITE_TOK_IDENTIFIER, start);
  }
  for (size_t i = 0; i < length; i++) {
    upper[i] = (char)toupper((unsigned char)lexer->sql[start + i]);
  }
  upper[length] = '\0';

  if (upper[0] == '_' && is_charset_introducer_context(lexer)) {
    return make_token(lexer, MYLITE_TOK_UNDERSCORE_CS, start);
  }

  int compound_token = 0;
  if (scan_compound_keyword(lexer, upper, &compound_token)) {
    return make_token(lexer, compound_token, start);
  }

  if (is_keyword_context_identifier(lexer)) {
    return make_token(lexer, MYLITE_TOK_IDENTIFIER, start);
  }

  size_t lookahead = lexer->offset;
  while (lookahead < lexer->length && isspace((unsigned char)lexer->sql[lookahead])) {
    lookahead++;
  }
  int token = keyword_token(upper, lookahead < lexer->length && lexer->sql[lookahead] == '(');
  if (token != 0 && lexer->previous_token == MYLITE_TOK_CALL) {
    return make_token(lexer, MYLITE_TOK_IDENTIFIER, start);
  }
  if (token == MYLITE_TOK_CURRENT_ROLE && lexer->previous_token == MYLITE_TOK_AS) {
    return make_token(lexer, MYLITE_TOK_IDENTIFIER, start);
  }
  if (token == MYLITE_TOK_BUILTIN_USER &&
      (lexer->previous_token == MYLITE_TOK_KEY ||
       lexer->previous_token == MYLITE_TOK_INDEX) &&
      lookahead < lexer->length && lexer->sql[lookahead] == '(') {
    return make_token(lexer, MYLITE_TOK_IDENTIFIER, start);
  }
  if (token != 0 && is_table_name_keyword_identifier_context(lexer, token)) {
    return make_token(lexer, MYLITE_TOK_IDENTIFIER, start);
  }
  if (token != 0 && lexer->fetch_into_targets) {
    return make_token(lexer, MYLITE_TOK_IDENTIFIER, start);
  }
  if (token != 0 && is_stored_program_label_identifier_context(lexer, lookahead)) {
    return make_token(lexer, MYLITE_TOK_IDENTIFIER, start);
  }
  return make_token(lexer, token == 0 ? MYLITE_TOK_IDENTIFIER : token, start);
}

static int scan_compound_keyword(MyliteLexer *lexer, const char *first, int *token) {
  for (size_t i = 0; i < mylite_tidb_compound_keyword_count; i++) {
    const MyliteTidbCompoundKeyword *entry = &mylite_tidb_compound_keywords[i];
    if (strcmp(entry->first, first) != 0) {
      continue;
    }

    size_t saved = lexer->offset;
    char second[64];
    char third[64];
    if (!scan_word_upper(lexer->sql, lexer->length, &lexer->offset, second, sizeof(second)) ||
        strcmp(second, entry->second) != 0) {
      lexer->offset = saved;
      continue;
    }

    if (entry->third[0] != '\0') {
      if (!scan_word_upper(lexer->sql, lexer->length, &lexer->offset, third, sizeof(third)) ||
          strcmp(third, entry->third) != 0) {
        lexer->offset = saved;
        continue;
      }
    }

    *token = entry->token;
    return 1;
  }
  return 0;
}

static int scan_word_upper(const char *sql, size_t length, size_t *offset, char *buffer,
                           size_t buffer_size) {
  size_t i = *offset;
  while (i < length && isspace((unsigned char)sql[i])) {
    i++;
  }
  if (i >= length || !is_identifier_start((unsigned char)sql[i])) {
    return 0;
  }

  size_t start = i;
  while (i < length && is_identifier_continue((unsigned char)sql[i])) {
    i++;
  }

  size_t word_length = i - start;
  if (word_length >= buffer_size) {
    return 0;
  }
  for (size_t j = 0; j < word_length; j++) {
    buffer[j] = (char)toupper((unsigned char)sql[start + j]);
  }
  buffer[word_length] = '\0';
  *offset = i;
  return 1;
}

static int keyword_token(const char *keyword, int builtin_context) {
  if (builtin_context) {
    int builtin =
        lookup_keyword_token(mylite_tidb_builtin_keywords, mylite_tidb_builtin_keyword_count,
                             keyword);
    if (builtin != 0) {
      return builtin;
    }
  }
  return lookup_keyword_token(mylite_tidb_keywords, mylite_tidb_keyword_count, keyword);
}

static int lookup_keyword_token(const MyliteTidbKeyword *keywords, size_t keyword_count,
                                const char *keyword) {
  size_t low = 0;
  size_t high = keyword_count;
  while (low < high) {
    size_t mid = low + (high - low) / 2;
    int cmp = strcmp(keyword, keywords[mid].keyword);
    if (cmp == 0) {
      return keywords[mid].token;
    }
    if (cmp < 0) {
      high = mid;
    } else {
      low = mid + 1;
    }
  }
  return 0;
}

static int is_keyword_context_identifier(const MyliteLexer *lexer) {
  if (lexer->previous_token == MYLITE_TOK_DOT) {
    return 1;
  }

  size_t offset = lexer->offset;
  while (offset < lexer->length && isspace((unsigned char)lexer->sql[offset])) {
    offset++;
  }
  return offset < lexer->length && lexer->sql[offset] == '.' &&
         !isdigit((unsigned char)peek(lexer, offset + 1));
}

static int is_table_name_keyword_identifier_context(const MyliteLexer *lexer, int token) {
  return lexer->previous_token == MYLITE_TOK_TABLE_KWD && token != MYLITE_TOK_IF_KWD &&
         token != MYLITE_TOK_FROM && token != MYLITE_TOK_STATUS && token != MYLITE_TOK_WITH;
}

static int is_stored_program_label_identifier_context(const MyliteLexer *lexer,
                                                      size_t lookahead) {
  if (lookahead < lexer->length && lexer->sql[lookahead] == ':') {
    return 1;
  }

  if (lookahead < lexer->length && lexer->sql[lookahead] != ';') {
    return 0;
  }

  return lexer->previous_token == MYLITE_TOK_LEAVE ||
         lexer->previous_token == MYLITE_TOK_ITERATE ||
         lexer->previous_token == MYLITE_TOK_LOOP ||
         lexer->previous_token == MYLITE_TOK_REPEAT ||
         lexer->previous_token == MYLITE_TOK_WHILE;
}

static int is_charset_introducer_context(const MyliteLexer *lexer) {
  size_t offset = lexer->offset;
  while (offset < lexer->length && isspace((unsigned char)lexer->sql[offset])) {
    offset++;
  }

  int ch = peek(lexer, offset);
  if (ch == '\'' || ch == '"') {
    return 1;
  }
  if ((ch == 'x' || ch == 'X' || ch == 'b' || ch == 'B') &&
      peek(lexer, offset + 1) == '\'') {
    return 1;
  }
  if (ch == '0') {
    int next = peek(lexer, offset + 1);
    if ((next == 'x' || next == 'X') && is_hex_digit((unsigned char)peek(lexer, offset + 2))) {
      return 1;
    }
    if ((next == 'b' || next == 'B') && is_bit_digit((unsigned char)peek(lexer, offset + 2))) {
      return 1;
    }
  }
  return 0;
}

static MyliteToken scan_number(MyliteLexer *lexer) {
  size_t start = lexer->offset;
  int has_dot = 0;
  int has_exponent = 0;

  if (lexer->sql[lexer->offset] == '0' &&
      (peek(lexer, lexer->offset + 1) == 'x' || peek(lexer, lexer->offset + 1) == 'X') &&
      is_hex_digit((unsigned char)peek(lexer, lexer->offset + 2))) {
    lexer->offset += 2;
    while (is_hex_digit((unsigned char)peek(lexer, lexer->offset))) {
      lexer->offset++;
    }
    return make_token(lexer, MYLITE_TOK_HEX_LIT, start);
  }

  if (lexer->sql[lexer->offset] == '0' &&
      (peek(lexer, lexer->offset + 1) == 'b' || peek(lexer, lexer->offset + 1) == 'B') &&
      is_bit_digit((unsigned char)peek(lexer, lexer->offset + 2))) {
    lexer->offset += 2;
    while (is_bit_digit((unsigned char)peek(lexer, lexer->offset))) {
      lexer->offset++;
    }
    if (is_identifier_continue((unsigned char)peek(lexer, lexer->offset))) {
      while (is_identifier_continue((unsigned char)peek(lexer, lexer->offset))) {
        lexer->offset++;
      }
      return make_token(lexer, MYLITE_TOK_IDENTIFIER, start);
    }
    return make_token(lexer, MYLITE_TOK_BIT_LIT, start);
  }

  while (isdigit((unsigned char)peek(lexer, lexer->offset))) {
    lexer->offset++;
  }

  if (peek(lexer, lexer->offset) == '.') {
    has_dot = 1;
    lexer->offset++;
    while (isdigit((unsigned char)peek(lexer, lexer->offset))) {
      lexer->offset++;
    }
  }

  int exponent = peek(lexer, lexer->offset);
  if (exponent == 'e' || exponent == 'E') {
    size_t exponent_offset = lexer->offset;
    size_t digit_offset = exponent_offset + 1;
    int sign = peek(lexer, digit_offset);
    if (sign == '+' || sign == '-') {
      digit_offset++;
    }
    if (isdigit((unsigned char)peek(lexer, digit_offset))) {
      has_exponent = 1;
      lexer->offset = digit_offset + 1;
      while (isdigit((unsigned char)peek(lexer, lexer->offset))) {
        lexer->offset++;
      }
    }
  }

  if (!has_dot && !has_exponent &&
      is_identifier_continue((unsigned char)peek(lexer, lexer->offset))) {
    while (is_identifier_continue((unsigned char)peek(lexer, lexer->offset))) {
      lexer->offset++;
    }
    return make_token(lexer, MYLITE_TOK_IDENTIFIER, start);
  }

  if (has_exponent) {
    return make_token(lexer, MYLITE_TOK_FLOAT_LIT, start);
  }
  if (has_dot) {
    return make_token(lexer, MYLITE_TOK_DEC_LIT, start);
  }
  return make_token(lexer, MYLITE_TOK_INT_LIT, start);
}

static MyliteToken scan_string(MyliteLexer *lexer, char quote, int token) {
  size_t start = lexer->offset;
  lexer->offset++;
  while (lexer->offset < lexer->length) {
    char ch = lexer->sql[lexer->offset++];
    if (ch == '\\') {
      if (lexer->offset < lexer->length) {
        lexer->offset++;
      }
      continue;
    }
    if (ch == quote) {
      if (peek(lexer, lexer->offset) == quote) {
        lexer->offset++;
        continue;
      }
      if (quote == '\'' && lexer->previous_token == MYLITE_TOK_TIMESTAMP_TYPE) {
        skip_timestamp_time_zone_suffix(lexer);
      }
      return make_token(lexer, token, start);
    }
  }

  set_lex_error(lexer, start);
  return make_token(lexer, 0, start);
}

static void skip_timestamp_time_zone_suffix(MyliteLexer *lexer) {
  (void)skip_time_zone_suffix(lexer);
}

static MyliteToken scan_backtick_identifier(MyliteLexer *lexer) {
  return scan_quoted_identifier(lexer, '`');
}

static MyliteToken scan_quoted_identifier(MyliteLexer *lexer, char quote) {
  size_t start = lexer->offset;
  lexer->offset++;
  while (lexer->offset < lexer->length) {
    char ch = lexer->sql[lexer->offset++];
    if (ch == quote) {
      if (peek(lexer, lexer->offset) == quote) {
        lexer->offset++;
        continue;
      }
      return make_token(lexer, MYLITE_TOK_IDENTIFIER, start);
    }
  }

  set_lex_error(lexer, start);
  return make_token(lexer, 0, start);
}

static MyliteToken scan_at_identifier(MyliteLexer *lexer) {
  size_t start = lexer->offset;
  lexer->offset++;
  int token = MYLITE_TOK_SINGLE_AT_IDENTIFIER;
  if (peek(lexer, lexer->offset) == '@') {
    lexer->offset++;
    token = MYLITE_TOK_DOUBLE_AT_IDENTIFIER;
  }
  if (token == MYLITE_TOK_SINGLE_AT_IDENTIFIER &&
      (lexer->previous_token == MYLITE_TOK_IDENTIFIER ||
       lexer->previous_token == MYLITE_TOK_STRING_LIT)) {
    int next = peek(lexer, lexer->offset);
    if (next == '\'' || next == '"' || next == '`') {
      return make_token(lexer, MYLITE_TOK_CHAR_40, start);
    }
  }

  if (token == MYLITE_TOK_SINGLE_AT_IDENTIFIER) {
    int quote = peek(lexer, lexer->offset);
    if (quote == '\'' || quote == '"' || quote == '`') {
      lexer->offset++;
      while (lexer->offset < lexer->length) {
        char ch = lexer->sql[lexer->offset++];
        if (ch == '\\' && quote != '`') {
          if (lexer->offset < lexer->length) {
            lexer->offset++;
          }
          continue;
        }
        if (ch == quote) {
          if (peek(lexer, lexer->offset) == quote) {
            lexer->offset++;
            continue;
          }
          return make_token(lexer, token, start);
        }
      }
      set_lex_error(lexer, start);
      return make_token(lexer, 0, start);
    }
  }

  while (lexer->offset < lexer->length &&
         !is_delimiter((unsigned char)lexer->sql[lexer->offset])) {
    lexer->offset++;
  }
  return make_token(lexer, token, start);
}

static MyliteToken make_token(MyliteLexer *lexer, int type, size_t offset) {
  (void)lexer;
  MyliteToken token;
  token.type = type;
  token.offset = offset;
  return token;
}

static void set_lex_error(MyliteLexer *lexer, size_t offset) {
  lexer->lex_error = 1;
  lexer->lex_error_offset = offset;
}

static int is_identifier_start(unsigned char ch) {
  return isalpha(ch) || ch == '_' || ch == '$' || ch >= 0x80;
}

static int is_identifier_continue(unsigned char ch) {
  return isalnum(ch) || ch == '_' || ch == '$' || ch >= 0x80;
}

static int is_hex_digit(unsigned char ch) {
  return isxdigit(ch);
}

static int is_bit_digit(unsigned char ch) {
  return ch == '0' || ch == '1';
}

static int is_delimiter(unsigned char ch) {
  if (ch == 0 || isspace(ch)) {
    return 1;
  }
  switch (ch) {
    case '(':
    case ')':
    case ',':
    case ';':
    case '+':
    case '-':
    case '*':
    case '/':
    case '%':
    case '^':
    case '~':
    case '=':
    case '<':
    case '>':
    case '!':
    case '&':
    case '|':
    case '?':
      return 1;
  }
  return 0;
}

static int peek(const MyliteLexer *lexer, size_t offset) {
  if (offset >= lexer->length) {
    return 0;
  }
  return (unsigned char)lexer->sql[offset];
}
