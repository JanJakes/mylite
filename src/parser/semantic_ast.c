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
  size_t query_block_count;
  int query_has_with_clause;
  int query_has_set_operation;
  MyliteSemanticClauseKind clause_kind;
  MyliteSemanticDescriptorKind descriptor_kind;
  MyliteCreateTableColumnTypeFamily data_type_family;
  MyliteCreateTableColumnTypeKind data_type_kind;
  MyliteCreateTableColumnStorageClass data_type_storage_class;
  unsigned int data_type_flags;
  size_t data_type_numeric_parameter_count;
  unsigned long long data_type_numeric_parameters[2];
  MyliteSemanticDataTypeAttributeKind data_type_attribute_kind;
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
static size_t mylite_semantic_ast_count_statement_queries(
    const MyliteAst *parser_ast, size_t statement_index);
static size_t mylite_semantic_ast_count_statement_descriptors(
    const MyliteAst *parser_ast, size_t statement_index);
static size_t mylite_semantic_ast_count_statement_clauses(
    const MyliteAst *parser_ast, size_t statement_index);
static int mylite_semantic_ast_fill_statement_children(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *statement,
    const MyliteAst *parser_ast, size_t statement_index);
static int mylite_semantic_ast_fill_statement_queries(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *statement,
    const MyliteAst *parser_ast, size_t statement_index, size_t *child_index);
static MyliteSemanticAstNode *mylite_semantic_ast_materialize_select_query(
    MyliteSemanticAst *ast, const MyliteAstSelectStatement *select_statement);
static size_t mylite_semantic_ast_count_select_query_children(
    const MyliteAstSelectStatement *select_statement);
static size_t mylite_semantic_ast_count_select_query_clauses(
    const MyliteAstSelectStatement *select_statement);
static int mylite_semantic_ast_fill_select_query_children(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *query,
    const MyliteAstSelectStatement *select_statement);
static int mylite_semantic_ast_append_select_projection_descriptors(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *query,
    size_t *child_index, const MyliteAstSelectStatement *select_statement);
static int mylite_semantic_ast_append_select_clauses(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *query, size_t *child_index,
    const MyliteAstSelectStatement *select_statement);
static int mylite_semantic_ast_append_select_from_clause_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *query, size_t *child_index,
    size_t start, size_t end);
static MyliteSemanticAstNode *mylite_semantic_ast_materialize_values_query(
    MyliteSemanticAst *ast, const MyliteAstValuesStatement *values_statement);
static size_t mylite_semantic_ast_count_values_query_children(
    const MyliteAstValuesStatement *values_statement);
static size_t mylite_semantic_ast_count_values_query_clauses(
    const MyliteAstValuesStatement *values_statement);
static int mylite_semantic_ast_fill_values_query_children(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *query,
    const MyliteAstValuesStatement *values_statement);
static int mylite_semantic_ast_append_values_descriptors(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *query,
    size_t *child_index, const MyliteAstValuesStatement *values_statement);
static int mylite_semantic_ast_append_values_clauses(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *query, size_t *child_index,
    const MyliteAstValuesStatement *values_statement);
static MyliteSemanticAstNode *mylite_semantic_ast_materialize_table_reference(
    MyliteSemanticAst *ast, size_t start, size_t end);
static MyliteSemanticAstNode *mylite_semantic_ast_materialize_target(
    MyliteSemanticAst *ast, const MyliteAst *parser_ast,
    size_t statement_index, size_t target_index);
static int mylite_semantic_ast_fill_statement_clauses(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *statement,
    const MyliteAst *parser_ast, size_t statement_index, size_t *child_index);
static int mylite_semantic_ast_append_clause_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    MyliteSemanticClauseKind kind, size_t start, size_t end,
    const MyliteAstExpression *expression);
static int mylite_semantic_ast_append_clause_span_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    MyliteSemanticClauseKind kind, size_t start, size_t end);
static MyliteSemanticAstNode *mylite_semantic_ast_materialize_clause(
    MyliteSemanticAst *ast, MyliteSemanticClauseKind kind, size_t start,
    size_t end, const MyliteAstExpression *expression);
static int mylite_semantic_ast_fill_statement_descriptors(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *statement,
    const MyliteAst *parser_ast, size_t statement_index, size_t *child_index);
static int mylite_semantic_ast_append_create_table_column_descriptor(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    const MyliteAstCreateTableColumn *column);
static int mylite_semantic_ast_append_create_table_column_type_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *descriptor, size_t *index,
    const MyliteAstCreateTableColumn *column);
static MyliteSemanticAstNode *mylite_semantic_ast_materialize_data_type(
    MyliteSemanticAst *ast, const MyliteAstCreateTableColumn *column);
static int mylite_semantic_ast_fill_data_type_children(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *data_type,
    const MyliteAstCreateTableColumn *column);
static MyliteSemanticAstNode *mylite_semantic_ast_materialize_data_type_element(
    MyliteSemanticAst *ast,
    const MyliteAstCreateTableColumnTypeElement *element);
static int mylite_semantic_ast_append_data_type_attribute_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *data_type, size_t *index,
    MyliteSemanticDataTypeAttributeKind kind, size_t start, size_t end,
    const char *value, size_t value_length);
static MyliteSemanticAstNode *
mylite_semantic_ast_materialize_data_type_attribute(
    MyliteSemanticAst *ast, MyliteSemanticDataTypeAttributeKind kind,
    size_t start, size_t end, const char *value, size_t value_length);
static int mylite_semantic_ast_append_create_table_key_descriptors(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    const MyliteAstCreateTableKey *key);
static int mylite_semantic_ast_append_descriptor_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    MyliteSemanticDescriptorKind kind, size_t start, size_t end,
    const char *value, size_t value_length);
static int mylite_semantic_ast_append_descriptor_expression_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *descriptor, size_t *index,
    const MyliteAstExpression *expression);
static int mylite_semantic_ast_append_descriptor_with_expression_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    MyliteSemanticDescriptorKind kind, size_t start, size_t end,
    const char *value, size_t value_length,
    const MyliteAstExpression *expression);
static MyliteSemanticAstNode *mylite_semantic_ast_materialize_descriptor(
    MyliteSemanticAst *ast, MyliteSemanticDescriptorKind kind, size_t start,
    size_t end, const char *value, size_t value_length, size_t child_count);
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
static int mylite_semantic_ast_copy_source_span_value(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *node, size_t start,
    size_t end);
static size_t mylite_semantic_ast_count_expression_root(
    const MyliteAstExpression *expression);
static size_t mylite_semantic_ast_count_clause_span(size_t start, size_t end);
static size_t mylite_semantic_ast_count_create_table_column_data_type(
    const MyliteAstCreateTableColumn *column);
static size_t mylite_semantic_ast_count_create_table_column_data_type_attributes(
    const MyliteAstCreateTableColumn *column);
static int mylite_semantic_ast_append_expression_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    const MyliteAstExpression *expression);
static size_t mylite_semantic_ast_count_create_table_column_expressions(
    const MyliteAstCreateTableColumn *column);
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

size_t mylite_semantic_ast_node_query_block_count(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? 0 : node->query_block_count;
}

int mylite_semantic_ast_node_query_has_with_clause(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? 0 : node->query_has_with_clause;
}

int mylite_semantic_ast_node_query_has_set_operation(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? 0 : node->query_has_set_operation;
}

const char *mylite_semantic_data_type_attribute_kind_name(
    MyliteSemanticDataTypeAttributeKind kind) {
  switch (kind) {
  case MYLITE_SEMANTIC_DATA_TYPE_ATTRIBUTE_UNKNOWN:
    return "unknown";
  case MYLITE_SEMANTIC_DATA_TYPE_ATTRIBUTE_UNSIGNED:
    return "unsigned";
  case MYLITE_SEMANTIC_DATA_TYPE_ATTRIBUTE_ZEROFILL:
    return "zerofill";
  case MYLITE_SEMANTIC_DATA_TYPE_ATTRIBUTE_BINARY:
    return "binary";
  case MYLITE_SEMANTIC_DATA_TYPE_ATTRIBUTE_CHARSET:
    return "charset";
  case MYLITE_SEMANTIC_DATA_TYPE_ATTRIBUTE_COLLATION:
    return "collation";
  }
  return "unknown";
}

const char *mylite_semantic_clause_kind_name(MyliteSemanticClauseKind kind) {
  switch (kind) {
  case MYLITE_SEMANTIC_CLAUSE_UNKNOWN:
    return "unknown";
  case MYLITE_SEMANTIC_CLAUSE_WHERE:
    return "where";
  case MYLITE_SEMANTIC_CLAUSE_HAVING:
    return "having";
  case MYLITE_SEMANTIC_CLAUSE_SHOW_LIKE:
    return "show_like";
  case MYLITE_SEMANTIC_CLAUSE_SHOW_WHERE:
    return "show_where";
  case MYLITE_SEMANTIC_CLAUSE_KILL_TARGET:
    return "kill_target";
  case MYLITE_SEMANTIC_CLAUSE_CALL_ARGUMENT:
    return "call_argument";
  case MYLITE_SEMANTIC_CLAUSE_DO_EXPRESSION:
    return "do_expression";
  case MYLITE_SEMANTIC_CLAUSE_FROM:
    return "from";
  case MYLITE_SEMANTIC_CLAUSE_GROUP_BY:
    return "group_by";
  case MYLITE_SEMANTIC_CLAUSE_ORDER_BY:
    return "order_by";
  case MYLITE_SEMANTIC_CLAUSE_LIMIT:
    return "limit";
  case MYLITE_SEMANTIC_CLAUSE_INTO:
    return "into";
  case MYLITE_SEMANTIC_CLAUSE_LOCKING:
    return "locking";
  case MYLITE_SEMANTIC_CLAUSE_TABLE_REFERENCE:
    return "table_reference";
  }
  return "unknown";
}

MyliteSemanticClauseKind mylite_semantic_ast_node_clause_kind(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? MYLITE_SEMANTIC_CLAUSE_UNKNOWN : node->clause_kind;
}

