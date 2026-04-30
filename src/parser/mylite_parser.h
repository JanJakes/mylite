#ifndef MYLITE_PARSER_H
#define MYLITE_PARSER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MyliteParseStatus {
  MYLITE_PARSE_OK = 0,
  MYLITE_PARSE_ERROR = 1
} MyliteParseStatus;

typedef struct MyliteParseResult {
  size_t statement_count;
  size_t token_count;
  size_t error_offset;
  size_t error_line;
  size_t error_column;
  char error_message[160];
} MyliteParseResult;

MyliteParseStatus mylite_parse_sql(const char *sql, size_t length,
                                   MyliteParseResult *result);

#ifdef __cplusplus
}
#endif

#endif

