#include "mylite/parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct MyliteSemanticAstChunk {
  struct MyliteSemanticAstChunk *next;
  size_t capacity;
  size_t used;
  unsigned char data[];
} MyliteSemanticAstChunk;

struct MyliteSemanticAstNode {
  MyliteSemanticNodeKind kind;
  MyliteStatementKind statement_kind;
  MyliteStatementTargetKind target_kind;
  MyliteStatementTargetRole target_role;
  MyliteExpressionKind expression_kind;
  MyliteExpressionLiteralKind expression_literal_kind;
  MyliteExpressionOperatorKind expression_operator_kind;
  size_t start;
  size_t end;
  const char *value;
  size_t value_length;
  MyliteSemanticAstNode **children;
  size_t child_count;
};

struct MyliteSemanticAst {
  MyliteSemanticAstNode *root;
  MyliteSemanticAstChunk *chunks;
  char *source;
  size_t source_length;
  size_t node_count;
  size_t statement_count;
  size_t allocated_bytes;
};

static MyliteSemanticAst *mylite_semantic_ast_create(const char *source);
static int mylite_semantic_ast_from_parser_ast(MyliteSemanticAst *ast,
                                               const MyliteAst *parser_ast);
static void *mylite_semantic_ast_alloc(MyliteSemanticAst *ast, size_t size);
static int mylite_semantic_ast_set_node_child_count(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *node, size_t child_count);
static MyliteSemanticAstNode *mylite_semantic_ast_materialize_statement(
    MyliteSemanticAst *ast, const MyliteAst *parser_ast,
    size_t statement_index);
static size_t mylite_semantic_ast_count_statement_expression_roots(
    const MyliteAst *parser_ast, size_t statement_index);
static int mylite_semantic_ast_fill_statement_children(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *statement,
    const MyliteAst *parser_ast, size_t statement_index);
static MyliteSemanticAstNode *mylite_semantic_ast_materialize_target(
    MyliteSemanticAst *ast, const MyliteAst *parser_ast,
    size_t statement_index, size_t target_index);
static int mylite_semantic_ast_fill_statement_expression_roots(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *statement,
    const MyliteAst *parser_ast, size_t statement_index, size_t *child_index);
static MyliteSemanticAstNode *mylite_semantic_ast_materialize_expression(
    MyliteSemanticAst *ast, const MyliteAstExpression *expression);
static MyliteSemanticAstNode *mylite_semantic_ast_new_node(
    MyliteSemanticAst *ast, MyliteSemanticNodeKind kind, size_t start,
    size_t end);
static int mylite_semantic_ast_append_child(MyliteSemanticAstNode *parent,
                                            size_t *index,
                                            MyliteSemanticAstNode *child);
static int mylite_semantic_ast_copy_node_value(MyliteSemanticAst *ast,
                                               MyliteSemanticAstNode *node,
                                               const char *value,
                                               size_t value_length);
static size_t mylite_semantic_ast_count_expression_root(
    const MyliteAstExpression *expression);
static int mylite_semantic_ast_append_expression_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    const MyliteAstExpression *expression);
static size_t mylite_semantic_ast_count_create_table_column_expressions(
    const MyliteAstCreateTableColumn *column);
static int mylite_semantic_ast_append_create_table_column_expressions(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    const MyliteAstCreateTableColumn *column);
static size_t mylite_semantic_ast_count_create_table_key_expressions(
    const MyliteAstCreateTableKey *key);
static int mylite_semantic_ast_append_create_table_key_expressions(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    const MyliteAstCreateTableKey *key);
static void mylite_semantic_ast_set_no_memory(MyliteParseResult *result,
                                              const char *message);

MyliteParseStatus mylite_parse_sql_semantic_ast(const char *sql,
                                                MyliteSemanticAst **ast,
                                                MyliteParseResult *result) {
  if (ast == NULL) {
    if (result != NULL) {
      memset(result, 0, sizeof(*result));
      result->status = MYLITE_PARSE_LEX_ERROR;
      snprintf(result->message, sizeof(result->message),
               "semantic AST output is null");
    }
    return MYLITE_PARSE_LEX_ERROR;
  }
  *ast = NULL;

  MyliteAst *parser_ast = NULL;
  MyliteParseStatus status = mylite_parse_sql_ast(sql, &parser_ast, result);
  if (status != MYLITE_PARSE_OK) {
    return status;
  }

  MyliteSemanticAst *semantic_ast = mylite_semantic_ast_create(sql);
  if (semantic_ast == NULL) {
    mylite_ast_free(parser_ast);
    mylite_semantic_ast_set_no_memory(result,
                                      "semantic AST allocation failed");
    return MYLITE_PARSE_NO_MEMORY;
  }
  if (!mylite_semantic_ast_from_parser_ast(semantic_ast, parser_ast)) {
    mylite_ast_free(parser_ast);
    mylite_semantic_ast_free(semantic_ast);
    mylite_semantic_ast_set_no_memory(
        result, "semantic AST materialization failed");
    return MYLITE_PARSE_NO_MEMORY;
  }

  mylite_ast_free(parser_ast);
  *ast = semantic_ast;
  return MYLITE_PARSE_OK;
}