const char *mylite_semantic_descriptor_kind_name(
    MyliteSemanticDescriptorKind kind) {
  switch (kind) {
  case MYLITE_SEMANTIC_DESCRIPTOR_UNKNOWN:
    return "unknown";
  case MYLITE_SEMANTIC_DESCRIPTOR_PROJECTION:
    return "projection";
  case MYLITE_SEMANTIC_DESCRIPTOR_VALUE:
    return "value";
  case MYLITE_SEMANTIC_DESCRIPTOR_ASSIGNMENT:
    return "assignment";
  case MYLITE_SEMANTIC_DESCRIPTOR_COLUMN:
    return "column";
  case MYLITE_SEMANTIC_DESCRIPTOR_KEY:
    return "key";
  case MYLITE_SEMANTIC_DESCRIPTOR_KEY_PART:
    return "key_part";
  case MYLITE_SEMANTIC_DESCRIPTOR_OPTION:
    return "option";
  case MYLITE_SEMANTIC_DESCRIPTOR_DATABASE_OPTION:
    return "database_option";
  case MYLITE_SEMANTIC_DESCRIPTOR_VIEW_COLUMN:
    return "view_column";
  case MYLITE_SEMANTIC_DESCRIPTOR_ALTER_TABLE_SPEC:
    return "alter_table_spec";
  case MYLITE_SEMANTIC_DESCRIPTOR_TABLE_LOCK:
    return "table_lock";
  case MYLITE_SEMANTIC_DESCRIPTOR_TABLE_MAINTENANCE_TARGET:
    return "table_maintenance_target";
  case MYLITE_SEMANTIC_DESCRIPTOR_REPLICATION_OPTION:
    return "replication_option";
  case MYLITE_SEMANTIC_DESCRIPTOR_STORED_OBJECT:
    return "stored_object";
  case MYLITE_SEMANTIC_DESCRIPTOR_FLUSH_TARGET:
    return "flush_target";
  case MYLITE_SEMANTIC_DESCRIPTOR_FLUSH_PLUGIN:
    return "flush_plugin";
  case MYLITE_SEMANTIC_DESCRIPTOR_LOAD_ITEM:
    return "load_item";
  case MYLITE_SEMANTIC_DESCRIPTOR_LOAD_ASSIGNMENT:
    return "load_assignment";
  case MYLITE_SEMANTIC_DESCRIPTOR_LOAD_OPTION:
    return "load_option";
  case MYLITE_SEMANTIC_DESCRIPTOR_ACCOUNT:
    return "account";
  case MYLITE_SEMANTIC_DESCRIPTOR_PRIVILEGE_ITEM:
    return "privilege_item";
  case MYLITE_SEMANTIC_DESCRIPTOR_ROLE:
    return "role";
  case MYLITE_SEMANTIC_DESCRIPTOR_PREPARED_VARIABLE:
    return "prepared_variable";
  }
  return "unknown";
}

MyliteSemanticDescriptorKind mylite_semantic_ast_node_descriptor_kind(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? MYLITE_SEMANTIC_DESCRIPTOR_UNKNOWN
                      : node->descriptor_kind;
}

MyliteCreateTableColumnTypeFamily
mylite_semantic_ast_node_data_type_family(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? MYLITE_CREATE_TABLE_COLUMN_TYPE_UNKNOWN
                      : node->data_type_family;
}

MyliteCreateTableColumnTypeKind mylite_semantic_ast_node_data_type_kind(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_UNKNOWN
                      : node->data_type_kind;
}

MyliteCreateTableColumnStorageClass
mylite_semantic_ast_node_data_type_storage_class(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? MYLITE_CREATE_TABLE_COLUMN_STORAGE_UNKNOWN
                      : node->data_type_storage_class;
}

unsigned int mylite_semantic_ast_node_data_type_flags(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? 0 : node->data_type_flags;
}

size_t mylite_semantic_ast_node_data_type_numeric_parameter_count(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? 0 : node->data_type_numeric_parameter_count;
}

unsigned long long
mylite_semantic_ast_node_data_type_numeric_parameter_at(
    const MyliteSemanticAstNode *node, size_t parameter_index) {
  if (node == NULL ||
      parameter_index >= node->data_type_numeric_parameter_count ||
      parameter_index >= sizeof(node->data_type_numeric_parameters) /
                             sizeof(node->data_type_numeric_parameters[0])) {
    return 0;
  }
  return node->data_type_numeric_parameters[parameter_index];
}

MyliteSemanticDataTypeAttributeKind
mylite_semantic_ast_node_data_type_attribute_kind(
    const MyliteSemanticAstNode *node) {
  return node == NULL ? MYLITE_SEMANTIC_DATA_TYPE_ATTRIBUTE_UNKNOWN
                      : node->data_type_attribute_kind;
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
      mylite_semantic_ast_count_statement_queries(parser_ast,
                                                  statement_index) +
      mylite_semantic_ast_count_statement_descriptors(parser_ast,
                                                      statement_index) +
      mylite_semantic_ast_count_statement_clauses(parser_ast, statement_index);
  if (!mylite_semantic_ast_set_node_child_count(ast, statement, child_count)) {
    return NULL;
  }
  if (!mylite_semantic_ast_fill_statement_children(
          ast, statement, parser_ast, statement_index)) {
    return NULL;
  }
  return statement;
}

static size_t mylite_semantic_ast_count_statement_queries(
    const MyliteAst *parser_ast, size_t statement_index) {
  return mylite_ast_select_statement_view(parser_ast, statement_index) != NULL ||
                 mylite_ast_values_statement_view(parser_ast,
                                                  statement_index) != NULL
             ? 1
             : 0;
}

static size_t mylite_semantic_ast_count_statement_descriptors(
    const MyliteAst *parser_ast, size_t statement_index) {
  size_t count = 0;

  const MyliteAstInsertStatement *insert_statement =
      mylite_ast_insert_statement_view(parser_ast, statement_index);
  if (insert_statement != NULL) {
    count += mylite_ast_insert_statement_view_column_count(insert_statement);
    count += mylite_ast_insert_statement_view_value_count(insert_statement);
    count +=
        mylite_ast_insert_statement_view_set_assignment_count(insert_statement);
    count += mylite_ast_insert_statement_view_duplicate_assignment_count(
        insert_statement);
  }

  const MyliteAstReplaceStatement *replace_statement =
      mylite_ast_replace_statement_view(parser_ast, statement_index);
  if (replace_statement != NULL) {
    count += mylite_ast_replace_statement_view_column_count(replace_statement);
    count += mylite_ast_replace_statement_view_value_count(replace_statement);
    count += mylite_ast_replace_statement_view_set_assignment_count(
        replace_statement);
  }

  const MyliteAstUpdateStatement *update_statement =
      mylite_ast_update_statement_view(parser_ast, statement_index);
  if (update_statement != NULL) {
    count += mylite_ast_update_statement_view_assignment_count(update_statement);
  }

  const MyliteAstSetStatement *set_statement =
      mylite_ast_set_statement_view(parser_ast, statement_index);
  if (set_statement != NULL) {
    count += mylite_ast_set_statement_view_assignment_count(set_statement);
  }

  const MyliteAstExecuteStatement *execute_statement =
      mylite_ast_execute_statement_view(parser_ast, statement_index);
  if (execute_statement != NULL) {
    count += mylite_ast_execute_statement_view_using_count(execute_statement);
  }

  const MyliteAstCreateTable *create_table =
      mylite_ast_create_table_view(parser_ast, statement_index);
  if (create_table != NULL) {
    count += mylite_ast_create_table_view_column_count(create_table);
    count += mylite_ast_create_table_view_option_count(create_table);
    for (size_t i = 0; i < mylite_ast_create_table_view_key_count(create_table);
         i++) {
      const MyliteAstCreateTableKey *key =
          mylite_ast_create_table_view_key_at(create_table, i);
      count++;
      count += mylite_ast_create_table_key_view_column_count(key);
      count += mylite_ast_create_table_key_view_option_count(key);
    }
  }

  const MyliteAstAlterTable *alter_table =
      mylite_ast_alter_table_view(parser_ast, statement_index);
  if (alter_table != NULL) {
    count += mylite_ast_alter_table_view_spec_count(alter_table);
    count += mylite_ast_alter_table_view_option_count(alter_table);
    for (size_t i = 0; i < mylite_ast_alter_table_view_spec_count(alter_table);
         i++) {
      const MyliteAstAlterTableSpec *spec =
          mylite_ast_alter_table_view_spec_at(alter_table, i);
      count += mylite_ast_alter_table_spec_view_column_count(spec);
      for (size_t j = 0; j < mylite_ast_alter_table_spec_view_key_count(spec);
           j++) {
        const MyliteAstCreateTableKey *key =
            mylite_ast_alter_table_spec_view_key_at(spec, j);
        count++;
        count += mylite_ast_create_table_key_view_column_count(key);
        count += mylite_ast_create_table_key_view_option_count(key);
      }
    }
  }

  const MyliteAstCreateIndex *create_index =
      mylite_ast_create_index_view(parser_ast, statement_index);
  if (create_index != NULL) {
    count += mylite_ast_create_index_view_column_count(create_index);
    count += mylite_ast_create_index_view_option_count(create_index);
  }

  const MyliteAstCreateDatabase *create_database =
      mylite_ast_create_database_view(parser_ast, statement_index);
  if (create_database != NULL) {
    count += mylite_ast_create_database_view_option_count(create_database);
  }

  const MyliteAstCreateView *create_view =
      mylite_ast_create_view_view(parser_ast, statement_index);
  if (create_view != NULL) {
    count += mylite_ast_create_view_view_column_count(create_view);
  }

  const MyliteAstLockStatement *lock_statement =
      mylite_ast_lock_statement_view(parser_ast, statement_index);
  if (lock_statement != NULL) {
    count += mylite_ast_lock_statement_view_table_lock_count(lock_statement);
  }

  const MyliteAstTableMaintenanceStatement *maintenance_statement =
      mylite_ast_table_maintenance_statement_view(parser_ast, statement_index);
  if (maintenance_statement != NULL) {
    count += mylite_ast_table_maintenance_statement_view_target_count(
        maintenance_statement);
  }

  const MyliteAstReplicationStatement *replication_statement =
      mylite_ast_replication_statement_view(parser_ast, statement_index);
  if (replication_statement != NULL) {
    count +=
        mylite_ast_replication_statement_view_option_count(replication_statement);
  }

  if (mylite_ast_stored_object_statement_view(parser_ast, statement_index) !=
      NULL) {
    count++;
  }

  const MyliteAstFlushStatement *flush_statement =
      mylite_ast_flush_statement_view(parser_ast, statement_index);
  if (flush_statement != NULL) {
    count += mylite_ast_flush_statement_view_target_count(flush_statement);
    count += mylite_ast_flush_statement_view_plugin_count(flush_statement);
  }

  const MyliteAstLoadStatement *load_statement =
      mylite_ast_load_statement_view(parser_ast, statement_index);
  if (load_statement != NULL) {
    count += mylite_ast_load_statement_view_item_count(load_statement);
    count += mylite_ast_load_statement_view_assignment_count(load_statement);
    count += mylite_ast_load_statement_view_option_count(load_statement);
  }

  const MyliteAstAccountStatement *account_statement =
      mylite_ast_account_statement_view(parser_ast, statement_index);
  if (account_statement != NULL) {
    count += mylite_ast_account_statement_view_account_count(account_statement);
  }

  const MyliteAstPrivilegeStatement *privilege_statement =
      mylite_ast_privilege_statement_view(parser_ast, statement_index);
  if (privilege_statement != NULL) {
    count += mylite_ast_privilege_statement_view_item_count(privilege_statement);
    count += mylite_ast_privilege_statement_view_user_count(privilege_statement);
    if (mylite_ast_privilege_statement_view_proxy_user(privilege_statement) !=
        NULL) {
      count++;
    }
  }

  const MyliteAstRoleStatement *role_statement =
      mylite_ast_role_statement_view(parser_ast, statement_index);
  if (role_statement != NULL) {
    count += mylite_ast_role_statement_view_role_count(role_statement);
    count += mylite_ast_role_statement_view_user_count(role_statement);
  }

  return count;
}

