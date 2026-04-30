#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mylite_parser.h"

typedef struct Buffer {
  char *data;
  size_t length;
  size_t capacity;
} Buffer;

static int run_single_stdin(int quiet);
static int run_nul_stream(int quiet);
static int parse_one(const char *sql, size_t length, size_t index, int quiet);
static int read_stdin(Buffer *buffer);
static int buffer_append(Buffer *buffer, const char *data, size_t length);
static void buffer_free(Buffer *buffer);
static void usage(const char *argv0);

int main(int argc, char **argv) {
  int nul_stream = 0;
  int quiet = 0;
  int i;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--nul") == 0) {
      nul_stream = 1;
    } else if (strcmp(argv[i], "--quiet") == 0) {
      quiet = 1;
    } else if (strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      usage(argv[0]);
      return 2;
    }
  }

  if (nul_stream) {
    return run_nul_stream(quiet);
  }

  return run_single_stdin(quiet);
}

static int run_single_stdin(int quiet) {
  Buffer buffer = {0};
  int status;

  if (!read_stdin(&buffer)) {
    fprintf(stderr, "failed to read stdin\n");
    return 2;
  }

  status = parse_one(buffer.data, buffer.length, 1, quiet);
  buffer_free(&buffer);
  return status;
}

static int run_nul_stream(int quiet) {
  Buffer buffer = {0};
  size_t start = 0;
  size_t index = 1;
  size_t failures = 0;
  size_t i;

  if (!read_stdin(&buffer)) {
    fprintf(stderr, "failed to read stdin\n");
    return 2;
  }

  for (i = 0; i <= buffer.length; i++) {
    if (i == buffer.length || buffer.data[i] == '\0') {
      if (parse_one(buffer.data + start, i - start, index, quiet) != 0) {
        failures++;
      }
      start = i + 1;
      index++;
    }
  }

  if (!quiet) {
    fprintf(stderr, "parsed %zu queries, failures: %zu\n", index - 1,
            failures);
  }

  buffer_free(&buffer);
  return failures == 0 ? 0 : 1;
}

static int parse_one(const char *sql, size_t length, size_t index, int quiet) {
  MyliteParseResult result;
  MyliteParseStatus status = mylite_parse_sql(sql, length, &result);

  if (status == MYLITE_PARSE_OK) {
    return 0;
  }

  if (!quiet || index <= 20) {
    fprintf(stderr, "query %zu: %s at %zu:%zu\n", index,
            result.error_message[0] == '\0' ? "parse error"
                                             : result.error_message,
            result.error_line, result.error_column);
  }

  return 1;
}

static int read_stdin(Buffer *buffer) {
  char chunk[8192];
  size_t nread;

  while ((nread = fread(chunk, 1, sizeof(chunk), stdin)) > 0) {
    if (!buffer_append(buffer, chunk, nread)) {
      return 0;
    }
  }

  return ferror(stdin) == 0;
}

static int buffer_append(Buffer *buffer, const char *data, size_t length) {
  if (buffer->length + length > buffer->capacity) {
    size_t capacity = buffer->capacity == 0 ? 8192 : buffer->capacity;
    char *next;

    while (capacity < buffer->length + length) {
      capacity *= 2;
    }

    next = realloc(buffer->data, capacity);
    if (next == NULL) {
      return 0;
    }

    buffer->data = next;
    buffer->capacity = capacity;
  }

  memcpy(buffer->data + buffer->length, data, length);
  buffer->length += length;
  return 1;
}

static void buffer_free(Buffer *buffer) {
  free(buffer->data);
}

static void usage(const char *argv0) {
  fprintf(stderr,
          "usage: %s [--nul] [--quiet]\n"
          "  default: parse stdin as one SQL string\n"
          "  --nul:  parse NUL-separated SQL strings from stdin\n",
          argv0);
}