void mylite_semantic_ast_free(MyliteSemanticAst *ast) {
  if (ast == NULL) {
    return;
  }
  MyliteSemanticAstChunk *chunk = ast->chunks;
  while (chunk != NULL) {
    MyliteSemanticAstChunk *next = chunk->next;
    free(chunk);
    chunk = next;
  }
  free(ast->source);
  free(ast);
}

const MyliteSemanticAstNode *mylite_semantic_ast_root(
    const MyliteSemanticAst *ast) {
  return ast == NULL ? NULL : ast->root;
}

size_t mylite_semantic_ast_node_count(const MyliteSemanticAst *ast) {
  return ast == NULL ? 0 : ast->node_count;
}

size_t mylite_semantic_ast_allocated_bytes(const MyliteSemanticAst *ast) {
  return ast == NULL ? 0 : ast->allocated_bytes;
}

size_t mylite_semantic_ast_statement_count(const MyliteSemanticAst *ast) {
  return ast == NULL ? 0 : ast->statement_count;
}

MyliteSemanticNodeKind mylite_semantic_ast_node_kind(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? MYLITE_SEMANTIC_NODE_UNKNOWN : node->kind;
}

MyliteStatementKind mylite_semantic_ast_node_statement_kind(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? MYLITE_STATEMENT_UNKNOWN : node->statement_kind;
}

MyliteStatementTargetKind mylite_semantic_ast_node_target_kind(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? MYLITE_STATEMENT_TARGET_NONE : node->target_kind;
}

MyliteStatementTargetRole mylite_semantic_ast_node_target_role(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? MYLITE_STATEMENT_TARGET_ROLE_NONE : node->target_role;
}

MyliteExpressionKind mylite_semantic_ast_node_expression_kind(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? MYLITE_EXPRESSION_UNKNOWN : node->expression_kind;
}

MyliteExpressionLiteralKind mylite_semantic_ast_node_expression_literal_kind(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? MYLITE_EXPRESSION_LITERAL_NONE
                      : node->expression_literal_kind;
}

MyliteExpressionOperatorKind mylite_semantic_ast_node_expression_operator_kind(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? MYLITE_EXPRESSION_OPERATOR_NONE
                      : node->expression_operator_kind;
}

size_t mylite_semantic_ast_node_start(const MyliteSemanticAstNode *node) {
  return node == NULL ? 0 : node->start;
}

size_t mylite_semantic_ast_node_end(const MyliteSemanticAstNode *node) {
  return node == NULL ? 0 : node->end;
}

const char *mylite_semantic_ast_node_value(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? NULL : node->value;
}

size_t mylite_semantic_ast_node_value_length(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? 0 : node->value_length;
}

size_t mylite_semantic_ast_node_child_count(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? 0 : node->child_count;
}

const MyliteSemanticAstNode *mylite_semantic_ast_node_child_at(
    const MyliteSemanticAstNode *node, size_t index) {
  if (node == NULL || index >= node->child_count) {
    return NULL;
  }
  return node->children[index];
}

static MyliteSemanticAst *mylite_semantic_ast_create(const char *source) {
  MyliteSemanticAst *ast = calloc(1, sizeof(*ast));
  if (ast == NULL) {
    return NULL;
  }
  if (source != NULL) {
    ast->source_length = strlen(source);
    ast->source = malloc(ast->source_length + 1);
    if (ast->source == NULL) {
      free(ast);
      return NULL;
    }
    memcpy(ast->source, source, ast->source_length + 1);
    ast->allocated_bytes += ast->source_length + 1;
  }
  return ast;
}

