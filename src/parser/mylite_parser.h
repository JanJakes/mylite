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

typedef enum MyliteStatementKind {
  MYLITE_STATEMENT_EMPTY = 0,
  MYLITE_STATEMENT_SELECT,
  MYLITE_STATEMENT_INSERT,
  MYLITE_STATEMENT_REPLACE,
  MYLITE_STATEMENT_UPDATE,
  MYLITE_STATEMENT_DELETE,
  MYLITE_STATEMENT_DDL,
  MYLITE_STATEMENT_TRANSACTION,
  MYLITE_STATEMENT_PREPARED,
  MYLITE_STATEMENT_SHOW,
  MYLITE_STATEMENT_UTILITY,
  MYLITE_STATEMENT_ADMIN,
  MYLITE_STATEMENT_STORED_PROGRAM,
  MYLITE_STATEMENT_REPLICATION,
  MYLITE_STATEMENT_PERMISSIVE,
  MYLITE_STATEMENT_KIND_COUNT
} MyliteStatementKind;

typedef struct MyliteParseResult {
  size_t statement_count;
  size_t empty_statement_count;
  size_t token_count;
  size_t permissive_fallbacks;
  size_t statement_kind_counts[MYLITE_STATEMENT_KIND_COUNT];
  size_t error_offset;
  size_t error_line;
  size_t error_column;
  char error_message[160];
} MyliteParseResult;

MyliteParseStatus mylite_parse_sql(const char *sql, size_t length,
                                   MyliteParseResult *result);
MyliteParseStatus mylite_parse_sql_permissive(const char *sql, size_t length,
                                              MyliteParseResult *result);
const char *mylite_statement_kind_name(MyliteStatementKind kind);

#ifdef __cplusplus
}
#endif

#endif
