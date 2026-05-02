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
  size_t target_schema_values = 0;
  size_t target_name_values = 0;
  size_t create_table_views = 0;
  size_t create_table_view_schema_values = 0;
  size_t create_table_view_name_values = 0;
  size_t create_table_view_columns = 0;
  size_t create_table_view_keys = 0;
  size_t create_table_view_options = 0;
  size_t create_table_view_column_handles = 0;
  size_t create_table_view_known_column_types = 0;
  size_t create_table_view_key_handles = 0;
  size_t create_table_view_named_keys = 0;
  size_t create_table_view_option_handles = 0;
  size_t columns = 0;
  size_t column_name_values = 0;
  size_t column_known_types = 0;
  size_t column_known_storage_classes = 0;
  size_t column_type_numeric_parameters = 0;
  size_t column_type_elements = 0;
  size_t column_type_element_values = 0;
  size_t column_type_lengths = 0;
  size_t column_type_precisions = 0;
  size_t column_type_scales = 0;
  size_t column_type_fsps = 0;
  size_t column_type_unsigned_attrs = 0;
  size_t column_type_zerofill_attrs = 0;
  size_t column_type_binary_attrs = 0;
  size_t column_type_charsets = 0;
  size_t column_type_collations = 0;
  size_t column_value_roots = 0;
  size_t column_defaults = 0;
  size_t column_on_updates = 0;
  size_t column_generated = 0;
  size_t column_checks = 0;
  size_t column_references = 0;
  size_t keys = 0;
  size_t key_constraint_name_values = 0;
  size_t key_name_values = 0;
  size_t key_referenced_table_schema_values = 0;
  size_t key_referenced_table_name_values = 0;
  size_t key_columns = 0;
  size_t key_column_name_values = 0;
  size_t key_referenced_column_name_values = 0;
  size_t key_options = 0;
  size_t options = 0;
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
            for (size_t j = 0; j < mylite_ast_statement_target_count(ast, i);
                 j++) {
              if (mylite_ast_statement_target_schema_value_at(ast, i, j) != NULL) {
                target_schema_values++;
              }
              if (mylite_ast_statement_target_name_value_at(ast, i, j) != NULL) {
                target_name_values++;
              }
            }
            columns += mylite_ast_create_table_column_count(ast, i);
            keys += mylite_ast_create_table_key_count(ast, i);
            options += mylite_ast_create_table_option_count(ast, i);
            const MyliteAstCreateTable *create_table =
                mylite_ast_create_table_view(ast, i);
            if (create_table != NULL) {
              create_table_views++;
              if (mylite_ast_create_table_view_schema_value(create_table) !=
                  NULL) {
                create_table_view_schema_values++;
              }
              if (mylite_ast_create_table_view_name_value(create_table) !=
                  NULL) {
                create_table_view_name_values++;
              }
              create_table_view_columns +=
                  mylite_ast_create_table_view_column_count(create_table);
              create_table_view_keys +=
                  mylite_ast_create_table_view_key_count(create_table);
              create_table_view_options +=
                  mylite_ast_create_table_view_option_count(create_table);
              for (size_t k = 0;
                   k < mylite_ast_create_table_view_column_count(create_table);
                   k++) {
                const MyliteAstCreateTableColumn *column =
                    mylite_ast_create_table_view_column_at(create_table, k);
                if (column != NULL) {
                  create_table_view_column_handles++;
                }
                if (mylite_ast_create_table_column_view_type_kind(column) !=
                    MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_UNKNOWN) {
                  create_table_view_known_column_types++;
                }
              }
              for (size_t k = 0;
                   k < mylite_ast_create_table_view_key_count(create_table);
                   k++) {
                const MyliteAstCreateTableKey *key =
                    mylite_ast_create_table_view_key_at(create_table, k);
                if (key != NULL) {
                  create_table_view_key_handles++;
                }
                if (mylite_ast_create_table_key_view_name_value(key) != NULL) {
                  create_table_view_named_keys++;
                }
              }
              for (size_t k = 0;
                   k < mylite_ast_create_table_view_option_count(create_table);
                   k++) {
                if (mylite_ast_create_table_view_option_at(create_table, k) !=
                    NULL) {
                  create_table_view_option_handles++;
                }
              }
            }
            for (size_t j = 0; j < mylite_ast_create_table_column_count(ast, i);
                 j++) {
              if (mylite_ast_create_table_column_name_value(ast, i, j) != NULL) {
                column_name_values++;
              }
              if (mylite_ast_create_table_column_type_kind(ast, i, j) !=
                  MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_UNKNOWN) {
                column_known_types++;
              }
              if (mylite_ast_create_table_column_storage_class(ast, i, j) !=
                  MYLITE_CREATE_TABLE_COLUMN_STORAGE_UNKNOWN) {
                column_known_storage_classes++;
              }
              column_type_numeric_parameters +=
                  mylite_ast_create_table_column_type_numeric_parameter_count(
                      ast, i, j);
              column_type_elements +=
                  mylite_ast_create_table_column_type_element_count(ast, i, j);
              for (size_t k = 0;
                   k < mylite_ast_create_table_column_type_element_count(ast, i,
                                                                         j);
                   k++) {
                if (mylite_ast_create_table_column_type_element_value(ast, i, j,
                                                                      k) != NULL) {
                  column_type_element_values++;
                }
              }
              if (mylite_ast_create_table_column_type_has_length(ast, i, j)) {
                column_type_lengths++;
              }
              if (mylite_ast_create_table_column_type_has_precision(ast, i, j)) {
                column_type_precisions++;
              }
              if (mylite_ast_create_table_column_type_has_scale(ast, i, j)) {
                column_type_scales++;
              }
              if (mylite_ast_create_table_column_type_has_fractional_seconds_precision(
                      ast, i, j)) {
                column_type_fsps++;
              }
              if (mylite_ast_create_table_column_type_unsigned_end(ast, i, j) !=
                  0) {
                column_type_unsigned_attrs++;
              }
              if (mylite_ast_create_table_column_type_zerofill_end(ast, i, j) !=
                  0) {
                column_type_zerofill_attrs++;
              }
              if (mylite_ast_create_table_column_type_binary_end(ast, i, j) !=
                  0) {
                column_type_binary_attrs++;
              }
              if (mylite_ast_create_table_column_type_charset_value_end(ast, i,
                                                                        j) != 0) {
                column_type_charsets++;
              }
              if (mylite_ast_create_table_column_type_collation_value_end(ast, i,
                                                                          j) != 0) {
                column_type_collations++;
              }
              if (mylite_ast_create_table_column_default_value_node(ast, i, j) !=
                  NULL) {
                column_value_roots++;
              }
              if (mylite_ast_create_table_column_on_update_value_node(ast, i, j) !=
                  NULL) {
                column_value_roots++;
              }
              if (mylite_ast_create_table_column_generated_expression_node(
                      ast, i, j) != NULL) {
                column_value_roots++;
              }
              if (mylite_ast_create_table_column_check_expression_node(ast, i, j) !=
                  NULL) {
                column_value_roots++;
              }
              if (mylite_ast_create_table_column_default_end(ast, i, j) != 0) {
                column_defaults++;
              }
              if (mylite_ast_create_table_column_on_update_end(ast, i, j) != 0) {
                column_on_updates++;
              }
              if (mylite_ast_create_table_column_generated_end(ast, i, j) != 0) {
                column_generated++;
              }
              if (mylite_ast_create_table_column_check_end(ast, i, j) != 0) {
                column_checks++;
              }
              if (mylite_ast_create_table_column_reference_end(ast, i, j) != 0) {
                column_references++;
              }
            }
            for (size_t j = 0; j < mylite_ast_create_table_key_count(ast, i); j++) {
              if (mylite_ast_create_table_key_constraint_name_value(ast, i, j) !=
                  NULL) {
                key_constraint_name_values++;
              }
              if (mylite_ast_create_table_key_name_value(ast, i, j) != NULL) {
                key_name_values++;
              }
              if (mylite_ast_create_table_key_referenced_table_schema_value(
                      ast, i, j) != NULL) {
                key_referenced_table_schema_values++;
              }
              if (mylite_ast_create_table_key_referenced_table_name_value(ast, i,
                                                                          j) !=
                  NULL) {
                key_referenced_table_name_values++;
              }
              key_columns += mylite_ast_create_table_key_column_count(ast, i, j);
              for (size_t k = 0;
                   k < mylite_ast_create_table_key_column_count(ast, i, j);
                   k++) {
                if (mylite_ast_create_table_key_column_name_value(ast, i, j, k) !=
                    NULL) {
                  key_column_name_values++;
                }
              }
              key_columns +=
                  mylite_ast_create_table_key_referenced_column_count(ast, i, j);
              for (size_t k = 0;
                   k <
                   mylite_ast_create_table_key_referenced_column_count(ast, i, j);
                   k++) {
                if (mylite_ast_create_table_key_referenced_column_name_value(
                        ast, i, j, k) != NULL) {
                  key_referenced_column_name_values++;
                }
              }
              key_options += mylite_ast_create_table_key_option_count(ast, i, j);
            }
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
    printf(" avg_nodes=%.1f avg_ast_bytes=%.1f avg_statements=%.2f "
           "avg_targets=%.2f avg_target_schema_values=%.2f "
           "avg_target_name_values=%.2f avg_columns=%.2f avg_keys=%.2f "
           "avg_create_table_views=%.2f "
           "avg_create_table_view_schema_values=%.2f "
           "avg_create_table_view_name_values=%.2f "
           "avg_create_table_view_columns=%.2f "
           "avg_create_table_view_keys=%.2f "
           "avg_create_table_view_options=%.2f "
           "avg_create_table_view_column_handles=%.2f "
           "avg_create_table_view_known_column_types=%.2f "
           "avg_create_table_view_key_handles=%.2f "
           "avg_create_table_view_named_keys=%.2f "
           "avg_create_table_view_option_handles=%.2f "
           "avg_key_constraint_name_values=%.2f avg_key_name_values=%.2f "
           "avg_key_referenced_table_schema_values=%.2f "
           "avg_key_referenced_table_name_values=%.2f "
           "avg_key_columns=%.2f avg_key_column_name_values=%.2f "
           "avg_key_referenced_column_name_values=%.2f "
           "avg_key_options=%.2f avg_options=%.2f",
           (double)ast_nodes / (double)parsed, (double)ast_bytes / (double)parsed,
           (double)statements / (double)parsed, (double)targets / (double)parsed,
           (double)target_schema_values / (double)parsed,
           (double)target_name_values / (double)parsed,
           (double)columns / (double)parsed, (double)keys / (double)parsed,
           (double)create_table_views / (double)parsed,
           (double)create_table_view_schema_values / (double)parsed,
           (double)create_table_view_name_values / (double)parsed,
           (double)create_table_view_columns / (double)parsed,
           (double)create_table_view_keys / (double)parsed,
           (double)create_table_view_options / (double)parsed,
           (double)create_table_view_column_handles / (double)parsed,
           (double)create_table_view_known_column_types / (double)parsed,
           (double)create_table_view_key_handles / (double)parsed,
           (double)create_table_view_named_keys / (double)parsed,
           (double)create_table_view_option_handles / (double)parsed,
           (double)key_constraint_name_values / (double)parsed,
           (double)key_name_values / (double)parsed,
           (double)key_referenced_table_schema_values / (double)parsed,
           (double)key_referenced_table_name_values / (double)parsed,
           (double)key_columns / (double)parsed,
           (double)key_column_name_values / (double)parsed,
           (double)key_referenced_column_name_values / (double)parsed,
           (double)key_options / (double)parsed, (double)options / (double)parsed);
    printf(" avg_column_name_values=%.2f "
           "avg_column_defaults=%.2f avg_column_on_updates=%.2f "
           "avg_column_generated=%.2f avg_column_checks=%.2f "
           "avg_column_references=%.2f avg_column_known_types=%.2f "
           "avg_column_storage_classes=%.2f "
           "avg_column_type_numeric_params=%.2f "
           "avg_column_type_elements=%.2f "
           "avg_column_type_element_values=%.2f "
           "avg_column_type_lengths=%.2f "
           "avg_column_type_precisions=%.2f avg_column_type_scales=%.2f "
           "avg_column_type_fsps=%.2f avg_column_type_unsigned_attrs=%.2f "
           "avg_column_type_zerofill_attrs=%.2f "
           "avg_column_type_binary_attrs=%.2f avg_column_type_charsets=%.2f "
           "avg_column_type_collations=%.2f avg_column_value_roots=%.2f",
           (double)column_name_values / (double)parsed,
           (double)column_defaults / (double)parsed,
           (double)column_on_updates / (double)parsed,
           (double)column_generated / (double)parsed,
           (double)column_checks / (double)parsed,
           (double)column_references / (double)parsed,
           (double)column_known_types / (double)parsed,
           (double)column_known_storage_classes / (double)parsed,
           (double)column_type_numeric_parameters / (double)parsed,
           (double)column_type_elements / (double)parsed,
           (double)column_type_element_values / (double)parsed,
           (double)column_type_lengths / (double)parsed,
           (double)column_type_precisions / (double)parsed,
           (double)column_type_scales / (double)parsed,
           (double)column_type_fsps / (double)parsed,
           (double)column_type_unsigned_attrs / (double)parsed,
           (double)column_type_zerofill_attrs / (double)parsed,
           (double)column_type_binary_attrs / (double)parsed,
           (double)column_type_charsets / (double)parsed,
           (double)column_type_collations / (double)parsed,
           (double)column_value_roots / (double)parsed);
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