static int mylite_semantic_ast_from_parser_ast(MyliteSemanticAst *ast,
                                               const MyliteAst *parser_ast) {
  if (ast == NULL || parser_ast == NULL) {
    return 0;
  }

  size_t statement_count = mylite_ast_statement_count(parser_ast);
  ast->root = mylite_semantic_ast_new_node(
      ast, MYLITE_SEMANTIC_NODE_PROGRAM, 0, 0);
  if (ast->root == NULL) {
    return 0;
  }
  ast->statement_count = statement_count;
  if (!mylite_semantic_ast_set_node_child_count(ast, ast->root,
                                                statement_count)) {
    return 0;
  }

  size_t child_index = 0;
  for (size_t i = 0; i < statement_count; i++) {
    MyliteSemanticAstNode *statement =
        mylite_semantic_ast_materialize_statement(ast, parser_ast, i);
    if (!mylite_semantic_ast_append_child(ast->root, &child_index,
                                          statement)) {
      return 0;
    }
  }
  return child_index == ast->root->child_count;
}

static void *mylite_semantic_ast_alloc(MyliteSemanticAst *ast, size_t size) {
  enum { DEFAULT_CHUNK_SIZE = 4096 };
  if (ast == NULL || size == 0) {
    return NULL;
  }

  size_t alignment = sizeof(void *);
  size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
  if (aligned_size < size) {
    return NULL;
  }

  MyliteSemanticAstChunk *chunk = ast->chunks;
  if (chunk == NULL || chunk->capacity - chunk->used < aligned_size) {
    size_t capacity =
        aligned_size > DEFAULT_CHUNK_SIZE ? aligned_size : DEFAULT_CHUNK_SIZE;
    if (capacity > ~(size_t)0 - sizeof(*chunk)) {
      return NULL;
    }
    chunk = malloc(sizeof(*chunk) + capacity);
    if (chunk == NULL) {
      return NULL;
    }
    chunk->next = ast->chunks;
    chunk->capacity = capacity;
    chunk->used = 0;
    ast->chunks = chunk;
    ast->allocated_bytes += sizeof(*chunk) + capacity;
  }

  void *memory = chunk->data + chunk->used;
  chunk->used += aligned_size;
  memset(memory, 0, aligned_size);
  return memory;
}

static int mylite_semantic_ast_set_node_child_count(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *node, size_t child_count) {
  if (ast == NULL || node == NULL) {
    return 0;
  }
  if (child_count == 0) {
    return 1;
  }
  if (child_count > ~(size_t)0 / sizeof(*node->children)) {
    return 0;
  }
  node->children =
      mylite_semantic_ast_alloc(ast, child_count * sizeof(*node->children));
  if (node->children == NULL) {
    return 0;
  }
  node->child_count = child_count;
  return 1;
}

static MyliteSemanticAstNode *mylite_semantic_ast_materialize_statement(
    MyliteSemanticAst *ast, const MyliteAst *parser_ast,
    size_t statement_index) {
  MyliteSemanticAstNode *statement = mylite_semantic_ast_new_node(
      ast, MYLITE_SEMANTIC_NODE_STATEMENT,
      mylite_ast_statement_start(parser_ast, statement_index),
      mylite_ast_statement_end(parser_ast, statement_index));
  if (statement == NULL) {
    return NULL;
  }
  statement->statement_kind =
      mylite_ast_statement_kind(parser_ast, statement_index);

  size_t child_count =
      mylite_ast_statement_target_count(parser_ast, statement_index) +
      mylite_semantic_ast_count_statement_expression_roots(parser_ast,
                                                           statement_index);
  if (!mylite_semantic_ast_set_node_child_count(ast, statement, child_count)) {
    return NULL;
  }
  if (!mylite_semantic_ast_fill_statement_children(
          ast, statement, parser_ast, statement_index)) {
    return NULL;
  }
  return statement;
}

