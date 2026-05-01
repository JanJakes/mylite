#include "mylite/parser.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum BenchMode {
  BENCH_SYNTAX,
  BENCH_AST
} BenchMode;

static int run_benchmark(const char *path, BenchMode mode, int iterations);
static char *read_file(const char *path, size_t *length);
static double monotonic_seconds(void);
static int parse_mode(const char *value, BenchMode *mode);

int main(int argc, char **argv) {
  if (argc < 2 || argc > 4) {
    fprintf(stderr, "usage: %s queries.nul [syntax|ast] [iterations]\n", argv[0]);
    return 2;
  }

  BenchMode mode = BENCH_SYNTAX;
  if (argc >= 3 && !parse_mode(argv[2], &mode)) {
    fprintf(stderr, "unknown mode: %s\n", argv[2]);
    return 2;
  }

  int iterations = argc == 4 ? atoi(argv[3]) : 10;
  if (iterations <= 0) {
    iterations = 1;
  }

  return run_benchmark(argv[1], mode, iterations);
}

static int run_benchmark(const char *path, BenchMode mode, int iterations) {
  size_t length = 0;
  char *buffer = read_file(path, &length);
  if (buffer == NULL) {
    return 2;
  }

  size_t query_count = 0;
  size_t query_bytes = 0;
  for (size_t offset = 0; offset < length;) {
    size_t query_length = strlen(buffer + offset);
    query_count++;
    query_bytes += query_length;
    offset += query_length + 1;
  }

  size_t parsed = 0;
  size_t failed = 0;
  size_t ast_nodes = 0;
  size_t ast_bytes = 0;
  size_t statements = 0;
  size_t targets = 0;
  double start = monotonic_seconds();
  for (int iteration = 0; iteration < iterations; iteration++) {
    for (size_t offset = 0; offset < length;) {
      const char *query = buffer + offset;
      size_t query_length = strlen(query);
      MyliteParseResult result;
      MyliteParseStatus status;
      if (mode == BENCH_AST) {
        MyliteAst *ast = NULL;
        status = mylite_parse_sql_ast(query, &ast, &result);
        if (status == MYLITE_PARSE_OK) {
          ast_nodes += mylite_ast_node_count(ast);
          ast_bytes += mylite_ast_allocated_bytes(ast);
          statements += mylite_ast_statement_count(ast);
          for (size_t i = 0; i < mylite_ast_statement_count(ast); i++) {
            targets += mylite_ast_statement_target_count(ast, i);
          }
        }
        mylite_ast_free(ast);
      } else {
        status = mylite_parse_sql(query, &result);
      }

      if (status == MYLITE_PARSE_OK) {
        parsed++;
      } else {
        failed++;
      }
      offset += query_length + 1;
    }
  }
  double elapsed = monotonic_seconds() - start;

  double total_queries = (double)query_count * (double)iterations;
  double total_bytes = (double)query_bytes * (double)iterations;
  printf("mode=%s queries=%zu iterations=%d parsed=%zu failed=%zu elapsed=%.6f "
         "qps=%.0f mbps=%.2f avg_us=%.3f",
         mode == BENCH_AST ? "ast" : "syntax", query_count, iterations, parsed, failed,
         elapsed, total_queries / elapsed, (total_bytes / (1024.0 * 1024.0)) / elapsed,
         (elapsed * 1000000.0) / total_queries);
  if (mode == BENCH_AST && parsed > 0) {
    printf(" avg_nodes=%.1f avg_ast_bytes=%.1f avg_statements=%.2f avg_targets=%.2f",
           (double)ast_nodes / (double)parsed, (double)ast_bytes / (double)parsed,
           (double)statements / (double)parsed, (double)targets / (double)parsed);
  }
  printf("\n");

  free(buffer);
  return failed == 0 ? 0 : 1;
}

static char *read_file(const char *path, size_t *length) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "open %s: %s\n", path, strerror(errno));
    return NULL;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fprintf(stderr, "seek %s: %s\n", path, strerror(errno));
    fclose(file);
    return NULL;
  }
  long size = ftell(file);
  if (size < 0) {
    fprintf(stderr, "tell %s: %s\n", path, strerror(errno));
    fclose(file);
    return NULL;
  }
  rewind(file);

  char *buffer = malloc((size_t)size + 1);
  if (buffer == NULL) {
    fprintf(stderr, "malloc %ld bytes failed\n", size);
    fclose(file);
    return NULL;
  }
  size_t read = fread(buffer, 1, (size_t)size, file);
  fclose(file);
  if (read != (size_t)size) {
    fprintf(stderr, "read %s failed\n", path);
    free(buffer);
    return NULL;
  }
  buffer[size] = '\0';
  *length = (size_t)size;
  return buffer;
}

static double monotonic_seconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static int parse_mode(const char *value, BenchMode *mode) {
  if (strcmp(value, "syntax") == 0) {
    *mode = BENCH_SYNTAX;
    return 1;
  }
  if (strcmp(value, "ast") == 0) {
    *mode = BENCH_AST;
    return 1;
  }
  return 0;
}