static size_t mylite_semantic_ast_count_statement_clauses(
    const MyliteAst *parser_ast, size_t statement_index) {
  size_t count = 0;

  const MyliteAstUpdateStatement *update_statement =
      mylite_ast_update_statement_view(parser_ast, statement_index);
  if (update_statement != NULL) {
    count += mylite_semantic_ast_count_clause_span(
        mylite_ast_update_statement_view_table_reference_start(update_statement),
        mylite_ast_update_statement_view_table_reference_end(update_statement));
    count += mylite_semantic_ast_count_expression_root(
        mylite_ast_update_statement_view_where_expression(update_statement));
    count += mylite_semantic_ast_count_clause_span(
        mylite_ast_update_statement_view_order_by_start(update_statement),
        mylite_ast_update_statement_view_order_by_end(update_statement));
    count += mylite_semantic_ast_count_clause_span(
        mylite_ast_update_statement_view_limit_start(update_statement),
        mylite_ast_update_statement_view_limit_end(update_statement));
  }

  const MyliteAstDeleteStatement *delete_statement =
      mylite_ast_delete_statement_view(parser_ast, statement_index);
  if (delete_statement != NULL) {
    count += mylite_semantic_ast_count_clause_span(
        mylite_ast_delete_statement_view_table_reference_start(delete_statement),
        mylite_ast_delete_statement_view_table_reference_end(delete_statement));
    count += mylite_semantic_ast_count_expression_root(
        mylite_ast_delete_statement_view_where_expression(delete_statement));
    count += mylite_semantic_ast_count_clause_span(
        mylite_ast_delete_statement_view_order_by_start(delete_statement),
        mylite_ast_delete_statement_view_order_by_end(delete_statement));
    count += mylite_semantic_ast_count_clause_span(
        mylite_ast_delete_statement_view_limit_start(delete_statement),
        mylite_ast_delete_statement_view_limit_end(delete_statement));
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

  const MyliteAstShowStatement *show_statement =
      mylite_ast_show_statement_view(parser_ast, statement_index);
  if (show_statement != NULL) {
    count += mylite_semantic_ast_count_expression_root(
        mylite_ast_show_statement_view_like_expression(show_statement));
    count += mylite_semantic_ast_count_expression_root(
        mylite_ast_show_statement_view_where_expression(show_statement));
    count += mylite_semantic_ast_count_clause_span(
        mylite_ast_show_statement_view_limit_start(show_statement),
        mylite_ast_show_statement_view_limit_end(show_statement));
  }

  const MyliteAstKillStatement *kill_statement =
      mylite_ast_kill_statement_view(parser_ast, statement_index);
  if (kill_statement != NULL) {
    count += mylite_semantic_ast_count_expression_root(
        mylite_ast_kill_statement_view_target_expression(kill_statement));
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
  if (!mylite_semantic_ast_fill_statement_queries(
          ast, statement, parser_ast, statement_index, &child_index)) {
    return 0;
  }
  if (!mylite_semantic_ast_fill_statement_descriptors(
          ast, statement, parser_ast, statement_index, &child_index)) {
    return 0;
  }
  return mylite_semantic_ast_fill_statement_clauses(
             ast, statement, parser_ast, statement_index, &child_index) &&
         child_index == statement->child_count;
}

static int mylite_semantic_ast_fill_statement_queries(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *statement,
    const MyliteAst *parser_ast, size_t statement_index, size_t *child_index) {
  const MyliteAstValuesStatement *values_statement =
      mylite_ast_values_statement_view(parser_ast, statement_index);
  if (values_statement != NULL) {
    return mylite_semantic_ast_append_child(
        statement, child_index,
        mylite_semantic_ast_materialize_values_query(ast, values_statement));
  }

  const MyliteAstSelectStatement *select_statement =
      mylite_ast_select_statement_view(parser_ast, statement_index);
  if (select_statement != NULL) {
    return mylite_semantic_ast_append_child(
        statement, child_index,
        mylite_semantic_ast_materialize_select_query(ast, select_statement));
  }

  return 1;
}

static MyliteSemanticAstNode *mylite_semantic_ast_materialize_select_query(
    MyliteSemanticAst *ast, const MyliteAstSelectStatement *select_statement) {
  MyliteSemanticAstNode *query = mylite_semantic_ast_new_node(
      ast, MYLITE_SEMANTIC_NODE_QUERY,
      mylite_ast_select_statement_view_start(select_statement),
      mylite_ast_select_statement_view_end(select_statement));
  if (query == NULL) {
    return NULL;
  }
  query->query_block_count =
      mylite_ast_select_statement_view_query_block_count(select_statement);
  query->query_has_with_clause =
      mylite_ast_select_statement_view_has_with_clause(select_statement);
  query->query_has_set_operation =
      mylite_ast_select_statement_view_has_set_operation(select_statement);

  if (!mylite_semantic_ast_set_node_child_count(
          ast, query,
          mylite_semantic_ast_count_select_query_children(select_statement))) {
    return NULL;
  }
  if (!mylite_semantic_ast_fill_select_query_children(ast, query,
                                                      select_statement)) {
    return NULL;
  }
  return query;
}

static size_t mylite_semantic_ast_count_select_query_children(
    const MyliteAstSelectStatement *select_statement) {
  return mylite_ast_select_statement_view_projection_count(select_statement) +
         mylite_semantic_ast_count_select_query_clauses(select_statement);
}

static size_t mylite_semantic_ast_count_select_query_clauses(
    const MyliteAstSelectStatement *select_statement) {
  size_t count = 0;
  count += mylite_semantic_ast_count_clause_span(
      mylite_ast_select_statement_view_from_start(select_statement),
      mylite_ast_select_statement_view_from_end(select_statement));
  count += mylite_semantic_ast_count_expression_root(
      mylite_ast_select_statement_view_where_expression(select_statement));
  count += mylite_semantic_ast_count_clause_span(
      mylite_ast_select_statement_view_group_by_start(select_statement),
      mylite_ast_select_statement_view_group_by_end(select_statement));
  count += mylite_semantic_ast_count_expression_root(
      mylite_ast_select_statement_view_having_expression(select_statement));
  count += mylite_semantic_ast_count_clause_span(
      mylite_ast_select_statement_view_order_by_start(select_statement),
      mylite_ast_select_statement_view_order_by_end(select_statement));
  count += mylite_semantic_ast_count_clause_span(
      mylite_ast_select_statement_view_limit_start(select_statement),
      mylite_ast_select_statement_view_limit_end(select_statement));
  count += mylite_semantic_ast_count_clause_span(
      mylite_ast_select_statement_view_into_start(select_statement),
      mylite_ast_select_statement_view_into_end(select_statement));
  count += mylite_semantic_ast_count_clause_span(
      mylite_ast_select_statement_view_lock_start(select_statement),
      mylite_ast_select_statement_view_lock_end(select_statement));
  return count;
}

static int mylite_semantic_ast_fill_select_query_children(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *query,
    const MyliteAstSelectStatement *select_statement) {
  size_t child_index = 0;
  if (!mylite_semantic_ast_append_select_projection_descriptors(
          ast, query, &child_index, select_statement)) {
    return 0;
  }
  if (!mylite_semantic_ast_append_select_clauses(ast, query, &child_index,
                                                 select_statement)) {
    return 0;
  }
  return child_index == query->child_count;
}

static int mylite_semantic_ast_append_select_projection_descriptors(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *query,
    size_t *child_index, const MyliteAstSelectStatement *select_statement) {
  for (size_t i = 0;
       i < mylite_ast_select_statement_view_projection_count(select_statement);
       i++) {
    const MyliteAstSelectProjection *projection =
        mylite_ast_select_statement_view_projection_at(select_statement, i);
    const char *value = mylite_ast_select_projection_view_alias_value(
        projection);
    size_t value_length =
        mylite_ast_select_projection_view_alias_value_length(projection);
    if (value == NULL) {
      value = mylite_ast_select_projection_view_qualifier_value(projection);
      value_length =
          mylite_ast_select_projection_view_qualifier_value_length(projection);
    }
    if (!mylite_semantic_ast_append_descriptor_with_expression_child(
            ast, query, child_index, MYLITE_SEMANTIC_DESCRIPTOR_PROJECTION,
            mylite_ast_select_projection_view_start(projection),
            mylite_ast_select_projection_view_end(projection), value,
            value_length,
            mylite_ast_select_projection_view_expression(projection))) {
      return 0;
    }
  }
  return 1;
}

static int mylite_semantic_ast_append_select_clauses(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *query, size_t *child_index,
    const MyliteAstSelectStatement *select_statement) {
  if (!mylite_semantic_ast_append_select_from_clause_child(
          ast, query, child_index,
          mylite_ast_select_statement_view_from_start(select_statement),
          mylite_ast_select_statement_view_from_end(select_statement))) {
    return 0;
  }
  if (!mylite_semantic_ast_append_clause_child(
          ast, query, child_index, MYLITE_SEMANTIC_CLAUSE_WHERE,
          mylite_ast_select_statement_view_where_start(select_statement),
          mylite_ast_select_statement_view_where_end(select_statement),
          mylite_ast_select_statement_view_where_expression(select_statement))) {
    return 0;
  }
  if (!mylite_semantic_ast_append_clause_span_child(
          ast, query, child_index, MYLITE_SEMANTIC_CLAUSE_GROUP_BY,
          mylite_ast_select_statement_view_group_by_start(select_statement),
          mylite_ast_select_statement_view_group_by_end(select_statement))) {
    return 0;
  }
  if (!mylite_semantic_ast_append_clause_child(
          ast, query, child_index, MYLITE_SEMANTIC_CLAUSE_HAVING,
          mylite_ast_select_statement_view_having_start(select_statement),
          mylite_ast_select_statement_view_having_end(select_statement),
          mylite_ast_select_statement_view_having_expression(
              select_statement))) {
    return 0;
  }
  if (!mylite_semantic_ast_append_clause_span_child(
          ast, query, child_index, MYLITE_SEMANTIC_CLAUSE_ORDER_BY,
          mylite_ast_select_statement_view_order_by_start(select_statement),
          mylite_ast_select_statement_view_order_by_end(select_statement))) {
    return 0;
  }
  if (!mylite_semantic_ast_append_clause_span_child(
          ast, query, child_index, MYLITE_SEMANTIC_CLAUSE_LIMIT,
          mylite_ast_select_statement_view_limit_start(select_statement),
          mylite_ast_select_statement_view_limit_end(select_statement))) {
    return 0;
  }
  if (!mylite_semantic_ast_append_clause_span_child(
          ast, query, child_index, MYLITE_SEMANTIC_CLAUSE_INTO,
          mylite_ast_select_statement_view_into_start(select_statement),
          mylite_ast_select_statement_view_into_end(select_statement))) {
    return 0;
  }
  return mylite_semantic_ast_append_clause_span_child(
      ast, query, child_index, MYLITE_SEMANTIC_CLAUSE_LOCKING,
      mylite_ast_select_statement_view_lock_start(select_statement),
      mylite_ast_select_statement_view_lock_end(select_statement));
}

static int mylite_semantic_ast_append_select_from_clause_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *query, size_t *child_index,
    size_t start, size_t end) {
  if (mylite_semantic_ast_count_clause_span(start, end) == 0) {
    return 1;
  }

  MyliteSemanticAstNode *clause = mylite_semantic_ast_materialize_clause(
      ast, MYLITE_SEMANTIC_CLAUSE_FROM, start, end, NULL);
  if (clause == NULL ||
      !mylite_semantic_ast_set_node_child_count(ast, clause, 1)) {
    return 0;
  }

  size_t table_reference_index = 0;
  if (!mylite_semantic_ast_append_child(
          clause, &table_reference_index,
          mylite_semantic_ast_materialize_table_reference(ast, start, end)) ||
      table_reference_index != clause->child_count) {
    return 0;
  }

  return mylite_semantic_ast_append_child(query, child_index, clause);
}

static MyliteSemanticAstNode *mylite_semantic_ast_materialize_values_query(
    MyliteSemanticAst *ast, const MyliteAstValuesStatement *values_statement) {
  MyliteSemanticAstNode *query = mylite_semantic_ast_new_node(
      ast, MYLITE_SEMANTIC_NODE_QUERY,
      mylite_ast_values_statement_view_start(values_statement),
      mylite_ast_values_statement_view_end(values_statement));
  if (query == NULL) {
    return NULL;
  }
  if (!mylite_semantic_ast_set_node_child_count(
          ast, query,
          mylite_semantic_ast_count_values_query_children(values_statement))) {
    return NULL;
  }
  if (!mylite_semantic_ast_fill_values_query_children(ast, query,
                                                      values_statement)) {
    return NULL;
  }
  return query;
}

static size_t mylite_semantic_ast_count_values_query_children(
    const MyliteAstValuesStatement *values_statement) {
  return mylite_ast_values_statement_view_value_count(values_statement) +
         mylite_semantic_ast_count_values_query_clauses(values_statement);
}

static size_t mylite_semantic_ast_count_values_query_clauses(
    const MyliteAstValuesStatement *values_statement) {
  size_t count = 0;
  count += mylite_semantic_ast_count_clause_span(
      mylite_ast_values_statement_view_order_by_start(values_statement),
      mylite_ast_values_statement_view_order_by_end(values_statement));
  count += mylite_semantic_ast_count_clause_span(
      mylite_ast_values_statement_view_limit_start(values_statement),
      mylite_ast_values_statement_view_limit_end(values_statement));
  count += mylite_semantic_ast_count_clause_span(
      mylite_ast_values_statement_view_into_start(values_statement),
      mylite_ast_values_statement_view_into_end(values_statement));
  count += mylite_semantic_ast_count_clause_span(
      mylite_ast_values_statement_view_lock_start(values_statement),
      mylite_ast_values_statement_view_lock_end(values_statement));
  return count;
}

static int mylite_semantic_ast_fill_values_query_children(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *query,
    const MyliteAstValuesStatement *values_statement) {
  size_t child_index = 0;
  if (!mylite_semantic_ast_append_values_descriptors(
          ast, query, &child_index, values_statement)) {
    return 0;
  }
  if (!mylite_semantic_ast_append_values_clauses(ast, query, &child_index,
                                                 values_statement)) {
    return 0;
  }
  return child_index == query->child_count;
}

static int mylite_semantic_ast_append_values_descriptors(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *query,
    size_t *child_index, const MyliteAstValuesStatement *values_statement) {
  for (size_t i = 0;
       i < mylite_ast_values_statement_view_value_count(values_statement);
       i++) {
    const MyliteAstValuesValue *value =
        mylite_ast_values_statement_view_value_at(values_statement, i);
    if (!mylite_semantic_ast_append_descriptor_with_expression_child(
            ast, query, child_index, MYLITE_SEMANTIC_DESCRIPTOR_VALUE,
            mylite_ast_values_value_view_start(value),
            mylite_ast_values_value_view_end(value), NULL, 0,
            mylite_ast_values_value_view_expression(value))) {
      return 0;
    }
  }
  return 1;
}

static int mylite_semantic_ast_append_values_clauses(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *query, size_t *child_index,
    const MyliteAstValuesStatement *values_statement) {
  if (!mylite_semantic_ast_append_clause_span_child(
          ast, query, child_index, MYLITE_SEMANTIC_CLAUSE_ORDER_BY,
          mylite_ast_values_statement_view_order_by_start(values_statement),
          mylite_ast_values_statement_view_order_by_end(values_statement))) {
    return 0;
  }
  if (!mylite_semantic_ast_append_clause_span_child(
          ast, query, child_index, MYLITE_SEMANTIC_CLAUSE_LIMIT,
          mylite_ast_values_statement_view_limit_start(values_statement),
          mylite_ast_values_statement_view_limit_end(values_statement))) {
    return 0;
  }
  if (!mylite_semantic_ast_append_clause_span_child(
          ast, query, child_index, MYLITE_SEMANTIC_CLAUSE_INTO,
          mylite_ast_values_statement_view_into_start(values_statement),
          mylite_ast_values_statement_view_into_end(values_statement))) {
    return 0;
  }
  return mylite_semantic_ast_append_clause_span_child(
      ast, query, child_index, MYLITE_SEMANTIC_CLAUSE_LOCKING,
      mylite_ast_values_statement_view_lock_start(values_statement),
      mylite_ast_values_statement_view_lock_end(values_statement));
}

static MyliteSemanticAstNode *mylite_semantic_ast_materialize_table_reference(
    MyliteSemanticAst *ast, size_t start, size_t end) {
  return mylite_semantic_ast_new_node(
      ast, MYLITE_SEMANTIC_NODE_TABLE_REFERENCE, start, end);
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

static int mylite_semantic_ast_append_clause_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    MyliteSemanticClauseKind kind, size_t start, size_t end,
    const MyliteAstExpression *expression) {
  if (expression == NULL) {
    return 1;
  }
  return mylite_semantic_ast_append_child(
      parent, index,
      mylite_semantic_ast_materialize_clause(ast, kind, start, end,
                                             expression));
}

static int mylite_semantic_ast_append_clause_span_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    MyliteSemanticClauseKind kind, size_t start, size_t end) {
  if (mylite_semantic_ast_count_clause_span(start, end) == 0) {
    return 1;
  }
  return mylite_semantic_ast_append_child(
      parent, index,
      mylite_semantic_ast_materialize_clause(ast, kind, start, end, NULL));
}