static size_t mylite_semantic_ast_count_statement_expression_roots(
    const MyliteAst *parser_ast, size_t statement_index) {
  size_t count = 0;

  const MyliteAstSelectStatement *select_statement =
      mylite_ast_select_statement_view(parser_ast, statement_index);
  if (select_statement != NULL) {
    count += mylite_semantic_ast_count_expression_root(
        mylite_ast_select_statement_view_where_expression(select_statement));
    count += mylite_semantic_ast_count_expression_root(
        mylite_ast_select_statement_view_having_expression(select_statement));
    for (size_t i = 0;
         i < mylite_ast_select_statement_view_projection_count(select_statement);
         i++) {
      count += mylite_semantic_ast_count_expression_root(
          mylite_ast_select_projection_view_expression(
              mylite_ast_select_statement_view_projection_at(select_statement,
                                                             i)));
    }
  }

  const MyliteAstValuesStatement *values_statement =
      mylite_ast_values_statement_view(parser_ast, statement_index);
  if (values_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_values_statement_view_value_count(values_statement);
         i++) {
      count += mylite_semantic_ast_count_expression_root(
          mylite_ast_values_value_view_expression(
              mylite_ast_values_statement_view_value_at(values_statement, i)));
    }
  }

  const MyliteAstInsertStatement *insert_statement =
      mylite_ast_insert_statement_view(parser_ast, statement_index);
  if (insert_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_insert_statement_view_value_count(insert_statement);
         i++) {
      count += mylite_semantic_ast_count_expression_root(
          mylite_ast_insert_value_view_expression(
              mylite_ast_insert_statement_view_value_at(insert_statement, i)));
    }
    for (size_t i = 0; i < mylite_ast_insert_statement_view_set_assignment_count(
                               insert_statement);
         i++) {
      count += mylite_semantic_ast_count_expression_root(
          mylite_ast_insert_assignment_view_value_expression(
              mylite_ast_insert_statement_view_set_assignment_at(
                  insert_statement, i)));
    }
    for (size_t i = 0;
         i < mylite_ast_insert_statement_view_duplicate_assignment_count(
                 insert_statement);
         i++) {
      count += mylite_semantic_ast_count_expression_root(
          mylite_ast_insert_assignment_view_value_expression(
              mylite_ast_insert_statement_view_duplicate_assignment_at(
                  insert_statement, i)));
    }
  }

  const MyliteAstReplaceStatement *replace_statement =
      mylite_ast_replace_statement_view(parser_ast, statement_index);
  if (replace_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_replace_statement_view_value_count(replace_statement);
         i++) {
      count += mylite_semantic_ast_count_expression_root(
          mylite_ast_replace_value_view_expression(
              mylite_ast_replace_statement_view_value_at(replace_statement,
                                                         i)));
    }
    for (size_t i = 0;
         i < mylite_ast_replace_statement_view_set_assignment_count(
                 replace_statement);
         i++) {
      count += mylite_semantic_ast_count_expression_root(
          mylite_ast_replace_assignment_view_value_expression(
              mylite_ast_replace_statement_view_set_assignment_at(
                  replace_statement, i)));
    }
  }

  const MyliteAstUpdateStatement *update_statement =
      mylite_ast_update_statement_view(parser_ast, statement_index);
  if (update_statement != NULL) {
    count += mylite_semantic_ast_count_expression_root(
        mylite_ast_update_statement_view_where_expression(update_statement));
    for (size_t i = 0;
         i < mylite_ast_update_statement_view_assignment_count(update_statement);
         i++) {
      count += mylite_semantic_ast_count_expression_root(
          mylite_ast_update_assignment_view_value_expression(
              mylite_ast_update_statement_view_assignment_at(update_statement,
                                                             i)));
    }
  }

  const MyliteAstDeleteStatement *delete_statement =
      mylite_ast_delete_statement_view(parser_ast, statement_index);
  if (delete_statement != NULL) {
    count += mylite_semantic_ast_count_expression_root(
        mylite_ast_delete_statement_view_where_expression(delete_statement));
  }

  const MyliteAstCallStatement *call_statement =
      mylite_ast_call_statement_view(parser_ast, statement_index);
  if (call_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_call_statement_view_argument_count(call_statement);
         i++) {
      count += mylite_semantic_ast_count_expression_root(
          mylite_ast_call_argument_view_expression(
              mylite_ast_call_statement_view_argument_at(call_statement, i)));
    }
  }

  const MyliteAstDoStatement *do_statement =
      mylite_ast_do_statement_view(parser_ast, statement_index);
  if (do_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_do_statement_view_expression_count(do_statement); i++) {
      count += mylite_semantic_ast_count_expression_root(
          mylite_ast_do_expression_view_expression(
              mylite_ast_do_statement_view_expression_at(do_statement, i)));
    }
  }

  const MyliteAstSetStatement *set_statement =
      mylite_ast_set_statement_view(parser_ast, statement_index);
  if (set_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_set_statement_view_assignment_count(set_statement);
         i++) {
      count += mylite_semantic_ast_count_expression_root(
          mylite_ast_set_assignment_view_value_expression(
              mylite_ast_set_statement_view_assignment_at(set_statement, i)));
    }
  }

  const MyliteAstCreateTable *create_table =
      mylite_ast_create_table_view(parser_ast, statement_index);
  if (create_table != NULL) {
    for (size_t i = 0; i < mylite_ast_create_table_view_column_count(create_table);
         i++) {
      count += mylite_semantic_ast_count_create_table_column_expressions(
          mylite_ast_create_table_view_column_at(create_table, i));
    }
    for (size_t i = 0; i < mylite_ast_create_table_view_key_count(create_table);
         i++) {
      count += mylite_semantic_ast_count_create_table_key_expressions(
          mylite_ast_create_table_view_key_at(create_table, i));
    }
  }

  const MyliteAstAlterTable *alter_table =
      mylite_ast_alter_table_view(parser_ast, statement_index);
  if (alter_table != NULL) {
    for (size_t i = 0; i < mylite_ast_alter_table_view_spec_count(alter_table);
         i++) {
      const MyliteAstAlterTableSpec *spec =
          mylite_ast_alter_table_view_spec_at(alter_table, i);
      for (size_t j = 0;
           j < mylite_ast_alter_table_spec_view_column_count(spec); j++) {
        count += mylite_semantic_ast_count_create_table_column_expressions(
            mylite_ast_alter_table_spec_view_column_at(spec, j));
      }
      for (size_t j = 0; j < mylite_ast_alter_table_spec_view_key_count(spec);
           j++) {
        count += mylite_semantic_ast_count_create_table_key_expressions(
            mylite_ast_alter_table_spec_view_key_at(spec, j));
      }
    }
  }

  const MyliteAstCreateIndex *create_index =
      mylite_ast_create_index_view(parser_ast, statement_index);
  if (create_index != NULL) {
    for (size_t i = 0; i < mylite_ast_create_index_view_column_count(create_index);
         i++) {
      count += mylite_semantic_ast_count_expression_root(
          mylite_ast_create_table_key_part_view_expression(
              mylite_ast_create_index_view_column_at(create_index, i)));
    }
  }

  return count;
}

