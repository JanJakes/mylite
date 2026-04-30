#include "mylite_lexer.h"

#include <ctype.h>

#include "generated/mylite_lemon.h"

static int lexer_at_end(const MyliteLexer *lexer);
static unsigned char lexer_peek(const MyliteLexer *lexer, size_t ahead);
static unsigned char lexer_advance(MyliteLexer *lexer);
static void lexer_skip_space_and_comments(MyliteLexer *lexer);
static int lexer_skip_line_comment(MyliteLexer *lexer);
static int lexer_enter_executable_comment(MyliteLexer *lexer);
static int lexer_skip_block_comment(MyliteLexer *lexer);
static int lexer_skip_until_block_comment_end(MyliteLexer *lexer);
static int lexer_string(MyliteLexer *lexer, MyliteToken *token,
                        unsigned char quote);
static int lexer_quoted_identifier(MyliteLexer *lexer, MyliteToken *token);
static int lexer_dollar_quoted_string(MyliteLexer *lexer,
                                      MyliteToken *token);
static int lexer_number(MyliteLexer *lexer, MyliteToken *token);
static int lexer_identifier(MyliteLexer *lexer, MyliteToken *token);
static int lexer_operator(MyliteLexer *lexer, MyliteToken *token);
static int lexer_semicolon(MyliteLexer *lexer, MyliteToken *token);
static void lexer_note_token(MyliteLexer *lexer, int token_id);
static int is_routine_object_token(int token_id);
static int is_nonroutine_create_object_token(int token_id);
static int is_compound_opener_token(int token_id);
static int keyword_token(const MyliteToken *token);
static int keyword_equals(const MyliteToken *token, const char *keyword);
static unsigned char ascii_upper(unsigned char c);
static int is_identifier_start(unsigned char c);
static int is_identifier_continue(unsigned char c);
static int is_executable_comment_start(const MyliteLexer *lexer);
static int is_line_comment_start(const MyliteLexer *lexer);
static void token_start(MyliteLexer *lexer, MyliteToken *token);

void mylite_lexer_init(MyliteLexer *lexer, const char *sql, size_t length,
                       MyliteParseResult *result) {
  lexer->sql = sql;
  lexer->length = length;
  lexer->offset = 0;
  lexer->line = 1;
  lexer->column = 1;
  lexer->create_scan = 0;
  lexer->in_compound_definition = 0;
  lexer->compound_depth = 0;
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
      return lexer_semicolon(lexer, token);
    case ':':
      lexer_advance(lexer);
      if (lexer_peek(lexer, 0) == '=') {
        lexer_advance(lexer);
      }
      token->length = lexer->offset - token->offset;
      return ML_ATOM;
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

    if (is_executable_comment_start(lexer)) {
      consumed = lexer_enter_executable_comment(lexer);
      continue;
    }

    if (lexer_peek(lexer, 0) == '/' && lexer_peek(lexer, 1) == '*') {
      consumed = lexer_skip_block_comment(lexer);
      continue;
    }

    if (lexer_peek(lexer, 0) == '*' && lexer_peek(lexer, 1) == '/') {
      lexer_advance(lexer);
      lexer_advance(lexer);
      consumed = 1;
    }
  } while (consumed && !lexer_at_end(lexer));
}

static int lexer_skip_line_comment(MyliteLexer *lexer) {
  while (!lexer_at_end(lexer) && lexer_peek(lexer, 0) != '\n') {
    lexer_advance(lexer);
  }
  return 1;
}

static int lexer_enter_executable_comment(MyliteLexer *lexer) {
  unsigned long version = 0;
  size_t digits = 0;

  lexer_advance(lexer);
  lexer_advance(lexer);
  lexer_advance(lexer);

  while (digits < 6 && isdigit(lexer_peek(lexer, 0))) {
    version = version * 10 + (unsigned long) (lexer_peek(lexer, 0) - '0');
    lexer_advance(lexer);
    digits++;
  }

  if (digits == 0 || version <= 80409) {
    return 1;
  }

  return lexer_skip_until_block_comment_end(lexer);
}

static int lexer_skip_block_comment(MyliteLexer *lexer) {
  lexer_advance(lexer);
  lexer_advance(lexer);

  return lexer_skip_until_block_comment_end(lexer);
}