static MyliteSemanticAstNode *mylite_semantic_ast_materialize_clause(
    MyliteSemanticAst *ast, MyliteSemanticClauseKind kind, size_t start,
    size_t end, const MyliteAstExpression *expression) {
  MyliteSemanticAstNode *clause =
      mylite_semantic_ast_new_node(ast, MYLITE_SEMANTIC_NODE_CLAUSE, start, end);
  if (clause == NULL) {
    return NULL;
  }
  clause->clause_kind = kind;
  if (!mylite_semantic_ast_set_node_child_count(
          ast, clause, mylite_semantic_ast_count_expression_root(expression))) {
    return NULL;
  }
  if (expression == NULL) {
    return clause;
  }
  size_t child_index = 0;
  if (!mylite_semantic_ast_append_expression_child(ast, clause, &child_index,
                                                   expression) ||
      child_index != clause->child_count) {
    return NULL;
  }
  return clause;
}

static int mylite_semantic_ast_fill_statement_descriptors(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *statement,
    const MyliteAst *parser_ast, size_t statement_index, size_t *child_index) {
  const MyliteAstInsertStatement *insert_statement =
      mylite_ast_insert_statement_view(parser_ast, statement_index);
  if (insert_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_insert_statement_view_column_count(insert_statement);
         i++) {
      const MyliteAstInsertColumn *column =
          mylite_ast_insert_statement_view_column_at(insert_statement, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_COLUMN,
              mylite_ast_insert_column_view_start(column),
              mylite_ast_insert_column_view_end(column),
              mylite_ast_insert_column_view_name_value(column),
              mylite_ast_insert_column_view_name_value_length(column))) {
        return 0;
      }
    }
    for (size_t i = 0;
         i < mylite_ast_insert_statement_view_value_count(insert_statement);
         i++) {
      const MyliteAstInsertValue *value =
          mylite_ast_insert_statement_view_value_at(insert_statement, i);
      if (!mylite_semantic_ast_append_descriptor_with_expression_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_VALUE,
              mylite_ast_insert_value_view_start(value),
              mylite_ast_insert_value_view_end(value), NULL, 0,
              mylite_ast_insert_value_view_expression(value))) {
        return 0;
      }
    }
    for (size_t i = 0; i < mylite_ast_insert_statement_view_set_assignment_count(
                               insert_statement);
         i++) {
      const MyliteAstInsertAssignment *assignment =
          mylite_ast_insert_statement_view_set_assignment_at(insert_statement,
                                                             i);
      if (!mylite_semantic_ast_append_descriptor_with_expression_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_ASSIGNMENT,
              mylite_ast_insert_assignment_view_start(assignment),
              mylite_ast_insert_assignment_view_end(assignment),
              mylite_ast_insert_assignment_view_name_value(assignment),
              mylite_ast_insert_assignment_view_name_value_length(assignment),
              mylite_ast_insert_assignment_view_value_expression(assignment))) {
        return 0;
      }
    }
    for (size_t i = 0;
         i < mylite_ast_insert_statement_view_duplicate_assignment_count(
                 insert_statement);
         i++) {
      const MyliteAstInsertAssignment *assignment =
          mylite_ast_insert_statement_view_duplicate_assignment_at(
              insert_statement, i);
      if (!mylite_semantic_ast_append_descriptor_with_expression_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_ASSIGNMENT,
              mylite_ast_insert_assignment_view_start(assignment),
              mylite_ast_insert_assignment_view_end(assignment),
              mylite_ast_insert_assignment_view_name_value(assignment),
              mylite_ast_insert_assignment_view_name_value_length(assignment),
              mylite_ast_insert_assignment_view_value_expression(assignment))) {
        return 0;
      }
    }
  }

  const MyliteAstReplaceStatement *replace_statement =
      mylite_ast_replace_statement_view(parser_ast, statement_index);
  if (replace_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_replace_statement_view_column_count(replace_statement);
         i++) {
      const MyliteAstReplaceColumn *column =
          mylite_ast_replace_statement_view_column_at(replace_statement, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_COLUMN,
              mylite_ast_replace_column_view_start(column),
              mylite_ast_replace_column_view_end(column),
              mylite_ast_replace_column_view_name_value(column),
              mylite_ast_replace_column_view_name_value_length(column))) {
        return 0;
      }
    }
    for (size_t i = 0;
         i < mylite_ast_replace_statement_view_value_count(replace_statement);
         i++) {
      const MyliteAstReplaceValue *value =
          mylite_ast_replace_statement_view_value_at(replace_statement, i);
      if (!mylite_semantic_ast_append_descriptor_with_expression_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_VALUE,
              mylite_ast_replace_value_view_start(value),
              mylite_ast_replace_value_view_end(value), NULL, 0,
              mylite_ast_replace_value_view_expression(value))) {
        return 0;
      }
    }
    for (size_t i = 0;
         i < mylite_ast_replace_statement_view_set_assignment_count(
                 replace_statement);
         i++) {
      const MyliteAstReplaceAssignment *assignment =
          mylite_ast_replace_statement_view_set_assignment_at(replace_statement,
                                                              i);
      if (!mylite_semantic_ast_append_descriptor_with_expression_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_ASSIGNMENT,
              mylite_ast_replace_assignment_view_start(assignment),
              mylite_ast_replace_assignment_view_end(assignment),
              mylite_ast_replace_assignment_view_name_value(assignment),
              mylite_ast_replace_assignment_view_name_value_length(
                  assignment),
              mylite_ast_replace_assignment_view_value_expression(assignment))) {
        return 0;
      }
    }
  }

  const MyliteAstUpdateStatement *update_statement =
      mylite_ast_update_statement_view(parser_ast, statement_index);
  if (update_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_update_statement_view_assignment_count(update_statement);
         i++) {
      const MyliteAstUpdateAssignment *assignment =
          mylite_ast_update_statement_view_assignment_at(update_statement, i);
      if (!mylite_semantic_ast_append_descriptor_with_expression_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_ASSIGNMENT,
              mylite_ast_update_assignment_view_start(assignment),
              mylite_ast_update_assignment_view_end(assignment),
              mylite_ast_update_assignment_view_name_value(assignment),
              mylite_ast_update_assignment_view_name_value_length(assignment),
              mylite_ast_update_assignment_view_value_expression(assignment))) {
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
      const MyliteAstSetAssignment *assignment =
          mylite_ast_set_statement_view_assignment_at(set_statement, i);
      if (!mylite_semantic_ast_append_descriptor_with_expression_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_ASSIGNMENT,
              mylite_ast_set_assignment_view_start(assignment),
              mylite_ast_set_assignment_view_end(assignment),
              mylite_ast_set_assignment_view_name_value(assignment),
              mylite_ast_set_assignment_view_name_value_length(assignment),
              mylite_ast_set_assignment_view_value_expression(assignment))) {
        return 0;
      }
    }
  }

  const MyliteAstExecuteStatement *execute_statement =
      mylite_ast_execute_statement_view(parser_ast, statement_index);
  if (execute_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_execute_statement_view_using_count(execute_statement);
         i++) {
      const MyliteAstPreparedStatementVariable *variable =
          mylite_ast_execute_statement_view_using_variable_at(execute_statement,
                                                             i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index,
              MYLITE_SEMANTIC_DESCRIPTOR_PREPARED_VARIABLE,
              mylite_ast_prepared_statement_variable_view_start(variable),
              mylite_ast_prepared_statement_variable_view_end(variable),
              mylite_ast_prepared_statement_variable_view_name_value(variable),
              mylite_ast_prepared_statement_variable_view_name_value_length(
                  variable))) {
        return 0;
      }
    }
  }

  const MyliteAstCreateTable *create_table =
      mylite_ast_create_table_view(parser_ast, statement_index);
  if (create_table != NULL) {
    for (size_t i = 0; i < mylite_ast_create_table_view_column_count(create_table);
         i++) {
      if (!mylite_semantic_ast_append_create_table_column_descriptor(
              ast, statement, child_index,
              mylite_ast_create_table_view_column_at(create_table, i))) {
        return 0;
      }
    }
    for (size_t i = 0; i < mylite_ast_create_table_view_key_count(create_table);
         i++) {
      if (!mylite_semantic_ast_append_create_table_key_descriptors(
              ast, statement, child_index,
              mylite_ast_create_table_view_key_at(create_table, i))) {
        return 0;
      }
    }
    for (size_t i = 0; i < mylite_ast_create_table_view_option_count(create_table);
         i++) {
      const MyliteAstCreateTableOption *option =
          mylite_ast_create_table_view_option_at(create_table, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_OPTION,
              mylite_ast_create_table_option_view_start(option),
              mylite_ast_create_table_option_view_end(option),
              mylite_ast_create_table_option_view_value(option),
              mylite_ast_create_table_option_view_value_length(option))) {
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
      const char *value = mylite_ast_alter_table_spec_view_name_value(spec);
      size_t value_length =
          mylite_ast_alter_table_spec_view_name_value_length(spec);
      if (value == NULL) {
        value = mylite_ast_alter_table_spec_view_secondary_name_value(spec);
        value_length =
            mylite_ast_alter_table_spec_view_secondary_name_value_length(spec);
      }
      if (value == NULL) {
        value = mylite_ast_alter_table_spec_view_table_name_value(spec);
        value_length =
            mylite_ast_alter_table_spec_view_table_name_value_length(spec);
      }
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index,
              MYLITE_SEMANTIC_DESCRIPTOR_ALTER_TABLE_SPEC,
              mylite_ast_alter_table_spec_view_start(spec),
              mylite_ast_alter_table_spec_view_end(spec), value,
              value_length)) {
        return 0;
      }
      for (size_t j = 0;
           j < mylite_ast_alter_table_spec_view_column_count(spec); j++) {
        if (!mylite_semantic_ast_append_create_table_column_descriptor(
                ast, statement, child_index,
                mylite_ast_alter_table_spec_view_column_at(spec, j))) {
          return 0;
        }
      }
      for (size_t j = 0; j < mylite_ast_alter_table_spec_view_key_count(spec);
           j++) {
        if (!mylite_semantic_ast_append_create_table_key_descriptors(
                ast, statement, child_index,
                mylite_ast_alter_table_spec_view_key_at(spec, j))) {
          return 0;
        }
      }
    }
    for (size_t i = 0; i < mylite_ast_alter_table_view_option_count(alter_table);
         i++) {
      const MyliteAstCreateTableOption *option =
          mylite_ast_alter_table_view_option_at(alter_table, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_OPTION,
              mylite_ast_create_table_option_view_start(option),
              mylite_ast_create_table_option_view_end(option),
              mylite_ast_create_table_option_view_value(option),
              mylite_ast_create_table_option_view_value_length(option))) {
        return 0;
      }
    }
  }

  const MyliteAstCreateIndex *create_index =
      mylite_ast_create_index_view(parser_ast, statement_index);
  if (create_index != NULL) {
    for (size_t i = 0; i < mylite_ast_create_index_view_column_count(create_index);
         i++) {
      const MyliteAstCreateTableKeyPart *part =
          mylite_ast_create_index_view_column_at(create_index, i);
      if (!mylite_semantic_ast_append_descriptor_with_expression_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_KEY_PART,
              mylite_ast_create_table_key_part_view_start(part),
              mylite_ast_create_table_key_part_view_end(part),
              mylite_ast_create_table_key_part_view_name_value(part),
              mylite_ast_create_table_key_part_view_name_value_length(part),
              mylite_ast_create_table_key_part_view_expression(part))) {
        return 0;
      }
    }
    for (size_t i = 0; i < mylite_ast_create_index_view_option_count(create_index);
         i++) {
      const MyliteAstCreateTableKeyOption *option =
          mylite_ast_create_index_view_option_at(create_index, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_OPTION,
              mylite_ast_create_table_key_option_view_start(option),
              mylite_ast_create_table_key_option_view_end(option),
              mylite_ast_create_table_key_option_view_value(option),
              mylite_ast_create_table_key_option_view_value_length(option))) {
        return 0;
      }
    }
  }

  const MyliteAstCreateDatabase *create_database =
      mylite_ast_create_database_view(parser_ast, statement_index);
  if (create_database != NULL) {
    for (size_t i = 0;
         i < mylite_ast_create_database_view_option_count(create_database); i++) {
      const MyliteAstDatabaseOption *option =
          mylite_ast_create_database_view_option_at(create_database, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index,
              MYLITE_SEMANTIC_DESCRIPTOR_DATABASE_OPTION,
              mylite_ast_database_option_view_start(option),
              mylite_ast_database_option_view_end(option),
              mylite_ast_database_option_view_value(option),
              mylite_ast_database_option_view_value_length(option))) {
        return 0;
      }
    }
  }

  const MyliteAstCreateView *create_view =
      mylite_ast_create_view_view(parser_ast, statement_index);
  if (create_view != NULL) {
    for (size_t i = 0; i < mylite_ast_create_view_view_column_count(create_view);
         i++) {
      const MyliteAstViewColumn *column =
          mylite_ast_create_view_view_column_at(create_view, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index,
              MYLITE_SEMANTIC_DESCRIPTOR_VIEW_COLUMN,
              mylite_ast_view_column_view_start(column),
              mylite_ast_view_column_view_end(column),
              mylite_ast_view_column_view_name_value(column),
              mylite_ast_view_column_view_name_value_length(column))) {
        return 0;
      }
    }
  }

  const MyliteAstLockStatement *lock_statement =
      mylite_ast_lock_statement_view(parser_ast, statement_index);
  if (lock_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_lock_statement_view_table_lock_count(lock_statement);
         i++) {
      const MyliteAstTableLock *table_lock =
          mylite_ast_lock_statement_view_table_lock_at(lock_statement, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_TABLE_LOCK,
              mylite_ast_table_lock_view_start(table_lock),
              mylite_ast_table_lock_view_end(table_lock),
              mylite_ast_table_lock_view_table_name_value(table_lock),
              mylite_ast_table_lock_view_table_name_value_length(table_lock))) {
        return 0;
      }
    }
  }

  const MyliteAstTableMaintenanceStatement *maintenance_statement =
      mylite_ast_table_maintenance_statement_view(parser_ast, statement_index);
  if (maintenance_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_table_maintenance_statement_view_target_count(
                 maintenance_statement);
         i++) {
      const MyliteAstTableMaintenanceTarget *target =
          mylite_ast_table_maintenance_statement_view_target_at(
              maintenance_statement, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index,
              MYLITE_SEMANTIC_DESCRIPTOR_TABLE_MAINTENANCE_TARGET,
              mylite_ast_table_maintenance_target_view_start(target),
              mylite_ast_table_maintenance_target_view_end(target),
              mylite_ast_table_maintenance_target_view_name_value(target),
              mylite_ast_table_maintenance_target_view_name_value_length(
                  target))) {
        return 0;
      }
    }
  }

  const MyliteAstReplicationStatement *replication_statement =
      mylite_ast_replication_statement_view(parser_ast, statement_index);
  if (replication_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_replication_statement_view_option_count(
                 replication_statement);
         i++) {
      const MyliteAstReplicationOption *option =
          mylite_ast_replication_statement_view_option_at(replication_statement,
                                                          i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index,
              MYLITE_SEMANTIC_DESCRIPTOR_REPLICATION_OPTION,
              mylite_ast_replication_option_view_start(option),
              mylite_ast_replication_option_view_end(option),
              mylite_ast_replication_option_view_name_value(option),
              mylite_ast_replication_option_view_name_value_length(option))) {
        return 0;
      }
    }
  }

  const MyliteAstStoredObjectStatement *stored_object =
      mylite_ast_stored_object_statement_view(parser_ast, statement_index);
  if (stored_object != NULL &&
      !mylite_semantic_ast_append_descriptor_child(
          ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_STORED_OBJECT,
          mylite_ast_stored_object_statement_view_start(stored_object),
          mylite_ast_stored_object_statement_view_end(stored_object),
          mylite_ast_stored_object_statement_view_name_value(stored_object),
          mylite_ast_stored_object_statement_view_name_value_length(
              stored_object))) {
    return 0;
  }

  const MyliteAstFlushStatement *flush_statement =
      mylite_ast_flush_statement_view(parser_ast, statement_index);
  if (flush_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_flush_statement_view_target_count(flush_statement);
         i++) {
      const MyliteAstFlushTarget *target =
          mylite_ast_flush_statement_view_target_at(flush_statement, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_FLUSH_TARGET,
              mylite_ast_flush_target_view_start(target),
              mylite_ast_flush_target_view_end(target),
              mylite_ast_flush_target_view_name_value(target),
              mylite_ast_flush_target_view_name_value_length(target))) {
        return 0;
      }
    }
    for (size_t i = 0;
         i < mylite_ast_flush_statement_view_plugin_count(flush_statement);
         i++) {
      const MyliteAstFlushPlugin *plugin =
          mylite_ast_flush_statement_view_plugin_at(flush_statement, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_FLUSH_PLUGIN,
              mylite_ast_flush_plugin_view_start(plugin),
              mylite_ast_flush_plugin_view_end(plugin),
              mylite_ast_flush_plugin_view_name_value(plugin),
              mylite_ast_flush_plugin_view_name_value_length(plugin))) {
        return 0;
      }
    }
  }

  const MyliteAstLoadStatement *load_statement =
      mylite_ast_load_statement_view(parser_ast, statement_index);
  if (load_statement != NULL) {
    for (size_t i = 0; i < mylite_ast_load_statement_view_item_count(load_statement);
         i++) {
      const MyliteAstLoadListItem *item =
          mylite_ast_load_statement_view_item_at(load_statement, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_LOAD_ITEM,
              mylite_ast_load_list_item_view_start(item),
              mylite_ast_load_list_item_view_end(item),
              mylite_ast_load_list_item_view_value(item),
              mylite_ast_load_list_item_view_value_length(item))) {
        return 0;
      }
    }
    for (size_t i = 0;
         i < mylite_ast_load_statement_view_assignment_count(load_statement);
         i++) {
      const MyliteAstLoadAssignment *assignment =
          mylite_ast_load_statement_view_assignment_at(load_statement, i);
      if (!mylite_semantic_ast_append_descriptor_with_expression_child(
              ast, statement, child_index,
              MYLITE_SEMANTIC_DESCRIPTOR_LOAD_ASSIGNMENT,
              mylite_ast_load_assignment_view_start(assignment),
              mylite_ast_load_assignment_view_end(assignment),
              mylite_ast_load_assignment_view_column_value(assignment),
              mylite_ast_load_assignment_view_column_value_length(
                  assignment),
              mylite_ast_load_assignment_view_expression(assignment))) {
        return 0;
      }
    }
    for (size_t i = 0; i < mylite_ast_load_statement_view_option_count(load_statement);
         i++) {
      const MyliteAstLoadOption *option =
          mylite_ast_load_statement_view_option_at(load_statement, i);
      if (!mylite_semantic_ast_append_descriptor_with_expression_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_LOAD_OPTION,
              mylite_ast_load_option_view_start(option),
              mylite_ast_load_option_view_end(option),
              mylite_ast_load_option_view_name_value(option),
              mylite_ast_load_option_view_name_value_length(option),
              mylite_ast_load_option_view_value_expression(option))) {
        return 0;
      }
    }
  }

  const MyliteAstAccountStatement *account_statement =
      mylite_ast_account_statement_view(parser_ast, statement_index);
  if (account_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_account_statement_view_account_count(account_statement);
         i++) {
      const MyliteAstAccount *account =
          mylite_ast_account_statement_view_account_at(account_statement, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_ACCOUNT,
              mylite_ast_account_view_start(account),
              mylite_ast_account_view_end(account),
              mylite_ast_account_view_user_value(account),
              mylite_ast_account_view_user_value_length(account))) {
        return 0;
      }
    }
  }

  const MyliteAstPrivilegeStatement *privilege_statement =
      mylite_ast_privilege_statement_view(parser_ast, statement_index);
  if (privilege_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_privilege_statement_view_item_count(privilege_statement);
         i++) {
      const MyliteAstPrivilegeItem *item =
          mylite_ast_privilege_statement_view_item_at(privilege_statement, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index,
              MYLITE_SEMANTIC_DESCRIPTOR_PRIVILEGE_ITEM,
              mylite_ast_privilege_item_view_start(item),
              mylite_ast_privilege_item_view_end(item),
              mylite_ast_privilege_item_view_value(item),
              mylite_ast_privilege_item_view_value_length(item))) {
        return 0;
      }
    }
    for (size_t i = 0;
         i < mylite_ast_privilege_statement_view_user_count(privilege_statement);
         i++) {
      const MyliteAstAccount *account =
          mylite_ast_privilege_statement_view_user_at(privilege_statement, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_ACCOUNT,
              mylite_ast_account_view_start(account),
              mylite_ast_account_view_end(account),
              mylite_ast_account_view_user_value(account),
              mylite_ast_account_view_user_value_length(account))) {
        return 0;
      }
    }
    const MyliteAstAccount *proxy_user =
        mylite_ast_privilege_statement_view_proxy_user(privilege_statement);
    if (proxy_user != NULL &&
        !mylite_semantic_ast_append_descriptor_child(
            ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_ACCOUNT,
            mylite_ast_account_view_start(proxy_user),
            mylite_ast_account_view_end(proxy_user),
            mylite_ast_account_view_user_value(proxy_user),
            mylite_ast_account_view_user_value_length(proxy_user))) {
      return 0;
    }
  }

  const MyliteAstRoleStatement *role_statement =
      mylite_ast_role_statement_view(parser_ast, statement_index);
  if (role_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_role_statement_view_role_count(role_statement); i++) {
      const MyliteAstRoleName *role =
          mylite_ast_role_statement_view_role_at(role_statement, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_ROLE,
              mylite_ast_role_name_view_start(role),
              mylite_ast_role_name_view_end(role),
              mylite_ast_role_name_view_name_value(role),
              mylite_ast_role_name_view_name_value_length(role))) {
        return 0;
      }
    }
    for (size_t i = 0;
         i < mylite_ast_role_statement_view_user_count(role_statement); i++) {
      const MyliteAstAccount *account =
          mylite_ast_role_statement_view_user_at(role_statement, i);
      if (!mylite_semantic_ast_append_descriptor_child(
              ast, statement, child_index, MYLITE_SEMANTIC_DESCRIPTOR_ACCOUNT,
              mylite_ast_account_view_start(account),
              mylite_ast_account_view_end(account),
              mylite_ast_account_view_user_value(account),
              mylite_ast_account_view_user_value_length(account))) {
        return 0;
      }
    }
  }

  return 1;
}