static int mylite_semantic_ast_fill_statement_children(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *statement,
    const MyliteAst *parser_ast, size_t statement_index) {
  size_t child_index = 0;
  size_t target_count =
      mylite_ast_statement_target_count(parser_ast, statement_index);
  for (size_t i = 0; i < target_count; i++) {
    if (!mylite_semantic_ast_append_child(
            statement, &child_index,
            mylite_semantic_ast_materialize_target(ast, parser_ast,
                                                   statement_index, i))) {
      return 0;
    }
  }
  return mylite_semantic_ast_fill_statement_expression_roots(
             ast, statement, parser_ast, statement_index, &child_index) &&
         child_index == statement->child_count;
}

static MyliteSemanticAstNode *mylite_semantic_ast_materialize_target(
    MyliteSemanticAst *ast, const MyliteAst *parser_ast,
    size_t statement_index, size_t target_index) {
  MyliteSemanticAstNode *target = mylite_semantic_ast_new_node(
      ast, MYLITE_SEMANTIC_NODE_TARGET,
      mylite_ast_statement_target_start_at(parser_ast, statement_index,
                                           target_index),
      mylite_ast_statement_target_end_at(parser_ast, statement_index,
                                         target_index));
  if (target == NULL) {
    return NULL;
  }
  target->target_kind = mylite_ast_statement_target_kind_at(
      parser_ast, statement_index, target_index);
  target->target_role = mylite_ast_statement_target_role_at(
      parser_ast, statement_index, target_index);

  const char *value =
      mylite_ast_statement_target_name_value_at(parser_ast, statement_index,
                                                target_index);
  size_t value_length =
      mylite_ast_statement_target_name_value_length_at(parser_ast,
                                                       statement_index,
                                                       target_index);
  if (value == NULL) {
    value = mylite_ast_statement_target_schema_value_at(
        parser_ast, statement_index, target_index);
    value_length = mylite_ast_statement_target_schema_value_length_at(
        parser_ast, statement_index, target_index);
  }
  if (!mylite_semantic_ast_copy_node_value(ast, target, value, value_length)) {
    return NULL;
  }
  return target;
}

