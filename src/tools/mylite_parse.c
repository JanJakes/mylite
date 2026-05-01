#include "mylite/parser.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum OutputMode {
  OUTPUT_VALIDATE,
  OUTPUT_AST,
  OUTPUT_STATEMENTS
} OutputMode;

static int parse_stdin(OutputMode mode);
static int parse_file(const char *path, OutputMode mode);
static int parse_sql_text(const char *sql, const char *label, OutputMode mode);
static void dump_statements(const MyliteAst *ast);
static void dump_ast_node(const MyliteAstNode *node, unsigned depth);
static char *read_stream(FILE *stream, const char *label);

int main(int argc, char **argv) {
  OutputMode mode = OUTPUT_VALIDATE;
  int first_path = 1;
  while (first_path < argc && strncmp(argv[first_path], "--", 2) == 0) {
    if (strcmp(argv[first_path], "--ast") == 0) {
      mode = OUTPUT_AST;
    } else if (strcmp(argv[first_path], "--statements") == 0) {
      mode = OUTPUT_STATEMENTS;
    } else {
      fprintf(stderr, "unknown option: %s\n", argv[first_path]);
      return 2;
    }
    first_path++;
  }

  if (first_path == argc) {
    return parse_stdin(mode);
  }

  int failed = 0;
  for (int i = first_path; i < argc; i++) {
    if (parse_file(argv[i], mode) != 0) {
      failed = 1;
    }
  }
  return failed;
}

static int parse_stdin(OutputMode mode) {
  char *sql = read_stream(stdin, "<stdin>");
  if (sql == NULL) {
    return 2;
  }

  int status = parse_sql_text(sql, "<stdin>", mode);
  free(sql);
  return status;
}

static int parse_file(const char *path, OutputMode mode) {
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

  int status = parse_sql_text(sql, path, mode);
  free(sql);
  return status;
}

static int parse_sql_text(const char *sql, const char *label, OutputMode mode) {
  MyliteParseResult result;
  MyliteAst *ast = NULL;
  MyliteParseStatus status;
  if (mode == OUTPUT_VALIDATE) {
    status = mylite_parse_sql(sql, &result);
  } else {
    status = mylite_parse_sql_ast(sql, &ast, &result);
  }

  if (status == MYLITE_PARSE_OK) {
    if (mode == OUTPUT_STATEMENTS) {
      dump_statements(ast);
    } else if (mode == OUTPUT_AST) {
      dump_statements(ast);
      dump_ast_node(mylite_ast_root(ast), 0);
    }
    mylite_ast_free(ast);
    return 0;
  }

  mylite_ast_free(ast);
  fprintf(stderr, "%s:%zu: %s: %s\n", label, result.offset,
          mylite_parse_status_name(status), result.message);
  return 1;
}

static void dump_statements(const MyliteAst *ast) {
  size_t count = mylite_ast_statement_count(ast);
  printf("statements=%zu nodes=%zu ast_bytes=%zu\n", count,
         mylite_ast_node_count(ast), mylite_ast_allocated_bytes(ast));
  for (size_t i = 0; i < count; i++) {
    printf("statement[%zu] kind=%s symbol=%s span=%zu..%zu\n", i,
           mylite_statement_kind_name(mylite_ast_statement_kind(ast, i)),
           mylite_ast_statement_symbol_name(ast, i),
           mylite_ast_statement_start(ast, i), mylite_ast_statement_end(ast, i));
  }
}

static void dump_ast_node(const MyliteAstNode *node, unsigned depth) {
  if (node == NULL) {
    return;
  }

  for (unsigned i = 0; i < depth; i++) {
    fputs("  ", stdout);
  }
  if (mylite_ast_node_kind(node) == MYLITE_AST_NODE_TOKEN) {
    printf("token id=%d span=%zu..%zu\n", mylite_ast_node_token(node),
           mylite_ast_node_start(node), mylite_ast_node_end(node));
  } else {
    printf("rule id=%u symbol=%s span=%zu..%zu children=%zu\n",
           mylite_ast_node_rule_id(node), mylite_ast_node_symbol_name(node),
           mylite_ast_node_start(node), mylite_ast_node_end(node),
           mylite_ast_node_child_count(node));
  }

  for (size_t i = 0; i < mylite_ast_node_child_count(node); i++) {
    dump_ast_node(mylite_ast_node_child(node, i), depth + 1);
  }
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