static int mylite_semantic_ast_append_create_table_column_descriptor(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    const MyliteAstCreateTableColumn *column) {
  if (column == NULL) {
    return 1;
  }

  MyliteSemanticAstNode *descriptor = mylite_semantic_ast_materialize_descriptor(
      ast, MYLITE_SEMANTIC_DESCRIPTOR_COLUMN,
      mylite_ast_create_table_column_view_start(column),
      mylite_ast_create_table_column_view_end(column),
      mylite_ast_create_table_column_view_name_value(column),
      mylite_ast_create_table_column_view_name_value_length(column),
      mylite_semantic_ast_count_create_table_column_data_type(column) +
          mylite_semantic_ast_count_create_table_column_expressions(column));
  if (descriptor == NULL) {
    return 0;
  }

  size_t child_index = 0;
  if (!mylite_semantic_ast_append_create_table_column_type_child(
          ast, descriptor, &child_index, column) ||
      !mylite_semantic_ast_append_descriptor_expression_child(
          ast, descriptor, &child_index,
          mylite_ast_create_table_column_view_default_value_expression(column)) ||
      !mylite_semantic_ast_append_descriptor_expression_child(
          ast, descriptor, &child_index,
          mylite_ast_create_table_column_view_on_update_value_expression(column)) ||
      !mylite_semantic_ast_append_descriptor_expression_child(
          ast, descriptor, &child_index,
          mylite_ast_create_table_column_view_generated_expression(column)) ||
      !mylite_semantic_ast_append_descriptor_expression_child(
          ast, descriptor, &child_index,
          mylite_ast_create_table_column_view_check_expression(column))) {
    return 0;
  }

  return child_index == descriptor->child_count &&
         mylite_semantic_ast_append_child(parent, index, descriptor);
}

