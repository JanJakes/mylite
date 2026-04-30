#include "mylite_lexer.h"

#include <ctype.h>

#include "generated/mylite_lemon.h"

static int lexer_at_end(const MyliteLexer *lexer);
static unsigned char lexer_peek(const MyliteLexer *lexer, size_t ahead);
static unsigned char lexer_advance(MyliteLexer *lexer);
static void lexer_skip_space_and_comments(MyliteLexer *lexer);
static int lexer_skip_line_comment(MyliteLexer *lexer);
static int lexer_skip_block_comment(MyliteLexer *lexer);
static int lexer_string(MyliteLexer *lexer, MyliteToken *token,
                        unsigned char quote);
static int lexer_quoted_identifier(MyliteLexer *lexer, MyliteToken *token);
static int lexer_dollar_quoted_string(MyliteLexer *lexer,
                                      MyliteToken *token);
static int lexer_number(MyliteLexer *lexer, MyliteToken *token);
static int lexer_identifier(MyliteLexer *lexer, MyliteToken *token);
static int lexer_operator(MyliteLexer *lexer, MyliteToken *token);
static int is_identifier_start(unsigned char c);
static int is_identifier_continue(unsigned char c);
static int is_line_comment_start(const MyliteLexer *lexer);
static void token_start(MyliteLexer *lexer, MyliteToken *token);

void mylite_lexer_init(MyliteLexer *lexer, const char *sql, size_t length,
                       MyliteParseResult *result) {
  lexer->sql = sql;
  lexer->length = length;
  lexer->offset = 0;
  lexer->line = 1;
  lexer->column = 1;
  lexer->result = result;
}

int mylite_lexer_next(MyliteLexer *lexer, MyliteToken *token) {
  unsigned char c;

  lexer_skip_space_and_comments(lexer);
  token_start(lexer, token);

  if (lexer_at_end(lexer)) {
    return 0;
  }

  c = lexer_peek(lexer, 0);

  switch (c) {
    case '(':
      lexer_advance(lexer);
      token->length = 1;
      return ML_LP;
    case ')':
      lexer_advance(lexer);
      token->length = 1;
      return ML_RP;
    case '[':
      lexer_advance(lexer);
      token->length = 1;
      return ML_LB;
    case ']':
      lexer_advance(lexer);
      token->length = 1;
      return ML_RB;
    case '{':
      lexer_advance(lexer);
      token->length = 1;
      return ML_LC;
    case '}':
      lexer_advance(lexer);
      token->length = 1;
      return ML_RC;
    case ';':
      lexer_advance(lexer);
      token->length = 1;
      return ML_SEMI;
    case '\'':
    case '"':
      return lexer_string(lexer, token, c);
    case '`':
      return lexer_quoted_identifier(lexer, token);
    case '$':
      if (lexer_dollar_quoted_string(lexer, token)) {
        return ML_ATOM;
      }
      return lexer_operator(lexer, token);
    default:
      break;
  }

  if (isdigit(c)) {
    return lexer_number(lexer, token);
  }

  if (is_identifier_start(c)) {
    return lexer_identifier(lexer, token);
  }

  return lexer_operator(lexer, token);
}

static int lexer_at_end(const MyliteLexer *lexer) {
  return lexer->offset >= lexer->length;
}

static unsigned char lexer_peek(const MyliteLexer *lexer, size_t ahead) {
  size_t offset = lexer->offset + ahead;
  if (offset >= lexer->length) {
    return '\0';
  }
  return (unsigned char) lexer->sql[offset];
}

static unsigned char lexer_advance(MyliteLexer *lexer) {
  unsigned char c = lexer_peek(lexer, 0);
  if (c == '\0') {
    return c;
  }

  lexer->offset++;
  if (c == '\n') {
    lexer->line++;
    lexer->column = 1;
  } else {
    lexer->column++;
  }

  return c;
}

static void lexer_skip_space_and_comments(MyliteLexer *lexer) {
  int consumed;

  do {
    consumed = 0;
    while (isspace(lexer_peek(lexer, 0))) {
      lexer_advance(lexer);
      consumed = 1;
    }

    if (lexer_peek(lexer, 0) == '#' || is_line_comment_start(lexer)) {
      consumed = lexer_skip_line_comment(lexer);
      continue;
    }

    if (lexer_peek(lexer, 0) == '/' && lexer_peek(lexer, 1) == '*') {
      consumed = lexer_skip_block_comment(lexer);
    }
  } while (consumed && !lexer_at_end(lexer));
}

static int lexer_skip_line_comment(MyliteLexer *lexer) {
  while (!lexer_at_end(lexer) && lexer_peek(lexer, 0) != '\n') {
    lexer_advance(lexer);
  }
  return 1;
}