static int lexer_skip_until_block_comment_end(MyliteLexer *lexer) {
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
  int keyword;

  lexer_advance(lexer);
  while (is_identifier_continue(lexer_peek(lexer, 0))) {
    lexer_advance(lexer);
  }

  if (lexer_peek(lexer, 0) == ':' && lexer_peek(lexer, 1) != '=') {
    lexer_advance(lexer);
    token->length = lexer->offset - token->offset;
    return ML_LABEL;
  }

  token->length = lexer->offset - token->offset;
  keyword = keyword_token(token);
  if (keyword == 0) {
    return ML_ATOM;
  }

  lexer_note_token(lexer, keyword);
  return keyword;
}

static int lexer_operator(MyliteLexer *lexer, MyliteToken *token) {
  lexer_advance(lexer);
  token->length = lexer->offset - token->offset;
  return ML_ATOM;
}

static int lexer_semicolon(MyliteLexer *lexer, MyliteToken *token) {
  lexer_advance(lexer);
  token->length = 1;

  if (lexer->in_compound_definition && lexer->compound_depth > 0) {
    return ML_ATOM;
  }

  lexer->in_compound_definition = 0;
  lexer->create_scan = 0;
  return ML_SEMI;
}

static void lexer_note_token(MyliteLexer *lexer, int token_id) {
  if (token_id == ML_CREATE) {
    lexer->create_scan = 32;
    return;
  }

  if (lexer->create_scan > 0) {
    if (is_routine_object_token(token_id)) {
      lexer->in_compound_definition = 1;
      lexer->compound_depth = 0;
      lexer->create_scan = 0;
    } else if (is_nonroutine_create_object_token(token_id)) {
      lexer->create_scan = 0;
    } else {
      lexer->create_scan--;
    }
  }

  if (!lexer->in_compound_definition) {
    return;
  }

  if (is_compound_opener_token(token_id)) {
    lexer->compound_depth++;
  } else if (token_id == ML_END && lexer->compound_depth > 0) {
    lexer->compound_depth--;
  }
}

static int is_routine_object_token(int token_id) {
  return token_id == ML_PROCEDURE || token_id == ML_FUNCTION ||
         token_id == ML_TRIGGER || token_id == ML_EVENT;
}

static int is_nonroutine_create_object_token(int token_id) {
  return token_id == ML_TABLE || token_id == ML_VIEW || token_id == ML_INDEX ||
         token_id == ML_DATABASE || token_id == ML_SCHEMA ||
         token_id == ML_USER || token_id == ML_ROLE ||
         token_id == ML_TABLESPACE || token_id == ML_SERVER;
}

static int is_compound_opener_token(int token_id) {
  return token_id == ML_BEGIN || token_id == ML_IF || token_id == ML_LOOP ||
         token_id == ML_REPEAT || token_id == ML_WHILE || token_id == ML_CASE;
}