static int mylite_semantic_ast_append_create_table_column_type_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *descriptor, size_t *index,
    const MyliteAstCreateTableColumn *column) {
  if (mylite_semantic_ast_count_create_table_column_data_type(column) == 0) {
    return 1;
  }
  return mylite_semantic_ast_append_child(
      descriptor, index, mylite_semantic_ast_materialize_data_type(ast, column));
}

static MyliteSemanticAstNode *mylite_semantic_ast_materialize_data_type(
    MyliteSemanticAst *ast, const MyliteAstCreateTableColumn *column) {
  MyliteSemanticAstNode *data_type = mylite_semantic_ast_new_node(
      ast, MYLITE_SEMANTIC_NODE_DATA_TYPE,
      mylite_ast_create_table_column_view_type_start(column),
      mylite_ast_create_table_column_view_type_end(column));
  if (data_type == NULL) {
    return NULL;
  }
  data_type->data_type_family =
      mylite_ast_create_table_column_view_type_family(column);
  data_type->data_type_kind =
      mylite_ast_create_table_column_view_type_kind(column);
  data_type->data_type_storage_class =
      mylite_ast_create_table_column_view_storage_class(column);
  data_type->data_type_flags =
      mylite_ast_create_table_column_view_flags(column);
  data_type->data_type_numeric_parameter_count =
      mylite_ast_create_table_column_view_type_numeric_parameter_count(column);
  for (size_t i = 0;
       i < data_type->data_type_numeric_parameter_count &&
       i < sizeof(data_type->data_type_numeric_parameters) /
               sizeof(data_type->data_type_numeric_parameters[0]);
       i++) {
    data_type->data_type_numeric_parameters[i] =
        mylite_ast_create_table_column_view_type_numeric_parameter_at(column,
                                                                      i);
  }
  if (!mylite_semantic_ast_copy_source_span_value(
          ast, data_type,
          mylite_ast_create_table_column_view_type_name_start(column),
          mylite_ast_create_table_column_view_type_name_end(column))) {
    return NULL;
  }
  if (!mylite_semantic_ast_set_node_child_count(
          ast, data_type,
          mylite_ast_create_table_column_view_type_element_count(column) +
              mylite_semantic_ast_count_create_table_column_data_type_attributes(
                  column)) ||
      !mylite_semantic_ast_fill_data_type_children(ast, data_type, column)) {
    return NULL;
  }
  return data_type;
}

