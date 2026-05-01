#ifndef MYLITE_PARSER_H
#define MYLITE_PARSER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MyliteParseStatus {
  MYLITE_PARSE_OK = 0,
  MYLITE_PARSE_SYNTAX_ERROR = 1,
  MYLITE_PARSE_LEX_ERROR = 2,
  MYLITE_PARSE_NO_MEMORY = 3
} MyliteParseStatus;

typedef struct MyliteParseResult {
  MyliteParseStatus status;
  size_t offset;
  int token;
  char message[160];
} MyliteParseResult;

MyliteParseStatus mylite_parse_sql(const char *sql, MyliteParseResult *result);
const char *mylite_parse_status_name(MyliteParseStatus status);

#ifdef __cplusplus
}
#endif

#endif