static int keyword_token(const MyliteToken *token) {
  static const struct {
    const char *keyword;
    int token;
  } keywords[] = {
      {"ALTER", ML_ALTER},
      {"AGGREGATE", ML_AGGREGATE},
      {"ALGORITHM", ML_ALGORITHM},
      {"ANALYZE", ML_ANALYZE},
      {"BEGIN", ML_BEGIN},
      {"BINLOG", ML_BINLOG},
      {"CACHE", ML_CACHE},
      {"CALL", ML_CALL},
      {"CASE", ML_CASE},
      {"CHANGE", ML_CHANGE},
      {"CHECK", ML_CHECK},
      {"CHECKSUM", ML_CHECKSUM},
      {"CLONE", ML_CLONE},
      {"CLOSE", ML_CLOSE},
      {"COMMIT", ML_COMMIT},
      {"CREATE", ML_CREATE},
      {"DATABASE", ML_DATABASE},
      {"DEFINER", ML_DEFINER},
      {"DECLARE", ML_DECLARE},
      {"DEALLOCATE", ML_DEALLOCATE},
      {"DELETE", ML_DELETE},
      {"DESC", ML_DESC},
      {"DESCRIBE", ML_DESCRIBE},
      {"DO", ML_DO},
      {"DROP", ML_DROP},
      {"ELSE", ML_ELSE},
      {"ELSEIF", ML_ELSEIF},
      {"END", ML_END},
      {"EVENT", ML_EVENT},
      {"EXECUTE", ML_EXECUTE},
      {"EXPLAIN", ML_EXPLAIN},
      {"FETCH", ML_FETCH},
      {"FLUSH", ML_FLUSH},
      {"FROM", ML_FROM},
      {"FUNCTION", ML_FUNCTION},
      {"FULLTEXT", ML_FULLTEXT},
      {"GET", ML_GET},
      {"GRANT", ML_GRANT},
      {"HANDLER", ML_HANDLER},
      {"HAVING", ML_HAVING},
      {"HELP", ML_HELP},
      {"IF", ML_IF},
      {"IMPORT", ML_IMPORT},
      {"INDEX", ML_INDEX},
      {"INSERT", ML_INSERT},
      {"INSTALL", ML_INSTALL},
      {"ITERATE", ML_ITERATE},
      {"KILL", ML_KILL},
      {"LEAVE", ML_LEAVE},
      {"LOAD", ML_LOAD},
      {"LOCK", ML_LOCK},
      {"LOGFILE", ML_LOGFILE},
      {"LOOP", ML_LOOP},
      {"OPEN", ML_OPEN},
      {"OPTIMIZE", ML_OPTIMIZE},
      {"OR", ML_OR},
      {"PURGE", ML_PURGE},
      {"PREPARE", ML_PREPARE},
      {"PROCEDURE", ML_PROCEDURE},
      {"RELEASE", ML_RELEASE},
      {"RENAME", ML_RENAME},
      {"REPAIR", ML_REPAIR},
      {"REPEAT", ML_REPEAT},
      {"REPLACE", ML_REPLACE},
      {"RESET", ML_RESET},
      {"RESIGNAL", ML_RESIGNAL},
      {"RESOURCE", ML_RESOURCE},
      {"RESTART", ML_RESTART},
      {"RETURN", ML_RETURN},
      {"REVOKE", ML_REVOKE},
      {"ROLE", ML_ROLE},
      {"ROLLBACK", ML_ROLLBACK},
      {"SAVEPOINT", ML_SAVEPOINT},
      {"SCHEMA", ML_SCHEMA},
      {"SELECT", ML_SELECT},
      {"SERVER", ML_SERVER},
      {"SECURITY", ML_SECURITY},
      {"SET", ML_SET},
      {"SHOW", ML_SHOW},
      {"SHUTDOWN", ML_SHUTDOWN},
      {"SIGNAL", ML_SIGNAL},
      {"SQL", ML_SQL},
      {"SPATIAL", ML_SPATIAL},
      {"START", ML_START},
      {"TABLE", ML_TABLE},
      {"TABLES", ML_TABLES},
      {"TABLESPACE", ML_TABLESPACE},
      {"TEMPORARY", ML_TEMPORARY},
      {"TRIGGER", ML_TRIGGER},
      {"TRUNCATE", ML_TRUNCATE},
      {"UNTIL", ML_UNTIL},
      {"UNINSTALL", ML_UNINSTALL},
      {"UNLOCK", ML_UNLOCK},
      {"UPDATE", ML_UPDATE},
      {"USE", ML_USE},
      {"USER", ML_USER},
      {"UNDO", ML_UNDO},
      {"UNIQUE", ML_UNIQUE},
      {"VALUES", ML_VALUES},
      {"VIEW", ML_VIEW},
      {"WHEN", ML_WHEN},
      {"WHILE", ML_WHILE},
      {"WITH", ML_WITH},
      {"XA", ML_XA},
  };
  size_t i;

  for (i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
    if (keyword_equals(token, keywords[i].keyword)) {
      return keywords[i].token;
    }
  }

  return 0;
}

static int keyword_equals(const MyliteToken *token, const char *keyword) {
  size_t i = 0;

  while (i < token->length && keyword[i] != '\0') {
    if (ascii_upper((unsigned char) token->start[i]) !=
        (unsigned char) keyword[i]) {
      return 0;
    }
    i++;
  }

  return i == token->length && keyword[i] == '\0';
}

static unsigned char ascii_upper(unsigned char c) {
  if (c >= 'a' && c <= 'z') {
    return (unsigned char) (c - ('a' - 'A'));
  }
  return c;
}

static int is_identifier_start(unsigned char c) {
  return isalpha(c) || c == '_' || c >= 0x80;
}

static int is_identifier_continue(unsigned char c) {
  return isalnum(c) || c == '_' || c == '$' || c >= 0x80;
}

static int is_executable_comment_start(const MyliteLexer *lexer) {
  return lexer_peek(lexer, 0) == '/' && lexer_peek(lexer, 1) == '*' &&
         lexer_peek(lexer, 2) == '!';
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