static int mylite_semantic_ast_fill_data_type_children(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *data_type,
    const MyliteAstCreateTableColumn *column) {
  size_t child_index = 0;
  for (size_t i = 0;
       i < mylite_ast_create_table_column_view_type_element_count(column); i++) {
    if (!mylite_semantic_ast_append_child(
            data_type, &child_index,
            mylite_semantic_ast_materialize_data_type_element(
                ast,
                mylite_ast_create_table_column_view_type_element_at(column,
                                                                    i)))) {
      return 0;
    }
  }
  if (!mylite_semantic_ast_append_data_type_attribute_child(
          ast, data_type, &child_index,
          MYLITE_SEMANTIC_DATA_TYPE_ATTRIBUTE_UNSIGNED,
          mylite_ast_create_table_column_view_type_unsigned_start(column),
          mylite_ast_create_table_column_view_type_unsigned_end(column), NULL,
          0) ||
      !mylite_semantic_ast_append_data_type_attribute_child(
          ast, data_type, &child_index,
          MYLITE_SEMANTIC_DATA_TYPE_ATTRIBUTE_ZEROFILL,
          mylite_ast_create_table_column_view_type_zerofill_start(column),
          mylite_ast_create_table_column_view_type_zerofill_end(column), NULL,
          0) ||
      !mylite_semantic_ast_append_data_type_attribute_child(
          ast, data_type, &child_index,
          MYLITE_SEMANTIC_DATA_TYPE_ATTRIBUTE_BINARY,
          mylite_ast_create_table_column_view_type_binary_start(column),
          mylite_ast_create_table_column_view_type_binary_end(column), NULL,
          0) ||
      !mylite_semantic_ast_append_data_type_attribute_child(
          ast, data_type, &child_index,
          MYLITE_SEMANTIC_DATA_TYPE_ATTRIBUTE_CHARSET,
          mylite_ast_create_table_column_view_type_charset_start(column),
          mylite_ast_create_table_column_view_type_charset_end(column),
          mylite_ast_create_table_column_view_type_charset_value(column),
          mylite_ast_create_table_column_view_type_charset_value_length(
              column)) ||
      !mylite_semantic_ast_append_data_type_attribute_child(
          ast, data_type, &child_index,
          MYLITE_SEMANTIC_DATA_TYPE_ATTRIBUTE_COLLATION,
          mylite_ast_create_table_column_view_type_collation_start(column),
          mylite_ast_create_table_column_view_type_collation_end(column),
          mylite_ast_create_table_column_view_type_collation_value(column),
          mylite_ast_create_table_column_view_type_collation_value_length(
              column))) {
    return 0;
  }
  return child_index == data_type->child_count;
}

static MyliteSemanticAstNode *mylite_semantic_ast_materialize_data_type_element(
    MyliteSemanticAst *ast,
    const MyliteAstCreateTableColumnTypeElement *element) {
  MyliteSemanticAstNode *node = mylite_semantic_ast_new_node(
      ast, MYLITE_SEMANTIC_NODE_DATA_TYPE_ELEMENT,
      mylite_ast_create_table_column_type_element_view_start(element),
      mylite_ast_create_table_column_type_element_view_end(element));
  if (node == NULL) {
    return NULL;
  }
  if (!mylite_semantic_ast_copy_node_value(
          ast, node,
          mylite_ast_create_table_column_type_element_view_value(element),
          mylite_ast_create_table_column_type_element_view_value_length(
              element))) {
    return NULL;
  }
  return node;
}

static int mylite_semantic_ast_append_data_type_attribute_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *data_type, size_t *index,
    MyliteSemanticDataTypeAttributeKind kind, size_t start, size_t end,
    const char *value, size_t value_length) {
  if (mylite_semantic_ast_count_clause_span(start, end) == 0) {
    return 1;
  }
  return mylite_semantic_ast_append_child(
      data_type, index,
      mylite_semantic_ast_materialize_data_type_attribute(
          ast, kind, start, end, value, value_length));
}

static MyliteSemanticAstNode *
mylite_semantic_ast_materialize_data_type_attribute(
    MyliteSemanticAst *ast, MyliteSemanticDataTypeAttributeKind kind,
    size_t start, size_t end, const char *value, size_t value_length) {
  MyliteSemanticAstNode *node = mylite_semantic_ast_new_node(
      ast, MYLITE_SEMANTIC_NODE_DATA_TYPE_ATTRIBUTE, start, end);
  if (node == NULL) {
    return NULL;
  }
  node->data_type_attribute_kind = kind;
  if (!mylite_semantic_ast_copy_node_value(ast, node, value, value_length)) {
    return NULL;
  }
  return node;
}

static int mylite_semantic_ast_append_create_table_key_descriptors(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    const MyliteAstCreateTableKey *key) {
  if (key == NULL) {
    return 1;
  }

  const char *value = mylite_ast_create_table_key_view_name_value(key);
  size_t value_length = mylite_ast_create_table_key_view_name_value_length(key);
  if (value == NULL) {
    value = mylite_ast_create_table_key_view_constraint_name_value(key);
    value_length =
        mylite_ast_create_table_key_view_constraint_name_value_length(key);
  }
  if (!mylite_semantic_ast_append_descriptor_with_expression_child(
          ast, parent, index, MYLITE_SEMANTIC_DESCRIPTOR_KEY,
          mylite_ast_create_table_key_view_start(key),
          mylite_ast_create_table_key_view_end(key), value, value_length,
          mylite_ast_create_table_key_view_check_expression(key))) {
    return 0;
  }

  for (size_t i = 0; i < mylite_ast_create_table_key_view_column_count(key);
       i++) {
    const MyliteAstCreateTableKeyPart *part =
        mylite_ast_create_table_key_view_column_at(key, i);
    if (!mylite_semantic_ast_append_descriptor_with_expression_child(
            ast, parent, index, MYLITE_SEMANTIC_DESCRIPTOR_KEY_PART,
            mylite_ast_create_table_key_part_view_start(part),
            mylite_ast_create_table_key_part_view_end(part),
            mylite_ast_create_table_key_part_view_name_value(part),
            mylite_ast_create_table_key_part_view_name_value_length(part),
            mylite_ast_create_table_key_part_view_expression(part))) {
      return 0;
    }
  }

  for (size_t i = 0; i < mylite_ast_create_table_key_view_option_count(key);
       i++) {
    const MyliteAstCreateTableKeyOption *option =
        mylite_ast_create_table_key_view_option_at(key, i);
    if (!mylite_semantic_ast_append_descriptor_child(
            ast, parent, index, MYLITE_SEMANTIC_DESCRIPTOR_OPTION,
            mylite_ast_create_table_key_option_view_start(option),
            mylite_ast_create_table_key_option_view_end(option),
            mylite_ast_create_table_key_option_view_value(option),
            mylite_ast_create_table_key_option_view_value_length(option))) {
      return 0;
    }
  }

  return 1;
}

static int mylite_semantic_ast_append_descriptor_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    MyliteSemanticDescriptorKind kind, size_t start, size_t end,
    const char *value, size_t value_length) {
  return mylite_semantic_ast_append_child(
      parent, index,
      mylite_semantic_ast_materialize_descriptor(ast, kind, start, end, value,
                                                 value_length, 0));
}