static int lexer_skip_block_comment(MyliteLexer *lexer) {
  lexer_advance(lexer);
  lexer_advance(lexer);

  while (!lexer_at_end(lexer)) {
    if (lexer_peek(lexer, 0) == '*' && lexer_peek(lexer, 1) == '/') {
      lexer_advance(lexer);
      lexer_advance(lexer);
      return 1;
    }
    lexer_advance(lexer);
  }

  return 1;
}

static int lexer_string(MyliteLexer *lexer, MyliteToken *token,
                        unsigned char quote) {
  lexer_advance(lexer);

  while (!lexer_at_end(lexer)) {
    unsigned char c = lexer_advance(lexer);

    if (c == '\\' && !lexer_at_end(lexer)) {
      lexer_advance(lexer);
      continue;
    }

    if (c == quote) {
      if (lexer_peek(lexer, 0) == quote) {
        lexer_advance(lexer);
        continue;
      }
      token->length = lexer->offset - token->offset;
      return ML_ATOM;
    }
  }

  token->length = lexer->offset - token->offset;
  return ML_ATOM;
}

static int lexer_quoted_identifier(MyliteLexer *lexer, MyliteToken *token) {
  lexer_advance(lexer);

  while (!lexer_at_end(lexer)) {
    unsigned char c = lexer_advance(lexer);
    if (c == '`') {
      if (lexer_peek(lexer, 0) == '`') {
        lexer_advance(lexer);
        continue;
      }
      token->length = lexer->offset - token->offset;
      return ML_ATOM;
    }
  }

  token->length = lexer->offset - token->offset;
  return ML_ATOM;
}

static int lexer_dollar_quoted_string(MyliteLexer *lexer,
                                      MyliteToken *token) {
  size_t tag_start;
  size_t tag_length;
  size_t search;

  if (lexer_peek(lexer, 0) != '$') {
    return 0;
  }

  tag_start = lexer->offset + 1;
  tag_length = 0;
  while (tag_start + tag_length < lexer->length &&
         is_identifier_continue((unsigned char) lexer->sql[tag_start + tag_length])) {
    tag_length++;
  }

  if (tag_start + tag_length >= lexer->length ||
      lexer->sql[tag_start + tag_length] != '$') {
    return 0;
  }

  while (lexer->offset <= tag_start + tag_length) {
    lexer_advance(lexer);
  }

  search = lexer->offset;
  while (search < lexer->length) {
    size_t i;
    if (lexer->sql[search] != '$') {
      lexer_advance(lexer);
      search = lexer->offset;
      continue;
    }

    for (i = 0; i < tag_length; i++) {
      if (search + 1 + i >= lexer->length ||
          lexer->sql[search + 1 + i] != lexer->sql[tag_start + i]) {
        break;
      }
    }

    if (i == tag_length && search + 1 + tag_length < lexer->length &&
        lexer->sql[search + 1 + tag_length] == '$') {
      while (lexer->offset <= search + 1 + tag_length) {
        lexer_advance(lexer);
      }
      token->length = lexer->offset - token->offset;
      return 1;
    }

    lexer_advance(lexer);
    search = lexer->offset;
  }

  token->length = lexer->offset - token->offset;
  return 1;
}

static int lexer_number(MyliteLexer *lexer, MyliteToken *token) {
  while (isalnum(lexer_peek(lexer, 0)) || lexer_peek(lexer, 0) == '.' ||
         lexer_peek(lexer, 0) == '_' || lexer_peek(lexer, 0) == '+' ||
         lexer_peek(lexer, 0) == '-') {
    lexer_advance(lexer);
  }

  token->length = lexer->offset - token->offset;
  return ML_ATOM;
}

static int lexer_identifier(MyliteLexer *lexer, MyliteToken *token) {
  lexer_advance(lexer);
  while (is_identifier_continue(lexer_peek(lexer, 0))) {
    lexer_advance(lexer);
  }

  token->length = lexer->offset - token->offset;
  return ML_ATOM;
}

static int lexer_operator(MyliteLexer *lexer, MyliteToken *token) {
  lexer_advance(lexer);
  token->length = lexer->offset - token->offset;
  return ML_ATOM;
}

static int is_identifier_start(unsigned char c) {
  return isalpha(c) || c == '_' || c >= 0x80;
}

static int is_identifier_continue(unsigned char c) {
  return isalnum(c) || c == '_' || c == '$' || c >= 0x80;
}

static int is_line_comment_start(const MyliteLexer *lexer) {
  unsigned char next;

  if (lexer_peek(lexer, 0) != '-' || lexer_peek(lexer, 1) != '-') {
    return 0;
  }

  next = lexer_peek(lexer, 2);
  return next == '\0' || isspace(next);
}

static void token_start(MyliteLexer *lexer, MyliteToken *token) {
  token->start = lexer->sql + lexer->offset;
  token->length = 0;
  token->offset = lexer->offset;
  token->line = lexer->line;
  token->column = lexer->column;
}
