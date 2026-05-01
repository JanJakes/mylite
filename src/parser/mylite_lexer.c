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
static int lexer_prefixed_string(MyliteLexer *lexer, MyliteToken *token);
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
    case ',':
      lexer_advance(lexer);
      token->length = 1;
      return ML_COMMA;
    case ':':
      lexer_advance(lexer);
      if (lexer_peek(lexer, 0) == '=') {
        lexer_advance(lexer);
        token->length = lexer->offset - token->offset;
        return ML_ASSIGN;
      }
      token->length = lexer->offset - token->offset;
      return ML_COLON;
    case '.':
      lexer_advance(lexer);
      token->length = 1;
      return ML_DOT;
    case '\'':
    case '"':
      return lexer_string(lexer, token, c);
    case '`':
      return lexer_quoted_identifier(lexer, token);
    case '$':
      if (lexer_dollar_quoted_string(lexer, token)) {
        return ML_ATOM;
      }
      if (is_identifier_continue(lexer_peek(lexer, 1))) {
        return lexer_identifier(lexer, token);
      }
      return lexer_operator(lexer, token);
    case '@':
      if (is_identifier_continue(lexer_peek(lexer, 1))) {
        lexer_advance(lexer);
        while (is_identifier_continue(lexer_peek(lexer, 0))) {
          lexer_advance(lexer);
        }
        token->length = lexer->offset - token->offset;
        return ML_AT_HOST;
      }
      lexer_advance(lexer);
      token->length = 1;
      if (lexer_peek(lexer, 0) == '\'' || lexer_peek(lexer, 0) == '"' ||
          lexer_peek(lexer, 0) == '`') {
        return ML_AT_SIGN;
      }
      return ML_AT_EMPTY;
    default:
      break;
  }

  if (isdigit(c) ||
      ((c == '-' || c == '+') && isdigit(lexer_peek(lexer, 1)))) {
    return lexer_number(lexer, token);
  }

  if (lexer_prefixed_string(lexer, token)) {
    return ML_ATOM;
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

static int lexer_prefixed_string(MyliteLexer *lexer, MyliteToken *token) {
  unsigned char c = lexer_peek(lexer, 0);

  if ((c != 'b' && c != 'B' && c != 'x' && c != 'X') ||
      lexer_peek(lexer, 1) != '\'') {
    return 0;
  }

  lexer_advance(lexer);
  return lexer_string(lexer, token, '\'') == ML_ATOM;
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
      if (quote == '"') {
        return ML_DOUBLE_QUOTED_STRING;
      }
      return ML_ATOM;
    }
  }

  token->length = lexer->offset - token->offset;
  if (quote == '"') {
    return ML_DOUBLE_QUOTED_STRING;
  }
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
      return ML_QUOTED_ID;
    }
  }

  token->length = lexer->offset - token->offset;
  return ML_QUOTED_ID;
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
  if (token->length == 1 &&
      (token->start[0] == '2' || token->start[0] == '3')) {
    return ML_FACTOR_NUMBER;
  }

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
  unsigned char c = lexer_peek(lexer, 0);

  lexer_advance(lexer);
  token->length = lexer->offset - token->offset;

  if (c == '=') {
    return ML_EQUALS;
  }
  if (c == '-') {
    return ML_MINUS;
  }
  if (c == '*') {
    return ML_STAR;
  }

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
      {"ACCOUNT", ML_ACCOUNT},
      {"ADMIN", ML_ADMIN},
      {"ALTER", ML_ALTER},
      {"AGGREGATE", ML_AGGREGATE},
      {"ACTIVE", ML_ACTIVE},
      {"ADD", ML_ADD},
      {"ALL", ML_ALL},
      {"AFTER", ML_AFTER},
      {"ALGORITHM", ML_ALGORITHM},
      {"ANALYZE", ML_ANALYZE},
      {"AND", ML_AND},
      {"ASC", ML_ASC},
      {"AS", ML_AS},
      {"AT", ML_AT},
      {"AUTOEXTEND_SIZE", ML_AUTOEXTEND_SIZE},
      {"ATTRIBUTE", ML_ATTRIBUTE},
      {"AUTHENTICATION", ML_AUTHENTICATION},
      {"AUTO_INCREMENT", ML_AUTO_INCREMENT},
      {"AVG_ROW_LENGTH", ML_AVG_ROW_LENGTH},
      {"BACKUP", ML_BACKUP},
      {"BEFORE", ML_BEFORE},
      {"BEGIN", ML_BEGIN},
      {"BINARY", ML_BINARY},
      {"BINLOG", ML_BINLOG},
      {"BLOCK", ML_BLOCK},
      {"BY", ML_BY},
      {"BUCKETS", ML_BUCKETS},
      {"CACHE", ML_CACHE},
      {"CALL", ML_CALL},
      {"CATALOG_NAME", ML_CATALOG_NAME},
      {"CASCADE", ML_CASCADE},
      {"CASE", ML_CASE},
      {"CHANGE", ML_CHANGE},
      {"CHANGED", ML_CHANGED},
      {"CHANNEL", ML_CHANNEL},
      {"CHAIN", ML_CHAIN},
      {"CHALLENGE_RESPONSE", ML_CHALLENGE_RESPONSE},
      {"CHARACTER", ML_CHARACTER},
      {"CHARSET", ML_CHARSET},
      {"CHECK", ML_CHECK},
      {"CHECKSUM", ML_CHECKSUM},
      {"CIPHER", ML_CIPHER},
      {"CLASS_ORIGIN", ML_CLASS_ORIGIN},
      {"CLONE", ML_CLONE},
      {"CLOSE", ML_CLOSE},
      {"COALESCE", ML_COALESCE},
      {"CODE", ML_CODE},
      {"COLLATE", ML_COLLATE},
      {"COLLATION", ML_COLLATION},
      {"COLUMNS", ML_COLUMNS},
      {"COLUMN_NAME", ML_COLUMN_NAME},
      {"COMMIT", ML_COMMIT},
      {"COMMITTED", ML_COMMITTED},
      {"COMMENT", ML_COMMENT},
      {"COMPLETION", ML_COMPLETION},
      {"COMPRESSION", ML_COMPRESSION},
      {"COMPONENT", ML_COMPONENT},
      {"CONCURRENT", ML_CONCURRENT},
      {"CONDITION", ML_CONDITION},
      {"CONNECTION", ML_CONNECTION},
      {"CONSISTENT", ML_CONSISTENT},
      {"CONSTRAINT_CATALOG", ML_CONSTRAINT_CATALOG},
      {"CONSTRAINT_NAME", ML_CONSTRAINT_NAME},
      {"CONSTRAINT_SCHEMA", ML_CONSTRAINT_SCHEMA},
      {"CONTAINS", ML_CONTAINS},
      {"CONTEXT", ML_CONTEXT},
      {"CONTINUE", ML_CONTINUE},
      {"CONVERT", ML_CONVERT},
      {"COPY", ML_COPY},
      {"COUNT", ML_COUNT},
      {"CREATE", ML_CREATE},
      {"CPU", ML_CPU},
      {"CURRENT", ML_CURRENT},
      {"CURSOR", ML_CURSOR},
      {"CURSOR_NAME", ML_CURSOR_NAME},
      {"CURRENT_USER", ML_CURRENT_USER},
      {"DATA", ML_DATA},
      {"DATAFILE", ML_DATAFILE},
      {"DATABASE", ML_DATABASE},
      {"DATABASES", ML_DATABASES},
      {"DAY", ML_DAY},
      {"DECIMAL", ML_DECIMAL},
      {"DEFAULT", ML_DEFAULT},
      {"DEFAULT_AUTH", ML_DEFAULT_AUTH},
      {"DEFINITION", ML_DEFINITION},
      {"DELAYED", ML_DELAYED},
      {"DELAY_KEY_WRITE", ML_DELAY_KEY_WRITE},
      {"DEFINER", ML_DEFINER},
      {"DECLARE", ML_DECLARE},
      {"DEALLOCATE", ML_DEALLOCATE},
      {"DELETE", ML_DELETE},
      {"DETERMINISTIC", ML_DETERMINISTIC},
      {"DESC", ML_DESC},
      {"DESCRIBE", ML_DESCRIBE},
      {"DESCRIPTION", ML_DESCRIPTION},
      {"DIAGNOSTICS", ML_DIAGNOSTICS},
      {"DIRECTORY", ML_DIRECTORY},
      {"DISABLE", ML_DISABLE},
      {"DISCARD", ML_DISCARD},
      {"DISTINCT", ML_DISTINCT},
      {"DISTINCTROW", ML_DISTINCTROW},
      {"DO", ML_DO},
      {"DROP", ML_DROP},
      {"DUPLICATE", ML_DUPLICATE},
      {"EACH", ML_EACH},
      {"ELSE", ML_ELSE},
      {"ELSEIF", ML_ELSEIF},
      {"END", ML_END},
      {"ENABLE", ML_ENABLE},
      {"ENGINE", ML_ENGINE},
      {"ENGINE_ATTRIBUTE", ML_ENGINE_ATTRIBUTE},
      {"ENCLOSED", ML_ENCLOSED},
      {"ENGINES", ML_ENGINES},
      {"ENCRYPTION", ML_ENCRYPTION},
      {"ESCAPED", ML_ESCAPED},
      {"ERROR", ML_ERROR},
      {"ERRORS", ML_ERRORS},
      {"EVENT", ML_EVENT},
      {"EVENTS", ML_EVENTS},
      {"EVERY", ML_EVERY},
      {"EXPIRE", ML_EXPIRE},
      {"EXCHANGE", ML_EXCHANGE},
      {"EXECUTE", ML_EXECUTE},
      {"EXCEPT", ML_EXCEPT},
      {"EXCLUSIVE", ML_EXCLUSIVE},
      {"EXISTS", ML_EXISTS},
      {"EXIT", ML_EXIT},
      {"EXPLAIN", ML_EXPLAIN},
      {"EXPORT", ML_EXPORT},
      {"EXTENT_SIZE", ML_EXTENT_SIZE},
      {"EXTENDED", ML_EXTENDED},
      {"FAST", ML_FAST},
      {"FAULTS", ML_FAULTS},
      {"FACTOR", ML_FACTOR},
      {"FAILED_LOGIN_ATTEMPTS", ML_FAILED_LOGIN_ATTEMPTS},
      {"FETCH", ML_FETCH},
      {"FILE_BLOCK_SIZE", ML_FILE_BLOCK_SIZE},
      {"FIELDS", ML_FIELDS},
      {"FILTER", ML_FILTER},
      {"FINISH", ML_FINISH},
      {"FIRST", ML_FIRST},
      {"FLUSH", ML_FLUSH},
      {"FOLLOWS", ML_FOLLOWS},
      {"FOR", ML_FOR},
      {"FORCE", ML_FORCE},
      {"FORMAT", ML_FORMAT},
      {"FROM", ML_FROM},
      {"FOREIGN", ML_FOREIGN},
      {"FOUND", ML_FOUND},
      {"FULL", ML_FULL},
      {"GENERAL", ML_GENERAL},
      {"FUNCTION", ML_FUNCTION},
      {"FULLTEXT", ML_FULLTEXT},
      {"GET", ML_GET},
      {"GLOBAL", ML_GLOBAL},
      {"GRANT", ML_GRANT},
      {"GRANTS", ML_GRANTS},
      {"GROUP", ML_GROUP},
      {"GROUP_REPLICATION", ML_GROUP_REPLICATION},
      {"GTIDS", ML_GTIDS},
      {"HANDLER", ML_HANDLER},
      {"HAVING", ML_HAVING},
      {"HELP", ML_HELP},
      {"HIGH_PRIORITY", ML_HIGH_PRIORITY},
      {"HISTORY", ML_HISTORY},
      {"HISTOGRAM", ML_HISTOGRAM},
      {"HOSTS", ML_HOSTS},
      {"IF", ML_IF},
      {"IGNORE", ML_IGNORE},
      {"IDENTIFIED", ML_IDENTIFIED},
      {"IN", ML_IN},
      {"INACTIVE", ML_INACTIVE},
      {"INITIAL_SIZE", ML_INITIAL_SIZE},
      {"INFILE", ML_INFILE},
      {"IMPORT", ML_IMPORT},
      {"INDEX", ML_INDEX},
      {"INITIAL", ML_INITIAL},
      {"INITIATE", ML_INITIATE},
      {"INSTANCE", ML_INSTANCE},
      {"INNODB", ML_INNODB},
      {"INSERT", ML_INSERT},
      {"INSTALL", ML_INSTALL},
      {"INSERT_METHOD", ML_INSERT_METHOD},
      {"INVISIBLE", ML_INVISIBLE},
      {"INT", ML_INT},
      {"INTEGER", ML_INTEGER},
      {"INTO", ML_INTO},
      {"INTERVAL", ML_INTERVAL},
      {"IO", ML_IO},
      {"IO_THREAD", ML_IO_THREAD},
      {"INVOKER", ML_INVOKER},
      {"IPC", ML_IPC},
      {"ISOLATION", ML_ISOLATION},
      {"ISSUER", ML_ISSUER},
      {"ITERATE", ML_ITERATE},
      {"INDEXES", ML_INDEXES},
      {"INPLACE", ML_INPLACE},
      {"JOIN", ML_JOIN},
      {"JSON", ML_JSON},
      {"KEY", ML_KEY},
      {"KEY_BLOCK_SIZE", ML_KEY_BLOCK_SIZE},
      {"KEYRING", ML_KEYRING},
      {"KEYS", ML_KEYS},
      {"KILL", ML_KILL},
      {"LANGUAGE", ML_LANGUAGE},
      {"LAST", ML_LAST},
      {"LEAVE", ML_LEAVE},
      {"LEAVES", ML_LEAVES},
      {"LEVEL", ML_LEVEL},
      {"LIKE", ML_LIKE},
      {"LIMIT", ML_LIMIT},
      {"LINES", ML_LINES},
      {"LOAD", ML_LOAD},
      {"LOCAL", ML_LOCAL},
      {"LOCK", ML_LOCK},
      {"LOG", ML_LOG},
      {"LOGFILE", ML_LOGFILE},
      {"LOGS", ML_LOGS},
      {"LOOP", ML_LOOP},
      {"LOW_PRIORITY", ML_LOW_PRIORITY},
      {"MASTER", ML_MASTER},
      {"MAX_CONNECTIONS_PER_HOUR", ML_MAX_CONNECTIONS_PER_HOUR},
      {"MAX_QUERIES_PER_HOUR", ML_MAX_QUERIES_PER_HOUR},
      {"MAX_ROWS", ML_MAX_ROWS},
      {"MAX_SIZE", ML_MAX_SIZE},
      {"MAX_UPDATES_PER_HOUR", ML_MAX_UPDATES_PER_HOUR},
      {"MAX_USER_CONNECTIONS", ML_MAX_USER_CONNECTIONS},
      {"MEDIUM", ML_MEDIUM},
      {"MEMORY", ML_MEMORY},
      {"MESSAGE_TEXT", ML_MESSAGE_TEXT},
      {"MERGE", ML_MERGE},
      {"MIGRATE", ML_MIGRATE},
      {"MIN_ROWS", ML_MIN_ROWS},
      {"MODIFIES", ML_MODIFIES},
      {"MODIFY", ML_MODIFY},
      {"MUTEX", ML_MUTEX},
      {"MYSQL_ERRNO", ML_MYSQL_ERRNO},
      {"NAME", ML_NAME},
      {"NAMES", ML_NAMES},
      {"NEVER", ML_NEVER},
      {"NEXT", ML_NEXT},
      {"NODEGROUP", ML_NODEGROUP},
      {"NO", ML_NO},
      {"NO_WRITE_TO_BINLOG", ML_NO_WRITE_TO_BINLOG},
      {"NONE", ML_NONE},
      {"NOT", ML_NOT},
      {"NUMBER", ML_NUMBER},
      {"OFFSET", ML_OFFSET},
      {"OLD", ML_OLD},
      {"ON", ML_ON},
      {"ONE", ML_ONE},
      {"ONLY", ML_ONLY},
      {"OPEN", ML_OPEN},
      {"OPTIMIZE", ML_OPTIMIZE},
      {"OPTIMIZER_COSTS", ML_OPTIMIZER_COSTS},
      {"OPTION", ML_OPTION},
      {"OPTIONS", ML_OPTIONS},
      {"OPTIONAL", ML_OPTIONAL},
      {"OPTIONALLY", ML_OPTIONALLY},
      {"OR", ML_OR},
      {"ORDER", ML_ORDER},
      {"ORGANIZATION", ML_ORGANIZATION},
      {"PACK_KEYS", ML_PACK_KEYS},
      {"PAGE", ML_PAGE},
      {"PARSE_TREE", ML_PARSE_TREE},
      {"PARSER", ML_PARSER},
      {"PASSWORD", ML_PASSWORD},
      {"PASSWORD_LOCK_TIME", ML_PASSWORD_LOCK_TIME},
      {"PARTITION", ML_PARTITION},
      {"PHASE", ML_PHASE},
      {"PERSIST", ML_PERSIST},
      {"PERSIST_ONLY", ML_PERSIST_ONLY},
      {"PLUGIN", ML_PLUGIN},
      {"PLUGIN_DIR", ML_PLUGIN_DIR},
      {"PLUGINS", ML_PLUGINS},
      {"PRECEDES", ML_PRECEDES},
      {"PRESERVE", ML_PRESERVE},
      {"PURGE", ML_PURGE},
      {"QUERY", ML_QUERY},
      {"QUICK", ML_QUICK},
      {"RANDOM", ML_RANDOM},
      {"READ", ML_READ},
      {"READS", ML_READS},
      {"REAL", ML_REAL},
      {"REBUILD", ML_REBUILD},
      {"RECOVER", ML_RECOVER},
      {"RECURSIVE", ML_RECURSIVE},
      {"REDO_BUFFER_SIZE", ML_REDO_BUFFER_SIZE},
      {"REDO_LOG", ML_REDO_LOG},
      {"PREPARE", ML_PREPARE},
      {"PRIVILEGES", ML_PRIVILEGES},
      {"PROCEDURE", ML_PROCEDURE},
      {"PROCESSLIST", ML_PROCESSLIST},
      {"PROFILE", ML_PROFILE},
      {"PROFILES", ML_PROFILES},
      {"PROXY", ML_PROXY},
      {"PREV", ML_PREV},
      {"REFERENCE", ML_REFERENCE},
      {"RELEASE", ML_RELEASE},
      {"RELAY_LOG_FILE", ML_RELAY_LOG_FILE},
      {"RELAY_LOG_POS", ML_RELAY_LOG_POS},
      {"RELOAD", ML_RELOAD},
      {"RELAY", ML_RELAY},
      {"RELAYLOG", ML_RELAYLOG},
      {"REPLICA", ML_REPLICA},
      {"REPLICAS", ML_REPLICAS},
      {"REPLICATION", ML_REPLICATION},
      {"REPLICATE_DO_DB", ML_REPLICATE_DO_DB},
      {"REPLICATE_DO_TABLE", ML_REPLICATE_DO_TABLE},
      {"REPLICATE_IGNORE_DB", ML_REPLICATE_IGNORE_DB},
      {"REPLICATE_IGNORE_TABLE", ML_REPLICATE_IGNORE_TABLE},
      {"REPLICATE_REWRITE_DB", ML_REPLICATE_REWRITE_DB},
      {"REPLICATE_WILD_DO_TABLE", ML_REPLICATE_WILD_DO_TABLE},
      {"REPLICATE_WILD_IGNORE_TABLE", ML_REPLICATE_WILD_IGNORE_TABLE},
      {"RENAME", ML_RENAME},
      {"REMOVE", ML_REMOVE},
      {"REORGANIZE", ML_REORGANIZE},
      {"REPAIR", ML_REPAIR},
      {"REGISTRATION", ML_REGISTRATION},
      {"REQUIRE", ML_REQUIRE},
      {"REPEAT", ML_REPEAT},
      {"REPEATABLE", ML_REPEATABLE},
      {"REPLACE", ML_REPLACE},
      {"RESUME", ML_RESUME},
      {"RESET", ML_RESET},
      {"RESIGNAL", ML_RESIGNAL},
      {"RESOURCE", ML_RESOURCE},
      {"RESTART", ML_RESTART},
      {"RESTRICT", ML_RESTRICT},
      {"RETAIN", ML_RETAIN},
      {"RETURN", ML_RETURN},
      {"RETURNED_SQLSTATE", ML_RETURNED_SQLSTATE},
      {"RETURNS", ML_RETURNS},
      {"REUSE", ML_REUSE},
      {"ROTATE", ML_ROTATE},
      {"REVOKE", ML_REVOKE},
      {"ROLE", ML_ROLE},
      {"ROLLBACK", ML_ROLLBACK},
      {"ROW", ML_ROW},
      {"ROW_COUNT", ML_ROW_COUNT},
      {"ROWS", ML_ROWS},
      {"ROW_FORMAT", ML_ROW_FORMAT},
      {"SAVEPOINT", ML_SAVEPOINT},
      {"SCHEMA", ML_SCHEMA},
      {"SCHEMAS", ML_SCHEMAS},
      {"SCHEMA_NAME", ML_SCHEMA_NAME},
      {"SELECT", ML_SELECT},
      {"SERIALIZABLE", ML_SERIALIZABLE},
      {"SERVER", ML_SERVER},
      {"SECURITY", ML_SECURITY},
      {"SECONDARY_ENGINE", ML_SECONDARY_ENGINE},
      {"SECONDARY_ENGINE_ATTRIBUTE", ML_SECONDARY_ENGINE_ATTRIBUTE},
      {"SET", ML_SET},
      {"SESSION", ML_SESSION},
      {"SCHEDULE", ML_SCHEDULE},
      {"SHOW", ML_SHOW},
      {"SHARED", ML_SHARED},
      {"SHUTDOWN", ML_SHUTDOWN},
      {"SIGNAL", ML_SIGNAL},
      {"SLAVE", ML_SLAVE},
      {"SLOW", ML_SLOW},
      {"SNAPSHOT", ML_SNAPSHOT},
      {"SQL", ML_SQL},
      {"SQL_THREAD", ML_SQL_THREAD},
      {"SQL_BIG_RESULT", ML_SQL_BIG_RESULT},
      {"SQL_BUFFER_RESULT", ML_SQL_BUFFER_RESULT},
      {"SQL_CALC_FOUND_ROWS", ML_SQL_CALC_FOUND_ROWS},
      {"SQL_SMALL_RESULT", ML_SQL_SMALL_RESULT},
      {"SSL", ML_SSL},
      {"SQLSTATE", ML_SQLSTATE},
      {"SPATIAL", ML_SPATIAL},
      {"SONAME", ML_SONAME},
      {"SOURCE", ML_SOURCE},
      {"SOURCE_LOG_FILE", ML_SOURCE_LOG_FILE},
      {"SOURCE_LOG_POS", ML_SOURCE_LOG_POS},
      {"SQL_AFTER_GTIDS", ML_SQL_AFTER_GTIDS},
      {"SQL_AFTER_MTS_GAPS", ML_SQL_AFTER_MTS_GAPS},
      {"SQL_BEFORE_GTIDS", ML_SQL_BEFORE_GTIDS},
      {"START", ML_START},
      {"STARTING", ML_STARTING},
      {"STATUS", ML_STATUS},
      {"STATS_AUTO_RECALC", ML_STATS_AUTO_RECALC},
      {"STATS_PERSISTENT", ML_STATS_PERSISTENT},
      {"STATS_SAMPLE_PAGES", ML_STATS_SAMPLE_PAGES},
      {"STOP", ML_STOP},
      {"STORAGE", ML_STORAGE},
      {"STRAIGHT_JOIN", ML_STRAIGHT_JOIN},
      {"STRING", ML_STRING},
      {"SUBCLASS_ORIGIN", ML_SUBCLASS_ORIGIN},
      {"SUBJECT", ML_SUBJECT},
      {"SUSPEND", ML_SUSPEND},
      {"SWAPS", ML_SWAPS},
      {"SWITCHES", ML_SWITCHES},
      {"SYSTEM", ML_SYSTEM},
      {"TABLE", ML_TABLE},
      {"TABLES", ML_TABLES},
      {"TABLE_NAME", ML_TABLE_NAME},
      {"TABLESPACE", ML_TABLESPACE},
      {"TEMPORARY", ML_TEMPORARY},
      {"TEMPTABLE", ML_TEMPTABLE},
      {"TERMINATED", ML_TERMINATED},
      {"THEN", ML_THEN},
      {"THREAD_PRIORITY", ML_THREAD_PRIORITY},
      {"TLS", ML_TLS},
      {"TO", ML_TO},
      {"TRANSACTION", ML_TRANSACTION},
      {"TRIGGER", ML_TRIGGER},
      {"TRIGGERS", ML_TRIGGERS},
      {"TRUNCATE", ML_TRUNCATE},
      {"TRADITIONAL", ML_TRADITIONAL},
      {"TREE", ML_TREE},
      {"TYPE", ML_TYPE},
      {"UNDEFINED", ML_UNDEFINED},
      {"UNCOMMITTED", ML_UNCOMMITTED},
      {"UNBOUNDED", ML_UNBOUNDED},
      {"UNREGISTER", ML_UNREGISTER},
      {"UNKNOWN", ML_UNKNOWN},
      {"UNTIL", ML_UNTIL},
      {"UNINSTALL", ML_UNINSTALL},
      {"UNLOCK", ML_UNLOCK},
      {"UNION", ML_UNION},
      {"UPDATE", ML_UPDATE},
      {"USE", ML_USE},
      {"USING", ML_USING},
      {"USER", ML_USER},
      {"USER_RESOURCES", ML_USER_RESOURCES},
      {"UNDO", ML_UNDO},
      {"UNDO_BUFFER_SIZE", ML_UNDO_BUFFER_SIZE},
      {"UNDOFILE", ML_UNDOFILE},
      {"UNIQUE", ML_UNIQUE},
      {"UPGRADE", ML_UPGRADE},
      {"VALUES", ML_VALUES},
      {"USE_FRM", ML_USE_FRM},
      {"VALUE", ML_VALUE},
      {"VARIABLES", ML_VARIABLES},
      {"VISIBLE", ML_VISIBLE},
      {"VCPU", ML_VCPU},
      {"VIEW", ML_VIEW},
      {"WARNINGS", ML_WARNINGS},
      {"WAIT", ML_WAIT},
      {"WHEN", ML_WHEN},
      {"WHERE", ML_WHERE},
      {"WHILE", ML_WHILE},
      {"WITH", ML_WITH},
      {"WORK", ML_WORK},
      {"WRAPPER", ML_WRAPPER},
      {"WRITE", ML_WRITE},
      {"XML", ML_XML},
      {"X509", ML_X509},
      {"XID", ML_XID},
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
  return isalpha(c) || c == '_' || c == '$' || c >= 0x80;
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