static int mylite_semantic_ast_fill_statement_expression_roots(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *statement,
    const MyliteAst *parser_ast, size_t statement_index, size_t *child_index) {
  const MyliteAstSelectStatement *select_statement =
      mylite_ast_select_statement_view(parser_ast, statement_index);
  if (select_statement != NULL) {
    if (!mylite_semantic_ast_append_expression_child(
            ast, statement, child_index,
            mylite_ast_select_statement_view_where_expression(select_statement)) ||
        !mylite_semantic_ast_append_expression_child(
            ast, statement, child_index,
            mylite_ast_select_statement_view_having_expression(select_statement))) {
      return 0;
    }
    for (size_t i = 0;
         i < mylite_ast_select_statement_view_projection_count(select_statement);
         i++) {
      if (!mylite_semantic_ast_append_expression_child(
              ast, statement, child_index,
              mylite_ast_select_projection_view_expression(
                  mylite_ast_select_statement_view_projection_at(
                      select_statement, i)))) {
        return 0;
      }
    }
  }

  const MyliteAstValuesStatement *values_statement =
      mylite_ast_values_statement_view(parser_ast, statement_index);
  if (values_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_values_statement_view_value_count(values_statement);
         i++) {
      if (!mylite_semantic_ast_append_expression_child(
              ast, statement, child_index,
              mylite_ast_values_value_view_expression(
                  mylite_ast_values_statement_view_value_at(values_statement,
                                                            i)))) {
        return 0;
      }
    }
  }

  const MyliteAstInsertStatement *insert_statement =
      mylite_ast_insert_statement_view(parser_ast, statement_index);
  if (insert_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_insert_statement_view_value_count(insert_statement);
         i++) {
      if (!mylite_semantic_ast_append_expression_child(
              ast, statement, child_index,
              mylite_ast_insert_value_view_expression(
                  mylite_ast_insert_statement_view_value_at(insert_statement,
                                                            i)))) {
        return 0;
      }
    }
    for (size_t i = 0; i < mylite_ast_insert_statement_view_set_assignment_count(
                               insert_statement);
         i++) {
      if (!mylite_semantic_ast_append_expression_child(
              ast, statement, child_index,
              mylite_ast_insert_assignment_view_value_expression(
                  mylite_ast_insert_statement_view_set_assignment_at(
                      insert_statement, i)))) {
        return 0;
      }
    }
    for (size_t i = 0;
         i < mylite_ast_insert_statement_view_duplicate_assignment_count(
                 insert_statement);
         i++) {
      if (!mylite_semantic_ast_append_expression_child(
              ast, statement, child_index,
              mylite_ast_insert_assignment_view_value_expression(
                  mylite_ast_insert_statement_view_duplicate_assignment_at(
                      insert_statement, i)))) {
        return 0;
      }
    }
  }

  const MyliteAstReplaceStatement *replace_statement =
      mylite_ast_replace_statement_view(parser_ast, statement_index);
  if (replace_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_replace_statement_view_value_count(replace_statement);
         i++) {
      if (!mylite_semantic_ast_append_expression_child(
              ast, statement, child_index,
              mylite_ast_replace_value_view_expression(
                  mylite_ast_replace_statement_view_value_at(replace_statement,
                                                             i)))) {
        return 0;
      }
    }
    for (size_t i = 0;
         i < mylite_ast_replace_statement_view_set_assignment_count(
                 replace_statement);
         i++) {
      if (!mylite_semantic_ast_append_expression_child(
              ast, statement, child_index,
              mylite_ast_replace_assignment_view_value_expression(
                  mylite_ast_replace_statement_view_set_assignment_at(
                      replace_statement, i)))) {
        return 0;
      }
    }
  }

  const MyliteAstUpdateStatement *update_statement =
      mylite_ast_update_statement_view(parser_ast, statement_index);
  if (update_statement != NULL) {
    if (!mylite_semantic_ast_append_expression_child(
            ast, statement, child_index,
            mylite_ast_update_statement_view_where_expression(update_statement))) {
      return 0;
    }
    for (size_t i = 0;
         i < mylite_ast_update_statement_view_assignment_count(update_statement);
         i++) {
      if (!mylite_semantic_ast_append_expression_child(
              ast, statement, child_index,
              mylite_ast_update_assignment_view_value_expression(
                  mylite_ast_update_statement_view_assignment_at(
                      update_statement, i)))) {
        return 0;
      }
    }
  }

  const MyliteAstDeleteStatement *delete_statement =
      mylite_ast_delete_statement_view(parser_ast, statement_index);
  if (delete_statement != NULL &&
      !mylite_semantic_ast_append_expression_child(
          ast, statement, child_index,
          mylite_ast_delete_statement_view_where_expression(delete_statement))) {
    return 0;
  }

  const MyliteAstCallStatement *call_statement =
      mylite_ast_call_statement_view(parser_ast, statement_index);
  if (call_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_call_statement_view_argument_count(call_statement);
         i++) {
      if (!mylite_semantic_ast_append_expression_child(
              ast, statement, child_index,
              mylite_ast_call_argument_view_expression(
                  mylite_ast_call_statement_view_argument_at(call_statement,
                                                             i)))) {
        return 0;
      }
    }
  }

  const MyliteAstDoStatement *do_statement =
      mylite_ast_do_statement_view(parser_ast, statement_index);
  if (do_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_do_statement_view_expression_count(do_statement); i++) {
      if (!mylite_semantic_ast_append_expression_child(
              ast, statement, child_index,
              mylite_ast_do_expression_view_expression(
                  mylite_ast_do_statement_view_expression_at(do_statement,
                                                             i)))) {
        return 0;
      }
    }
  }

  const MyliteAstSetStatement *set_statement =
      mylite_ast_set_statement_view(parser_ast, statement_index);
  if (set_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_set_statement_view_assignment_count(set_statement);
         i++) {
      if (!mylite_semantic_ast_append_expression_child(
              ast, statement, child_index,
              mylite_ast_set_assignment_view_value_expression(
                  mylite_ast_set_statement_view_assignment_at(set_statement,
                                                              i)))) {
        return 0;
      }
    }
  }

  const MyliteAstCreateTable *create_table =
      mylite_ast_create_table_view(parser_ast, statement_index);
  if (create_table != NULL) {
    for (size_t i = 0; i < mylite_ast_create_table_view_column_count(create_table);
         i++) {
      if (!mylite_semantic_ast_append_create_table_column_expressions(
              ast, statement, child_index,
              mylite_ast_create_table_view_column_at(create_table, i))) {
        return 0;
      }
    }
    for (size_t i = 0; i < mylite_ast_create_table_view_key_count(create_table);
         i++) {
      if (!mylite_semantic_ast_append_create_table_key_expressions(
              ast, statement, child_index,
              mylite_ast_create_table_view_key_at(create_table, i))) {
        return 0;
      }
    }
  }

  const MyliteAstAlterTable *alter_table =
      mylite_ast_alter_table_view(parser_ast, statement_index);
  if (alter_table != NULL) {
    for (size_t i = 0; i < mylite_ast_alter_table_view_spec_count(alter_table);
         i++) {
      const MyliteAstAlterTableSpec *spec =
          mylite_ast_alter_table_view_spec_at(alter_table, i);
      for (size_t j = 0;
           j < mylite_ast_alter_table_spec_view_column_count(spec); j++) {
        if (!mylite_semantic_ast_append_create_table_column_expressions(
                ast, statement, child_index,
                mylite_ast_alter_table_spec_view_column_at(spec, j))) {
          return 0;
        }
      }
      for (size_t j = 0; j < mylite_ast_alter_table_spec_view_key_count(spec);
           j++) {
        if (!mylite_semantic_ast_append_create_table_key_expressions(
                ast, statement, child_index,
                mylite_ast_alter_table_spec_view_key_at(spec, j))) {
          return 0;
        }
      }
    }
  }

  const MyliteAstCreateIndex *create_index =
      mylite_ast_create_index_view(parser_ast, statement_index);
  if (create_index != NULL) {
    for (size_t i = 0; i < mylite_ast_create_index_view_column_count(create_index);
         i++) {
      if (!mylite_semantic_ast_append_expression_child(
              ast, statement, child_index,
              mylite_ast_create_table_key_part_view_expression(
                  mylite_ast_create_index_view_column_at(create_index, i)))) {
        return 0;
      }
    }
  }

  return 1;
}