static int mylite_semantic_ast_append_descriptor_expression_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *descriptor, size_t *index,
    const MyliteAstExpression *expression) {
  if (expression == NULL) {
    return 1;
  }
  return mylite_semantic_ast_append_child(
      descriptor, index,
      mylite_semantic_ast_materialize_expression(ast, expression));
}

static int mylite_semantic_ast_append_descriptor_with_expression_child(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *parent, size_t *index,
    MyliteSemanticDescriptorKind kind, size_t start, size_t end,
    const char *value, size_t value_length,
    const MyliteAstExpression *expression) {
  MyliteSemanticAstNode *descriptor = mylite_semantic_ast_materialize_descriptor(
      ast, kind, start, end, value, value_length,
      mylite_semantic_ast_count_expression_root(expression));
  if (descriptor == NULL) {
    return 0;
  }

  size_t child_index = 0;
  if (!mylite_semantic_ast_append_descriptor_expression_child(
          ast, descriptor, &child_index, expression) ||
      child_index != descriptor->child_count) {
    return 0;
  }

  return mylite_semantic_ast_append_child(parent, index, descriptor);
}

static MyliteSemanticAstNode *mylite_semantic_ast_materialize_descriptor(
    MyliteSemanticAst *ast, MyliteSemanticDescriptorKind kind, size_t start,
    size_t end, const char *value, size_t value_length, size_t child_count) {
  MyliteSemanticAstNode *descriptor = mylite_semantic_ast_new_node(
      ast, MYLITE_SEMANTIC_NODE_DESCRIPTOR, start, end);
  if (descriptor == NULL) {
    return NULL;
  }
  descriptor->descriptor_kind = kind;
  if (!mylite_semantic_ast_copy_node_value(ast, descriptor, value,
                                           value_length)) {
    return NULL;
  }
  if (!mylite_semantic_ast_set_node_child_count(ast, descriptor,
                                                child_count)) {
    return NULL;
  }
  return descriptor;
}

static int mylite_semantic_ast_fill_statement_clauses(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *statement,
    const MyliteAst *parser_ast, size_t statement_index, size_t *child_index) {
  const MyliteAstUpdateStatement *update_statement =
      mylite_ast_update_statement_view(parser_ast, statement_index);
  if (update_statement != NULL) {
    if (!mylite_semantic_ast_append_clause_span_child(
            ast, statement, child_index,
            MYLITE_SEMANTIC_CLAUSE_TABLE_REFERENCE,
            mylite_ast_update_statement_view_table_reference_start(
                update_statement),
            mylite_ast_update_statement_view_table_reference_end(
                update_statement)) ||
        !mylite_semantic_ast_append_clause_child(
            ast, statement, child_index, MYLITE_SEMANTIC_CLAUSE_WHERE,
            mylite_ast_update_statement_view_where_start(update_statement),
            mylite_ast_update_statement_view_where_end(update_statement),
            mylite_ast_update_statement_view_where_expression(update_statement)) ||
        !mylite_semantic_ast_append_clause_span_child(
            ast, statement, child_index, MYLITE_SEMANTIC_CLAUSE_ORDER_BY,
            mylite_ast_update_statement_view_order_by_start(update_statement),
            mylite_ast_update_statement_view_order_by_end(update_statement)) ||
        !mylite_semantic_ast_append_clause_span_child(
            ast, statement, child_index, MYLITE_SEMANTIC_CLAUSE_LIMIT,
            mylite_ast_update_statement_view_limit_start(update_statement),
            mylite_ast_update_statement_view_limit_end(update_statement))) {
      return 0;
    }
  }

  const MyliteAstDeleteStatement *delete_statement =
      mylite_ast_delete_statement_view(parser_ast, statement_index);
  if (delete_statement != NULL) {
    if (!mylite_semantic_ast_append_clause_span_child(
            ast, statement, child_index,
            MYLITE_SEMANTIC_CLAUSE_TABLE_REFERENCE,
            mylite_ast_delete_statement_view_table_reference_start(
                delete_statement),
            mylite_ast_delete_statement_view_table_reference_end(
                delete_statement)) ||
        !mylite_semantic_ast_append_clause_child(
            ast, statement, child_index, MYLITE_SEMANTIC_CLAUSE_WHERE,
            mylite_ast_delete_statement_view_where_start(delete_statement),
            mylite_ast_delete_statement_view_where_end(delete_statement),
            mylite_ast_delete_statement_view_where_expression(delete_statement)) ||
        !mylite_semantic_ast_append_clause_span_child(
            ast, statement, child_index, MYLITE_SEMANTIC_CLAUSE_ORDER_BY,
            mylite_ast_delete_statement_view_order_by_start(delete_statement),
            mylite_ast_delete_statement_view_order_by_end(delete_statement)) ||
        !mylite_semantic_ast_append_clause_span_child(
            ast, statement, child_index, MYLITE_SEMANTIC_CLAUSE_LIMIT,
            mylite_ast_delete_statement_view_limit_start(delete_statement),
            mylite_ast_delete_statement_view_limit_end(delete_statement))) {
      return 0;
    }
  }

  const MyliteAstCallStatement *call_statement =
      mylite_ast_call_statement_view(parser_ast, statement_index);
  if (call_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_call_statement_view_argument_count(call_statement);
         i++) {
      const MyliteAstCallArgument *argument =
          mylite_ast_call_statement_view_argument_at(call_statement, i);
      if (!mylite_semantic_ast_append_clause_child(
              ast, statement, child_index,
              MYLITE_SEMANTIC_CLAUSE_CALL_ARGUMENT,
              mylite_ast_call_argument_view_start(argument),
              mylite_ast_call_argument_view_end(argument),
              mylite_ast_call_argument_view_expression(
                  argument))) {
        return 0;
      }
    }
  }

  const MyliteAstDoStatement *do_statement =
      mylite_ast_do_statement_view(parser_ast, statement_index);
  if (do_statement != NULL) {
    for (size_t i = 0;
         i < mylite_ast_do_statement_view_expression_count(do_statement); i++) {
      const MyliteAstDoExpression *expression =
          mylite_ast_do_statement_view_expression_at(do_statement, i);
      if (!mylite_semantic_ast_append_clause_child(
              ast, statement, child_index,
              MYLITE_SEMANTIC_CLAUSE_DO_EXPRESSION,
              mylite_ast_do_expression_view_start(expression),
              mylite_ast_do_expression_view_end(expression),
              mylite_ast_do_expression_view_expression(
                  expression))) {
        return 0;
      }
    }
  }

  const MyliteAstShowStatement *show_statement =
      mylite_ast_show_statement_view(parser_ast, statement_index);
  if (show_statement != NULL) {
    if (!mylite_semantic_ast_append_clause_child(
            ast, statement, child_index, MYLITE_SEMANTIC_CLAUSE_SHOW_LIKE,
            mylite_ast_show_statement_view_like_start(show_statement),
            mylite_ast_show_statement_view_like_end(show_statement),
            mylite_ast_show_statement_view_like_expression(show_statement)) ||
        !mylite_semantic_ast_append_clause_child(
            ast, statement, child_index, MYLITE_SEMANTIC_CLAUSE_SHOW_WHERE,
            mylite_ast_show_statement_view_where_start(show_statement),
            mylite_ast_show_statement_view_where_end(show_statement),
            mylite_ast_show_statement_view_where_expression(show_statement)) ||
        !mylite_semantic_ast_append_clause_span_child(
            ast, statement, child_index, MYLITE_SEMANTIC_CLAUSE_LIMIT,
            mylite_ast_show_statement_view_limit_start(show_statement),
            mylite_ast_show_statement_view_limit_end(show_statement))) {
      return 0;
    }
  }

  const MyliteAstKillStatement *kill_statement =
      mylite_ast_kill_statement_view(parser_ast, statement_index);
  if (kill_statement != NULL &&
      !mylite_semantic_ast_append_clause_child(
          ast, statement, child_index, MYLITE_SEMANTIC_CLAUSE_KILL_TARGET,
          mylite_ast_kill_statement_view_target_start(kill_statement),
          mylite_ast_kill_statement_view_target_end(kill_statement),
          mylite_ast_kill_statement_view_target_expression(kill_statement))) {
    return 0;
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

static int mylite_semantic_ast_copy_source_span_value(
    MyliteSemanticAst *ast, MyliteSemanticAstNode *node, size_t start,
    size_t end) {
  if (ast == NULL || node == NULL) {
    return 0;
  }
  if (start >= end) {
    return 1;
  }
  if (end > ast->source_length) {
    return 0;
  }
  return mylite_semantic_ast_copy_node_value(ast, node, ast->source + start,
                                             end - start);
}

static size_t mylite_semantic_ast_count_expression_root(
    const MyliteAstExpression *expression) {
  return expression == NULL ? 0 : 1;
}

static size_t mylite_semantic_ast_count_clause_span(size_t start, size_t end) {
  return start < end ? 1 : 0;
}

static size_t mylite_semantic_ast_count_create_table_column_data_type(
    const MyliteAstCreateTableColumn *column) {
  if (column == NULL) {
    return 0;
  }
  return mylite_semantic_ast_count_clause_span(
      mylite_ast_create_table_column_view_type_start(column),
      mylite_ast_create_table_column_view_type_end(column));
}

static size_t mylite_semantic_ast_count_create_table_column_data_type_attributes(
    const MyliteAstCreateTableColumn *column) {
  if (column == NULL) {
    return 0;
  }
  return mylite_semantic_ast_count_clause_span(
             mylite_ast_create_table_column_view_type_unsigned_start(column),
             mylite_ast_create_table_column_view_type_unsigned_end(column)) +
         mylite_semantic_ast_count_clause_span(
             mylite_ast_create_table_column_view_type_zerofill_start(column),
             mylite_ast_create_table_column_view_type_zerofill_end(column)) +
         mylite_semantic_ast_count_clause_span(
             mylite_ast_create_table_column_view_type_binary_start(column),
             mylite_ast_create_table_column_view_type_binary_end(column)) +
         mylite_semantic_ast_count_clause_span(
             mylite_ast_create_table_column_view_type_charset_start(column),
             mylite_ast_create_table_column_view_type_charset_end(column)) +
         mylite_semantic_ast_count_clause_span(
             mylite_ast_create_table_column_view_type_collation_start(column),
             mylite_ast_create_table_column_view_type_collation_end(column));
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

static void mylite_semantic_ast_set_no_memory(MyliteParseResult *result,
                                              const char *message) {
  if (result == NULL) {
    return;
  }
  memset(result, 0, sizeof(*result));
  result->status = MYLITE_PARSE_NO_MEMORY;
  snprintf(result->message, sizeof(result->message), "%s", message);
}
