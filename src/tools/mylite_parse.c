#include "mylite/parser.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_stdin(void);
static int parse_file(const char *path);
static char *read_stream(FILE *stream, const char *label);

int main(int argc, char **argv) {
  if (argc == 1) {
    return parse_stdin();
  }

  int failed = 0;
  for (int i = 1; i < argc; i++) {
    if (parse_file(argv[i]) != 0) {
      failed = 1;
    }
  }
  return failed;
}

static int parse_stdin(void) {
  char *sql = read_stream(stdin, "<stdin>");
  if (sql == NULL) {
    return 2;
  }

  MyliteParseResult result;
  MyliteParseStatus status = mylite_parse_sql(sql, &result);
  free(sql);

  if (status == MYLITE_PARSE_OK) {
    return 0;
  }

  fprintf(stderr, "%s: %s\n", mylite_parse_status_name(status), result.message);
  return 1;
}

static int parse_file(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "%s: %s\n", path, strerror(errno));
    return 2;
  }

  char *sql = read_stream(file, path);
  fclose(file);
  if (sql == NULL) {
    return 2;
  }

  MyliteParseResult result;
  MyliteParseStatus status = mylite_parse_sql(sql, &result);
  free(sql);

  if (status == MYLITE_PARSE_OK) {
    return 0;
  }

  fprintf(stderr, "%s:%zu: %s: %s\n", path, result.offset,
          mylite_parse_status_name(status), result.message);
  return 1;
}

static char *read_stream(FILE *stream, const char *label) {
  size_t capacity = 4096;
  size_t length = 0;
  char *buffer = (char *)malloc(capacity);
  if (buffer == NULL) {
    fprintf(stderr, "%s: allocation failed\n", label);
    return NULL;
  }

  for (;;) {
    if (length == capacity) {
      size_t next_capacity = capacity * 2;
      char *next = (char *)realloc(buffer, next_capacity);
      if (next == NULL) {
        free(buffer);
        fprintf(stderr, "%s: allocation failed\n", label);
        return NULL;
      }
      buffer = next;
      capacity = next_capacity;
    }

    size_t nread = fread(buffer + length, 1, capacity - length, stream);
    length += nread;

    if (nread == 0) {
      if (ferror(stream)) {
        fprintf(stderr, "%s: read failed\n", label);
        free(buffer);
        return NULL;
      }
      break;
    }
  }

  char *result = (char *)realloc(buffer, length + 1);
  if (result == NULL) {
    free(buffer);
    fprintf(stderr, "%s: allocation failed\n", label);
    return NULL;
  }
  result[length] = '\0';
  return result;
}