static MyliteSemanticAstNode *mylite_semantic_ast_materialize_expression(
    MyliteSemanticAst *ast, const MyliteAstExpression *expression) {
  if (expression == NULL) {
    return NULL;
  }
  MyliteSemanticAstNode *node = mylite_semantic_ast_new_node(
      ast, MYLITE_SEMANTIC_NODE_EXPRESSION,
      mylite_ast_expression_view_start(expression),
      mylite_ast_expression_view_end(expression));
  if (node == NULL) {
    return NULL;
  }
  node->expression_kind = mylite_ast_expression_view_kind(expression);
  node->expression_literal_kind =
      mylite_ast_expression_view_literal_kind(expression);
  node->expression_operator_kind =
      mylite_ast_expression_view_operator_kind(expression);
  if (!mylite_semantic_ast_copy_node_value(
          ast, node, mylite_ast_expression_view_value(expression),
          mylite_ast_expression_view_value_length(expression))) {
    return NULL;
  }

  size_t child_count = mylite_ast_expression_view_child_count(expression);
  if (!mylite_semantic_ast_set_node_child_count(ast, node, child_count)) {
    return NULL;
  }
  size_t child_index = 0;
  for (size_t i = 0; i < child_count; i++) {
    MyliteSemanticAstNode *child = mylite_semantic_ast_materialize_expression(
        ast, mylite_ast_expression_view_child_at(expression, i));
    if (!mylite_semantic_ast_append_child(node, &child_index, child)) {
      return NULL;
    }
  }
  return child_index == node->child_count ? node : NULL;
}

static MyliteSemanticAstNode *mylite_semantic_ast_new_node(
    MyliteSemanticAst *ast, MyliteSemanticNodeKind kind, size_t start,
    size_t end) {
  MyliteSemanticAstNode *node =
      mylite_semantic_ast_alloc(ast, sizeof(*node));
  if (node == NULL) {
    return NULL;
  }
  node->kind = kind;
  node->start = start;
  node->end = end;
  ast->node_count++;
  return node;
}

static int mylite_semantic_ast_append_child(MyliteSemanticAstNode *parent,
                                            size_t *index,
                                            MyliteSemanticAstNode *child) {
  if (parent == NULL || index == NULL || child == NULL ||
      *index >= parent->child_count) {
    return 0;
  }
  parent->children[*index] = child;
  (*index)++;
  return 1;
}

static int mylite_semantic_ast_copy_node_value(MyliteSemanticAst *ast,
                                               MyliteSemanticAstNode *node,
                                               const char *value,
                                               size_t value_length) {
  if (ast == NULL || node == NULL) {
    return 0;
  }
  if (value == NULL) {
    return 1;
  }
  if (value_length == ~(size_t)0) {
    return 0;
  }
  char *copy = mylite_semantic_ast_alloc(ast, value_length + 1);
  if (copy == NULL) {
    return 0;
  }
  memcpy(copy, value, value_length);
  copy[value_length] = '\0';
  node->value = copy;
  node->value_length = value_length;
  return 1;
}

static size_t mylite_semantic_ast_count_expression_root(
    const MyliteAstExpression *expression) {
  return expression == NULL ? 0 : 1;
}

static int mylite_semantic_ast_append_expression_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    const MyliteAstExpression *expression) {
  if (expression == NULL) {
    return 1;
  }
  return mylite_semantic_ast_append_child(
      parent, index, mylite_semantic_ast_materialize_expression(ast, expression));
}

static size_t mylite_semantic_ast_count_create_table_column_expressions(
    const MyliteAstCreateTableColumn *column) {
  if (column == NULL) {
    return 0;
  }
  return mylite_semantic_ast_count_expression_root(
             mylite_ast_create_table_column_view_default_value_expression(
                 column)) +
         mylite_semantic_ast_count_expression_root(
             mylite_ast_create_table_column_view_on_update_value_expression(
                 column)) +
         mylite_semantic_ast_count_expression_root(
             mylite_ast_create_table_column_view_generated_expression(column)) +
         mylite_semantic_ast_count_expression_root(
             mylite_ast_create_table_column_view_check_expression(column));
}

static int mylite_semantic_ast_append_create_table_column_expressions(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    const MyliteAstCreateTableColumn *column) {
  if (column == NULL) {
    return 1;
  }
  return mylite_semantic_ast_append_expression_child(
             ast, parent, index,
             mylite_ast_create_table_column_view_default_value_expression(
                 column)) &&
         mylite_semantic_ast_append_expression_child(
             ast, parent, index,
             mylite_ast_create_table_column_view_on_update_value_expression(
                 column)) &&
         mylite_semantic_ast_append_expression_child(
             ast, parent, index,
             mylite_ast_create_table_column_view_generated_expression(column)) &&
         mylite_semantic_ast_append_expression_child(
             ast, parent, index,
             mylite_ast_create_table_column_view_check_expression(column));
}

static size_t mylite_semantic_ast_count_create_table_key_expressions(
    const MyliteAstCreateTableKey *key) {
  if (key == NULL) {
    return 0;
  }
  size_t count = mylite_semantic_ast_count_expression_root(
      mylite_ast_create_table_key_view_check_expression(key));
  for (size_t i = 0; i < mylite_ast_create_table_key_view_column_count(key);
       i++) {
    count += mylite_semantic_ast_count_expression_root(
        mylite_ast_create_table_key_part_view_expression(
            mylite_ast_create_table_key_view_column_at(key, i)));
  }
  return count;
}

static int mylite_semantic_ast_append_create_table_key_expressions(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    const MyliteAstCreateTableKey *key) {
  if (key == NULL) {
    return 1;
  }
  if (!mylite_semantic_ast_append_expression_child(
          ast, parent, index,
          mylite_ast_create_table_key_view_check_expression(key))) {
    return 0;
  }
  for (size_t i = 0; i < mylite_ast_create_table_key_view_column_count(key);
       i++) {
    if (!mylite_semantic_ast_append_expression_child(
            ast, parent, index,
            mylite_ast_create_table_key_part_view_expression(
                mylite_ast_create_table_key_view_column_at(key, i)))) {
      return 0;
    }
  }
  return 1;
}

static void mylite_semantic_ast_set_no_memory(MyliteParseResult *result,
                                              const char *message) {
  if (result == NULL) {
    return;
  }
  memset(result, 0, sizeof(*result));
  result->status = MYLITE_PARSE_NO_MEMORY;
  snprintf(result->message, sizeof(result->message), "%s", message);
}
