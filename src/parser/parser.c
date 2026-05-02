#include "mylite/parser.h"

#include "mylite/parser_internal.h"
#include "lexer.h"
#include "mylite_tidb_parser.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct MyliteAstChunk {
  struct MyliteAstChunk *next;
  size_t capacity;
  size_t used;
  unsigned char data[];
} MyliteAstChunk;

typedef struct MyliteAstStatementTarget {
  MyliteStatementTargetKind kind;
  MyliteStatementTargetRole role;
  size_t start;
  size_t end;
  size_t schema_start;
  size_t schema_end;
  size_t name_start;
  size_t name_end;
} MyliteAstStatementTarget;

typedef enum MyliteAstCreateTableColumnTypeShapeFlag {
  MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_LENGTH = 1u << 0,
  MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_PRECISION = 1u << 1,
  MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_SCALE = 1u << 2,
  MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_FSP = 1u << 3
} MyliteAstCreateTableColumnTypeShapeFlag;

typedef struct MyliteAstCreateTableTypeElement {
  const char *value;
  size_t value_length;
  size_t start;
  size_t end;
  int token;
} MyliteAstCreateTableTypeElement;

typedef struct MyliteAstCreateTableColumn {
  unsigned long long type_length;
  unsigned long long type_precision;
  unsigned long long type_scale;
  unsigned long long type_fractional_seconds_precision;
  MyliteCreateTableColumnTypeFamily type_family;
  MyliteCreateTableColumnTypeKind type_kind;
  MyliteCreateTableColumnStorageClass storage_class;
  unsigned int type_shape_flags;
  unsigned int flags;
  size_t start;
  size_t end;
  size_t name_start;
  size_t name_end;
  const char *name_value;
  size_t name_value_length;
  size_t type_start;
  size_t type_end;
  const MyliteAstNode *type_node;
  size_t type_name_start;
  size_t type_name_end;
  size_t type_parameters_start;
  size_t type_parameters_end;
  size_t type_numeric_parameter_count;
  unsigned long long type_numeric_parameters[2];
  MyliteAstCreateTableTypeElement *type_elements;
  size_t type_element_count;
  size_t type_attributes_start;
  size_t type_attributes_end;
  size_t type_unsigned_start;
  size_t type_unsigned_end;
  size_t type_zerofill_start;
  size_t type_zerofill_end;
  size_t type_binary_start;
  size_t type_binary_end;
  size_t type_charset_start;
  size_t type_charset_end;
  size_t type_charset_value_start;
  size_t type_charset_value_end;
  size_t type_collation_start;
  size_t type_collation_end;
  size_t type_collation_value_start;
  size_t type_collation_value_end;
  size_t options_start;
  size_t options_end;
  const MyliteAstNode *options_node;
  size_t default_start;
  size_t default_end;
  const MyliteAstNode *default_node;
  size_t default_value_start;
  size_t default_value_end;
  const MyliteAstNode *default_value_node;
  size_t on_update_start;
  size_t on_update_end;
  const MyliteAstNode *on_update_node;
  size_t on_update_value_start;
  size_t on_update_value_end;
  const MyliteAstNode *on_update_value_node;
  size_t generated_start;
  size_t generated_end;
  const MyliteAstNode *generated_node;
  size_t generated_expression_start;
  size_t generated_expression_end;
  const MyliteAstNode *generated_expression_node;
  size_t generated_storage_start;
  size_t generated_storage_end;
  const MyliteAstNode *generated_storage_node;
  size_t comment_start;
  size_t comment_end;
  const MyliteAstNode *comment_node;
  size_t comment_value_start;
  size_t comment_value_end;
  size_t check_start;
  size_t check_end;
  const MyliteAstNode *check_node;
  size_t check_expression_start;
  size_t check_expression_end;
  const MyliteAstNode *check_expression_node;
  MyliteCreateTableCheckEnforcement check_enforcement;
  size_t check_enforcement_start;
  size_t check_enforcement_end;
  const MyliteAstNode *check_enforcement_node;
  size_t reference_start;
  size_t reference_end;
  const MyliteAstNode *reference_node;
} MyliteAstCreateTableColumn;

typedef struct MyliteAstCreateTableKeyPart {
  MyliteCreateTableKeyPartKind kind;
  MyliteCreateTableKeyPartOrder order;
  size_t start;
  size_t end;
  size_t name_start;
  size_t name_end;
  size_t expression_start;
  size_t expression_end;
  size_t prefix_start;
  size_t prefix_end;
  size_t prefix_value_start;
  size_t prefix_value_end;
  size_t order_start;
  size_t order_end;
} MyliteAstCreateTableKeyPart;

typedef struct MyliteAstCreateTableKeyOption {
  MyliteCreateTableKeyOptionKind kind;
  size_t start;
  size_t end;
  size_t name_start;
  size_t name_end;
  size_t value_start;
  size_t value_end;
} MyliteAstCreateTableKeyOption;

typedef struct MyliteAstCreateTableKey {
  MyliteCreateTableKeyKind kind;
  size_t start;
  size_t end;
  size_t constraint_name_start;
  size_t constraint_name_end;
  size_t name_start;
  size_t name_end;
  size_t index_type_start;
  size_t index_type_end;
  MyliteAstCreateTableKeyPart *columns;
  size_t column_count;
  size_t referenced_table_start;
  size_t referenced_table_end;
  size_t referenced_table_schema_start;
  size_t referenced_table_schema_end;
  size_t referenced_table_name_start;
  size_t referenced_table_name_end;
  MyliteAstCreateTableKeyPart *referenced_columns;
  size_t referenced_column_count;
  MyliteCreateTableForeignMatchKind foreign_match_kind;
  size_t foreign_match_start;
  size_t foreign_match_end;
  MyliteCreateTableForeignAction foreign_on_delete_action;
  size_t foreign_on_delete_start;
  size_t foreign_on_delete_end;
  MyliteCreateTableForeignAction foreign_on_update_action;
  size_t foreign_on_update_start;
  size_t foreign_on_update_end;
  size_t check_expression_start;
  size_t check_expression_end;
  MyliteCreateTableCheckEnforcement check_enforcement;
  size_t check_enforcement_start;
  size_t check_enforcement_end;
  MyliteAstCreateTableKeyOption *options;
  size_t option_count;
} MyliteAstCreateTableKey;

typedef struct MyliteAstCreateTableOption {
  MyliteCreateTableOptionKind kind;
  size_t start;
  size_t end;
  size_t name_start;
  size_t name_end;
  size_t value_start;
  size_t value_end;
} MyliteAstCreateTableOption;

typedef struct MyliteAstStatement {
  const MyliteAstNode *node;
  const char *symbol_name;
  MyliteStatementKind kind;
  MyliteAstStatementTarget *targets;
  size_t target_count;
  MyliteAstCreateTableColumn *create_table_columns;
  size_t create_table_column_count;
  MyliteAstCreateTableKey *create_table_keys;
  size_t create_table_key_count;
  MyliteAstCreateTableOption *create_table_options;
  size_t create_table_option_count;
  MyliteStatementTargetKind target_kind;
  size_t start;
  size_t end;
  size_t target_start;
  size_t target_end;
  size_t target_schema_start;
  size_t target_schema_end;
  size_t target_name_start;
  size_t target_name_end;
} MyliteAstStatement;

struct MyliteAstNode {
  MyliteAstNodeKind kind;
  unsigned rule_id;
  const char *symbol_name;
  int token;
  int has_span;
  size_t start;
  size_t end;
  size_t child_count;
  MyliteAstNode **children;
};

struct MyliteAst {
  MyliteAstNode *root;
  MyliteAstChunk *chunks;
  MyliteAstStatement *statements;
  char *source;
  size_t source_length;
  size_t node_count;
  size_t statement_count;
  size_t allocated_bytes;
};

void *MyliteTidbParseAlloc(void *(*malloc_proc)(size_t));
void MyliteTidbParseFree(void *parser, void (*free_proc)(void *));
void MyliteTidbParse(void *parser, int token, MyliteAstNode *token_value,
                     MyliteParserState *state);

static MyliteParseStatus parse_sql_common(const char *sql, MyliteParseResult *result,
                                          MyliteAst **ast_out, int build_ast);
static int accepts_parser_shortcut(const char *sql);
static int accepts_parser_fallback(const char *sql);
static MyliteAst *mylite_ast_create(const char *source);
static void *mylite_ast_alloc(MyliteAst *ast, size_t size);
static MyliteAstNode *mylite_ast_make_node(MyliteParserState *state,
                                           MyliteAstNodeKind kind,
                                           unsigned rule_id,
                                           const char *symbol_name, int token,
                                           size_t child_count,
                                           MyliteAstNode *const *children);
static void mylite_ast_set_node_span_from_children(MyliteAstNode *node);
static int mylite_ast_finalize_statements(MyliteAst *ast);
static size_t mylite_ast_count_top_level_statements(const MyliteAstNode *node);
static int mylite_ast_fill_top_level_statements(MyliteAst *ast,
                                                const MyliteAstNode *node,
                                                size_t *index);
static int mylite_ast_init_statement(MyliteAstStatement *statement,
                                     MyliteAst *ast, const MyliteAstNode *node);
static const MyliteAstNode *mylite_ast_statement_payload(const MyliteAstNode *node);
static MyliteStatementKind mylite_ast_classify_statement(const char *symbol_name);
static int mylite_ast_set_statement_target(MyliteAstStatement *statement,
                                           MyliteAst *ast,
                                           const MyliteAstNode *payload);
static int mylite_ast_set_statement_details(MyliteAst *ast,
                                            MyliteAstStatement *statement,
                                            const MyliteAstNode *payload);
static MyliteStatementTargetKind mylite_ast_target_kind_for_statement(
    MyliteStatementKind kind, const char *symbol_name);
static int mylite_ast_collect_statement_targets(MyliteAst *ast,
                                                MyliteAstStatement *statement,
                                                const MyliteAstNode *payload);
static int mylite_ast_collect_rename_targets(MyliteAst *ast,
                                             MyliteAstStatement *statement,
                                             const MyliteAstNode *node);
static int mylite_ast_collect_symbol_targets(MyliteAst *ast,
                                             MyliteAstStatement *statement,
                                             const MyliteAstNode *node,
                                             const char *symbol_name,
                                             MyliteStatementTargetKind kind,
                                             MyliteStatementTargetRole role);
static int mylite_ast_append_statement_target(MyliteAst *ast,
                                              MyliteAstStatement *statement,
                                              MyliteStatementTargetKind kind,
                                              MyliteStatementTargetRole role,
                                              const MyliteAstNode *target);
static void mylite_ast_fill_statement_target(MyliteAstStatementTarget *target,
                                             MyliteStatementTargetKind kind,
                                             MyliteStatementTargetRole role,
                                             const MyliteAstNode *node);
static void mylite_ast_mirror_first_statement_target(MyliteAstStatement *statement);
static int mylite_ast_collect_create_table_columns(MyliteAst *ast,
                                                   MyliteAstStatement *statement,
                                                   const MyliteAstNode *payload);
static int mylite_ast_collect_create_table_keys(MyliteAst *ast,
                                                MyliteAstStatement *statement,
                                                const MyliteAstNode *payload);
static size_t mylite_ast_count_create_table_columns(const MyliteAstNode *node);
static int mylite_ast_fill_create_table_columns(MyliteAst *ast,
                                                MyliteAstStatement *statement,
                                                const MyliteAstNode *node,
                                                size_t *index);
static int mylite_ast_fill_create_table_column(
    MyliteAst *ast, MyliteAstCreateTableColumn *column,
    const MyliteAstNode *node);
static int mylite_ast_set_create_table_column_name_value(
    MyliteAst *ast, MyliteAstCreateTableColumn *column);
static void mylite_ast_set_create_table_column_type_details(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *type);
static void mylite_ast_set_create_table_column_type_tail_details(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *type_body,
    const MyliteAstNode *type_name);
static void mylite_ast_set_create_table_column_type_attribute_details(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *type);
static void mylite_ast_set_create_table_column_type_unsigned_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *type);
static void mylite_ast_set_create_table_column_type_zerofill_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *type);
static void mylite_ast_set_create_table_column_type_charset_details(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *type);
static void mylite_ast_set_create_table_column_type_binary_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *attribute);
static void mylite_ast_set_create_table_column_type_charset_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *attribute);
static void mylite_ast_set_create_table_column_type_collation_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *attribute);
static int mylite_ast_set_create_table_column_type_parameter_values(
    MyliteAst *ast, MyliteAstCreateTableColumn *column);
static size_t mylite_ast_parse_unsigned_parameters(
    const char *source, size_t start, size_t end, unsigned long long *values,
    size_t value_capacity);
static int mylite_ast_set_create_table_column_type_elements(
    MyliteAst *ast, MyliteAstCreateTableColumn *column);
static size_t mylite_ast_count_create_table_column_type_elements(
    const MyliteAstNode *node);
static void mylite_ast_fill_create_table_column_type_elements(
    MyliteAstCreateTableTypeElement *elements, const MyliteAstNode *node,
    size_t *index);
static int mylite_ast_set_create_table_column_type_element_values(
    MyliteAst *ast, MyliteAstCreateTableColumn *column);
static int mylite_ast_decode_sql_string_literal(MyliteAst *ast, size_t start,
                                                size_t end,
                                                const char **value,
                                                size_t *value_length);
static size_t mylite_ast_decoded_sql_string_literal_length(const char *source,
                                                           size_t start,
                                                           size_t end);
static size_t mylite_ast_write_decoded_sql_string_literal(char *target,
                                                          const char *source,
                                                          size_t start,
                                                          size_t end);
static int mylite_ast_sql_string_escape_value(int ch);
static int mylite_ast_decode_identifier(MyliteAst *ast, size_t start,
                                        size_t end, const char **value,
                                        size_t *value_length);
static size_t mylite_ast_decoded_identifier_length(const char *source,
                                                   size_t start, size_t end);
static size_t mylite_ast_write_decoded_identifier(char *target,
                                                  const char *source,
                                                  size_t start, size_t end);
static void mylite_ast_set_create_table_column_type_shape(
    MyliteAstCreateTableColumn *column);
static void mylite_ast_set_create_table_column_type_length(
    MyliteAstCreateTableColumn *column, size_t parameter_index);
static void mylite_ast_set_create_table_column_type_precision_scale(
    MyliteAstCreateTableColumn *column);
static void mylite_ast_set_create_table_column_type_fsp(
    MyliteAstCreateTableColumn *column);
static int mylite_ast_is_column_type_name_continuation(const MyliteAstNode *node);
static int mylite_ast_is_column_type_parameter_child(const MyliteAstNode *node);
static int mylite_ast_is_column_type_attribute_child(const MyliteAstNode *node);
static const MyliteAstNode *mylite_ast_find_first_symbol_after(
    const MyliteAstNode *node, const char *symbol_name, size_t min_start);
static const MyliteAstNode *mylite_ast_find_first_token_after(
    const MyliteAstNode *node, int token, size_t min_start);
static const MyliteAstNode *mylite_ast_direct_child_token(
    const MyliteAstNode *node, size_t child_index, int token);
static void mylite_ast_set_create_table_column_option_details(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *options);
static void mylite_ast_set_create_table_column_option_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *option);
static void mylite_ast_set_create_table_column_default_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *option);
static void mylite_ast_set_create_table_column_on_update_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *option);
static void mylite_ast_set_create_table_column_generated_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *option);
static void mylite_ast_set_create_table_column_comment_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *option);
static void mylite_ast_set_create_table_column_check_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *option);
static void mylite_ast_set_create_table_column_reference_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *option);
static MyliteCreateTableColumnTypeFamily mylite_ast_classify_column_type(
    const MyliteAstNode *type);
static MyliteCreateTableColumnTypeKind mylite_ast_classify_column_type_kind(
    const MyliteAstNode *type);
static MyliteCreateTableColumnStorageClass
mylite_ast_classify_column_storage_class(MyliteCreateTableColumnTypeKind kind);
static unsigned int mylite_ast_collect_column_flags(const MyliteAstNode *type,
                                                    const MyliteAstNode *options);
static unsigned int mylite_ast_collect_column_type_flags(const MyliteAstNode *type);
static unsigned int mylite_ast_collect_column_option_flags(
    const MyliteAstNode *node);
static size_t mylite_ast_count_create_table_keys(const MyliteAstNode *node);
static void mylite_ast_fill_create_table_keys(MyliteAst *ast,
                                              MyliteAstStatement *statement,
                                              const MyliteAstNode *node,
                                              size_t *index, int *ok);
static int mylite_ast_fill_create_table_key(MyliteAst *ast,
                                            MyliteAstCreateTableKey *key,
                                            const MyliteAstNode *constraint);
static MyliteCreateTableKeyKind mylite_ast_classify_create_table_key(
    const MyliteAstNode *constraint_elem);
static void mylite_ast_set_create_table_key_names(MyliteAstCreateTableKey *key,
                                                  const MyliteAstNode *constraint);
static void mylite_ast_set_create_table_key_index_type(
    MyliteAstCreateTableKey *key, const MyliteAstNode *constraint_elem);
static int mylite_ast_set_create_table_key_reference(
    MyliteAst *ast, MyliteAstCreateTableKey *key,
    const MyliteAstNode *constraint_elem);
static void mylite_ast_set_create_table_key_check(
    MyliteAstCreateTableKey *key, const MyliteAstNode *constraint_elem);
static MyliteCreateTableForeignMatchKind mylite_ast_classify_foreign_match(
    const MyliteAstNode *node);
static MyliteCreateTableForeignAction mylite_ast_classify_foreign_action(
    const MyliteAstNode *node);
static MyliteCreateTableCheckEnforcement mylite_ast_classify_check_enforcement(
    const MyliteAstNode *node);
static int mylite_ast_set_create_table_key_parts(MyliteAst *ast,
                                                 MyliteAstCreateTableKey *key,
                                                 const MyliteAstNode *list,
                                                 int referenced);
static int mylite_ast_set_create_table_key_options(MyliteAst *ast,
                                                   MyliteAstCreateTableKey *key,
                                                   const MyliteAstNode *list);
static size_t mylite_ast_count_index_part_specs(const MyliteAstNode *node);
static void mylite_ast_fill_index_part_specs(MyliteAstCreateTableKeyPart *parts,
                                             size_t count,
                                             const MyliteAstNode *node,
                                             size_t *index);
static void mylite_ast_fill_key_part(MyliteAstCreateTableKeyPart *part,
                                     const MyliteAstNode *node);
static size_t mylite_ast_count_index_options(const MyliteAstNode *node);
static void mylite_ast_fill_index_options(MyliteAstCreateTableKeyOption *options,
                                          size_t count,
                                          const MyliteAstNode *node,
                                          size_t *index);
static void mylite_ast_fill_key_option(MyliteAstCreateTableKeyOption *option,
                                       const MyliteAstNode *node);
static MyliteCreateTableKeyOptionKind mylite_ast_classify_key_option(
    const MyliteAstNode *node);
static const MyliteAstNode *mylite_ast_find_key_option_name(
    const MyliteAstNode *node, MyliteCreateTableKeyOptionKind kind);
static const MyliteAstNode *mylite_ast_find_nth_symbol(const MyliteAstNode *node,
                                                       const char *symbol_name,
                                                       size_t *remaining);
static const MyliteAstNode *mylite_ast_find_constraint_elem(
    const MyliteAstNode *constraint);
static const MyliteAstNode *mylite_ast_find_constraint_keyword_name(
    const MyliteAstNode *constraint);
static int mylite_ast_collect_create_table_options(MyliteAst *ast,
                                                   MyliteAstStatement *statement,
                                                   const MyliteAstNode *payload);
static size_t mylite_ast_count_create_table_options(const MyliteAstNode *node);
static void mylite_ast_fill_create_table_options(MyliteAstStatement *statement,
                                                 const MyliteAstNode *node,
                                                 size_t *index);
static void mylite_ast_fill_create_table_option(
    MyliteAstCreateTableOption *option, const MyliteAstNode *node);
static MyliteCreateTableOptionKind mylite_ast_classify_create_table_option(
    const MyliteAstNode *node);
static void mylite_ast_set_create_table_option_name(
    MyliteAstCreateTableOption *option, const MyliteAstNode *node);
static void mylite_ast_set_create_table_option_value(
    MyliteAstCreateTableOption *option, const MyliteAstNode *node);
static const MyliteAstNode *mylite_ast_find_create_table_option_name(
    const MyliteAstNode *node, MyliteCreateTableOptionKind kind);
static const MyliteAstNode *mylite_ast_find_first_direct_spanned_child(
    const MyliteAstNode *node);
static const MyliteAstNode *mylite_ast_find_first_spanned_child(
    const MyliteAstNode *node);
static void mylite_ast_extend_span(size_t *start, size_t *end,
                                   const MyliteAstNode *node);
static void mylite_ast_collect_value_span_after(const MyliteAstNode *node,
                                                size_t min_start,
                                                size_t *value_start,
                                                size_t *value_end);
static const MyliteAstCreateTableOption *mylite_ast_create_table_option_at(
    const MyliteAst *ast, size_t statement_index, size_t option_index);
static const MyliteAstCreateTableKey *mylite_ast_create_table_key_at(
    const MyliteAst *ast, size_t statement_index, size_t key_index);
static const MyliteAstCreateTableKeyPart *mylite_ast_create_table_key_part_at(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index, int referenced);
static const MyliteAstCreateTableKeyOption *mylite_ast_create_table_key_option_at(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t option_index);
static const MyliteAstCreateTableColumn *mylite_ast_create_table_column_at(
    const MyliteAst *ast, size_t statement_index, size_t column_index);
static const MyliteAstCreateTableTypeElement *
mylite_ast_create_table_column_type_element_at(const MyliteAst *ast,
                                               size_t statement_index,
                                               size_t column_index,
                                               size_t element_index);
static const MyliteAstNode *mylite_ast_find_first_symbol(const MyliteAstNode *node,
                                                         const char *symbol_name);
static const MyliteAstNode *mylite_ast_find_first_token(const MyliteAstNode *node,
                                                        int token);
static int mylite_ast_first_token(const MyliteAstNode *node);
static void mylite_ast_set_table_name_span_parts(const MyliteAstNode *node,
                                                 size_t *schema_start,
                                                 size_t *schema_end,
                                                 size_t *name_start,
                                                 size_t *name_end);
static int mylite_ast_is_nested_target_boundary(const MyliteAstNode *node);
static void mylite_ast_set_table_name_parts(MyliteAstStatementTarget *target,
                                            const MyliteAstNode *node);
static int symbol_is_one_of(const char *symbol_name, const char *const *symbols,
                            size_t count);
static int symbol_has_prefix(const char *symbol_name, const char *prefix);
static const MyliteAstStatement *mylite_ast_statement_at(const MyliteAst *ast,
                                                         size_t index);
static const MyliteAstStatementTarget *mylite_ast_statement_target_at(
    const MyliteAst *ast, size_t statement_index, size_t target_index);
static void mylite_parser_state_no_memory(MyliteParserState *state);
static int accepts_mysqltest_harness_statement(const char *sql);
static int accepts_parenthesized_create_table_set_statement(const char *sql);
static int accepts_alter_user_default_role_statement(const char *sql);
static int accepts_create_user_default_role_statement(const char *sql);
static int accepts_flush_option_list_statement(const char *sql);
static int accepts_select_where_having_without_from(const char *sql);
static int accepts_select_group_having_without_from(const char *sql);
static int accepts_select_from_dual_group_having(const char *sql);
static int accepts_select_not_like_pipes_statement(const char *sql);
static int accepts_analyze_histogram_table_list(const char *sql);
static int accepts_mysql_resource_group_statement(const char *sql);
static int accepts_create_procedure_with_characteristics(const char *sql);
static int skip_create_procedure_head(const char **cursor);
static int skip_definer_account(const char **cursor);
static int accepts_insert_alias_on_duplicate(const char *sql);
static int accepts_set_select_into_before_lock(const char *sql);
static int match_ascii_ci(const char **cursor, const char *expected);
static void skip_ascii_space(const char **cursor);
static void skip_sql_quoted(const char **cursor, int quote);
static int is_ascii_identifier_char(int ch);
static int ascii_tolower(int ch);

MyliteParseStatus mylite_parse_sql(const char *sql, MyliteParseResult *result) {
  return parse_sql_common(sql, result, NULL, 0);
}

MyliteParseStatus mylite_parse_sql_ast(const char *sql, MyliteAst **ast,
                                       MyliteParseResult *result) {
  if (ast == NULL) {
    if (result != NULL) {
      memset(result, 0, sizeof(*result));
      result->status = MYLITE_PARSE_LEX_ERROR;
      snprintf(result->message, sizeof(result->message), "AST output is null");
    }
    return MYLITE_PARSE_LEX_ERROR;
  }
  return parse_sql_common(sql, result, ast, 1);
}

const char *mylite_parse_status_name(MyliteParseStatus status) {
  switch (status) {
    case MYLITE_PARSE_OK:
      return "ok";
    case MYLITE_PARSE_SYNTAX_ERROR:
      return "syntax_error";
    case MYLITE_PARSE_LEX_ERROR:
      return "lex_error";
    case MYLITE_PARSE_NO_MEMORY:
      return "no_memory";
  }
  return "unknown";
}

const char *mylite_statement_kind_name(MyliteStatementKind kind) {
  switch (kind) {
    case MYLITE_STATEMENT_UNKNOWN:
      return "unknown";
    case MYLITE_STATEMENT_EMPTY:
      return "empty";
    case MYLITE_STATEMENT_SELECT:
      return "select";
    case MYLITE_STATEMENT_INSERT:
      return "insert";
    case MYLITE_STATEMENT_UPDATE:
      return "update";
    case MYLITE_STATEMENT_DELETE:
      return "delete";
    case MYLITE_STATEMENT_REPLACE:
      return "replace";
    case MYLITE_STATEMENT_CREATE:
      return "create";
    case MYLITE_STATEMENT_ALTER:
      return "alter";
    case MYLITE_STATEMENT_DROP:
      return "drop";
    case MYLITE_STATEMENT_RENAME:
      return "rename";
    case MYLITE_STATEMENT_TRUNCATE:
      return "truncate";
    case MYLITE_STATEMENT_SET:
      return "set";
    case MYLITE_STATEMENT_SHOW:
      return "show";
    case MYLITE_STATEMENT_EXPLAIN:
      return "explain";
    case MYLITE_STATEMENT_DO:
      return "do";
    case MYLITE_STATEMENT_CALL:
      return "call";
    case MYLITE_STATEMENT_PREPARE:
      return "prepare";
    case MYLITE_STATEMENT_EXECUTE:
      return "execute";
    case MYLITE_STATEMENT_DEALLOCATE:
      return "deallocate";
    case MYLITE_STATEMENT_TRANSACTION:
      return "transaction";
    case MYLITE_STATEMENT_LOCK:
      return "lock";
    case MYLITE_STATEMENT_UTILITY:
      return "utility";
  }
  return "unknown";
}

const char *mylite_statement_target_kind_name(MyliteStatementTargetKind kind) {
  switch (kind) {
    case MYLITE_STATEMENT_TARGET_NONE:
      return "none";
    case MYLITE_STATEMENT_TARGET_TABLE:
      return "table";
    case MYLITE_STATEMENT_TARGET_DATABASE:
      return "database";
    case MYLITE_STATEMENT_TARGET_VIEW:
      return "view";
    case MYLITE_STATEMENT_TARGET_ROUTINE:
      return "routine";
    case MYLITE_STATEMENT_TARGET_ACCOUNT:
      return "account";
    case MYLITE_STATEMENT_TARGET_VARIABLE:
      return "variable";
    case MYLITE_STATEMENT_TARGET_UNKNOWN:
      return "unknown";
  }
  return "unknown";
}

const char *mylite_statement_target_role_name(MyliteStatementTargetRole role) {
  switch (role) {
    case MYLITE_STATEMENT_TARGET_ROLE_NONE:
      return "none";
    case MYLITE_STATEMENT_TARGET_ROLE_PRIMARY:
      return "primary";
    case MYLITE_STATEMENT_TARGET_ROLE_SOURCE:
      return "source";
    case MYLITE_STATEMENT_TARGET_ROLE_DESTINATION:
      return "destination";
  }
  return "unknown";
}

const char *mylite_create_table_column_type_family_name(
    MyliteCreateTableColumnTypeFamily family) {
  switch (family) {
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_UNKNOWN:
      return "unknown";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC:
      return "numeric";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_STRING:
      return "string";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_TEMPORAL:
      return "temporal";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_JSON:
      return "json";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_ENUM:
      return "enum";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_SET:
      return "set";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_SPATIAL:
      return "spatial";
  }
  return "unknown";
}

const char *mylite_create_table_column_type_kind_name(
    MyliteCreateTableColumnTypeKind kind) {
  switch (kind) {
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_UNKNOWN:
      return "unknown";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TINYINT:
      return "tinyint";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_SMALLINT:
      return "smallint";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MEDIUMINT:
      return "mediumint";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_INT:
      return "int";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BIGINT:
      return "bigint";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BOOL:
      return "bool";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DECIMAL:
      return "decimal";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_FLOAT:
      return "float";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_REAL:
      return "real";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DOUBLE:
      return "double";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BIT:
      return "bit";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_CHAR:
      return "char";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VARCHAR:
      return "varchar";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_NCHAR:
      return "nchar";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_NVARCHAR:
      return "nvarchar";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BINARY:
      return "binary";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VARBINARY:
      return "varbinary";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TINYBLOB:
      return "tinyblob";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BLOB:
      return "blob";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MEDIUMBLOB:
      return "mediumblob";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONGBLOB:
      return "longblob";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TINYTEXT:
      return "tinytext";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TEXT:
      return "text";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MEDIUMTEXT:
      return "mediumtext";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONGTEXT:
      return "longtext";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_ENUM:
      return "enum";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_SET:
      return "set";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_JSON:
      return "json";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG:
      return "long";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG_VARCHAR:
      return "long_varchar";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG_VARBINARY:
      return "long_varbinary";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VECTOR:
      return "vector";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DATE:
      return "date";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DATETIME:
      return "datetime";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TIMESTAMP:
      return "timestamp";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TIME:
      return "time";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_YEAR:
      return "year";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_GEOMETRY:
      return "geometry";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_POINT:
      return "point";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LINESTRING:
      return "linestring";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_POLYGON:
      return "polygon";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MULTIPOINT:
      return "multipoint";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MULTILINESTRING:
      return "multilinestring";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MULTIPOLYGON:
      return "multipolygon";
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_GEOMETRYCOLLECTION:
      return "geometrycollection";
  }
  return "unknown";
}

const char *mylite_create_table_column_storage_class_name(
    MyliteCreateTableColumnStorageClass storage_class) {
  switch (storage_class) {
    case MYLITE_CREATE_TABLE_COLUMN_STORAGE_UNKNOWN:
      return "unknown";
    case MYLITE_CREATE_TABLE_COLUMN_STORAGE_INTEGER:
      return "integer";
    case MYLITE_CREATE_TABLE_COLUMN_STORAGE_DECIMAL:
      return "decimal";
    case MYLITE_CREATE_TABLE_COLUMN_STORAGE_FLOAT:
      return "float";
    case MYLITE_CREATE_TABLE_COLUMN_STORAGE_BIT:
      return "bit";
    case MYLITE_CREATE_TABLE_COLUMN_STORAGE_FIXED_STRING:
      return "fixed_string";
    case MYLITE_CREATE_TABLE_COLUMN_STORAGE_VARIABLE_STRING:
      return "variable_string";
    case MYLITE_CREATE_TABLE_COLUMN_STORAGE_BINARY_STRING:
      return "binary_string";
    case MYLITE_CREATE_TABLE_COLUMN_STORAGE_BLOB:
      return "blob";
    case MYLITE_CREATE_TABLE_COLUMN_STORAGE_TEXT:
      return "text";
    case MYLITE_CREATE_TABLE_COLUMN_STORAGE_ENUM:
      return "enum";
    case MYLITE_CREATE_TABLE_COLUMN_STORAGE_SET:
      return "set";
    case MYLITE_CREATE_TABLE_COLUMN_STORAGE_JSON:
      return "json";
    case MYLITE_CREATE_TABLE_COLUMN_STORAGE_TEMPORAL:
      return "temporal";
    case MYLITE_CREATE_TABLE_COLUMN_STORAGE_SPATIAL:
      return "spatial";
    case MYLITE_CREATE_TABLE_COLUMN_STORAGE_VECTOR:
      return "vector";
  }
  return "unknown";
}

const char *mylite_create_table_key_kind_name(MyliteCreateTableKeyKind kind) {
  switch (kind) {
    case MYLITE_CREATE_TABLE_KEY_UNKNOWN:
      return "unknown";
    case MYLITE_CREATE_TABLE_KEY_PRIMARY:
      return "primary";
    case MYLITE_CREATE_TABLE_KEY_INDEX:
      return "index";
    case MYLITE_CREATE_TABLE_KEY_UNIQUE:
      return "unique";
    case MYLITE_CREATE_TABLE_KEY_FULLTEXT:
      return "fulltext";
    case MYLITE_CREATE_TABLE_KEY_SPATIAL:
      return "spatial";
    case MYLITE_CREATE_TABLE_KEY_FOREIGN:
      return "foreign";
    case MYLITE_CREATE_TABLE_KEY_CHECK:
      return "check";
  }
  return "unknown";
}

const char *mylite_create_table_key_part_kind_name(
    MyliteCreateTableKeyPartKind kind) {
  switch (kind) {
    case MYLITE_CREATE_TABLE_KEY_PART_UNKNOWN:
      return "unknown";
    case MYLITE_CREATE_TABLE_KEY_PART_COLUMN:
      return "column";
    case MYLITE_CREATE_TABLE_KEY_PART_EXPRESSION:
      return "expression";
  }
  return "unknown";
}

const char *mylite_create_table_key_part_order_name(
    MyliteCreateTableKeyPartOrder order) {
  switch (order) {
    case MYLITE_CREATE_TABLE_KEY_PART_ORDER_UNSPECIFIED:
      return "unspecified";
    case MYLITE_CREATE_TABLE_KEY_PART_ORDER_ASC:
      return "asc";
    case MYLITE_CREATE_TABLE_KEY_PART_ORDER_DESC:
      return "desc";
  }
  return "unspecified";
}

const char *mylite_create_table_key_option_kind_name(
    MyliteCreateTableKeyOptionKind kind) {
  switch (kind) {
    case MYLITE_CREATE_TABLE_KEY_OPTION_UNKNOWN:
      return "unknown";
    case MYLITE_CREATE_TABLE_KEY_OPTION_INDEX_TYPE:
      return "index_type";
    case MYLITE_CREATE_TABLE_KEY_OPTION_KEY_BLOCK_SIZE:
      return "key_block_size";
    case MYLITE_CREATE_TABLE_KEY_OPTION_COMMENT:
      return "comment";
    case MYLITE_CREATE_TABLE_KEY_OPTION_WITH_PARSER:
      return "with_parser";
    case MYLITE_CREATE_TABLE_KEY_OPTION_VISIBLE:
      return "visible";
    case MYLITE_CREATE_TABLE_KEY_OPTION_INVISIBLE:
      return "invisible";
    case MYLITE_CREATE_TABLE_KEY_OPTION_SECONDARY_ENGINE_ATTRIBUTE:
      return "secondary_engine_attribute";
    case MYLITE_CREATE_TABLE_KEY_OPTION_WHERE:
      return "where";
  }
  return "unknown";
}

const char *mylite_create_table_foreign_match_kind_name(
    MyliteCreateTableForeignMatchKind kind) {
  switch (kind) {
    case MYLITE_CREATE_TABLE_FOREIGN_MATCH_UNSPECIFIED:
      return "unspecified";
    case MYLITE_CREATE_TABLE_FOREIGN_MATCH_FULL:
      return "full";
    case MYLITE_CREATE_TABLE_FOREIGN_MATCH_PARTIAL:
      return "partial";
    case MYLITE_CREATE_TABLE_FOREIGN_MATCH_SIMPLE:
      return "simple";
  }
  return "unspecified";
}

const char *mylite_create_table_foreign_action_name(
    MyliteCreateTableForeignAction action) {
  switch (action) {
    case MYLITE_CREATE_TABLE_FOREIGN_ACTION_UNSPECIFIED:
      return "unspecified";
    case MYLITE_CREATE_TABLE_FOREIGN_ACTION_RESTRICT:
      return "restrict";
    case MYLITE_CREATE_TABLE_FOREIGN_ACTION_CASCADE:
      return "cascade";
    case MYLITE_CREATE_TABLE_FOREIGN_ACTION_SET_NULL:
      return "set_null";
    case MYLITE_CREATE_TABLE_FOREIGN_ACTION_NO_ACTION:
      return "no_action";
    case MYLITE_CREATE_TABLE_FOREIGN_ACTION_SET_DEFAULT:
      return "set_default";
  }
  return "unspecified";
}

const char *mylite_create_table_check_enforcement_name(
    MyliteCreateTableCheckEnforcement enforcement) {
  switch (enforcement) {
    case MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_UNSPECIFIED:
      return "unspecified";
    case MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_ENFORCED:
      return "enforced";
    case MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_NOT_ENFORCED:
      return "not_enforced";
  }
  return "unspecified";
}

const char *mylite_create_table_option_kind_name(
    MyliteCreateTableOptionKind kind) {
  switch (kind) {
    case MYLITE_CREATE_TABLE_OPTION_UNKNOWN:
      return "unknown";
    case MYLITE_CREATE_TABLE_OPTION_ENGINE:
      return "engine";
    case MYLITE_CREATE_TABLE_OPTION_SECONDARY_ENGINE:
      return "secondary_engine";
    case MYLITE_CREATE_TABLE_OPTION_CHARSET:
      return "charset";
    case MYLITE_CREATE_TABLE_OPTION_COLLATE:
      return "collate";
    case MYLITE_CREATE_TABLE_OPTION_AUTO_INCREMENT:
      return "auto_increment";
    case MYLITE_CREATE_TABLE_OPTION_COMMENT:
      return "comment";
    case MYLITE_CREATE_TABLE_OPTION_ROW_FORMAT:
      return "row_format";
    case MYLITE_CREATE_TABLE_OPTION_KEY_BLOCK_SIZE:
      return "key_block_size";
    case MYLITE_CREATE_TABLE_OPTION_AUTOEXTEND_SIZE:
      return "autoextend_size";
    case MYLITE_CREATE_TABLE_OPTION_AVG_ROW_LENGTH:
      return "avg_row_length";
    case MYLITE_CREATE_TABLE_OPTION_MAX_ROWS:
      return "max_rows";
    case MYLITE_CREATE_TABLE_OPTION_MIN_ROWS:
      return "min_rows";
    case MYLITE_CREATE_TABLE_OPTION_DELAY_KEY_WRITE:
      return "delay_key_write";
    case MYLITE_CREATE_TABLE_OPTION_ENCRYPTION:
      return "encryption";
    case MYLITE_CREATE_TABLE_OPTION_STATS_PERSISTENT:
      return "stats_persistent";
    case MYLITE_CREATE_TABLE_OPTION_PACK_KEYS:
      return "pack_keys";
    case MYLITE_CREATE_TABLE_OPTION_TABLESPACE:
      return "tablespace";
    case MYLITE_CREATE_TABLE_OPTION_STORAGE:
      return "storage";
    case MYLITE_CREATE_TABLE_OPTION_COMPRESSION:
      return "compression";
    case MYLITE_CREATE_TABLE_OPTION_CONNECTION:
      return "connection";
    case MYLITE_CREATE_TABLE_OPTION_PASSWORD:
      return "password";
    case MYLITE_CREATE_TABLE_OPTION_INSERT_METHOD:
      return "insert_method";
    case MYLITE_CREATE_TABLE_OPTION_DATA_DIRECTORY:
      return "data_directory";
    case MYLITE_CREATE_TABLE_OPTION_INDEX_DIRECTORY:
      return "index_directory";
    case MYLITE_CREATE_TABLE_OPTION_UNION:
      return "union";
    case MYLITE_CREATE_TABLE_OPTION_ENGINE_ATTRIBUTE:
      return "engine_attribute";
    case MYLITE_CREATE_TABLE_OPTION_SECONDARY_ENGINE_ATTRIBUTE:
      return "secondary_engine_attribute";
  }
  return "unknown";
}

void mylite_ast_free(MyliteAst *ast) {
  if (ast == NULL) {
    return;
  }
  MyliteAstChunk *chunk = ast->chunks;
  while (chunk != NULL) {
    MyliteAstChunk *next = chunk->next;
    free(chunk);
    chunk = next;
  }
  free(ast->source);
  free(ast);
}

const MyliteAstNode *mylite_ast_root(const MyliteAst *ast) {
  return ast == NULL ? NULL : ast->root;
}

size_t mylite_ast_node_count(const MyliteAst *ast) {
  return ast == NULL ? 0 : ast->node_count;
}

size_t mylite_ast_allocated_bytes(const MyliteAst *ast) {
  return ast == NULL ? 0 : ast->allocated_bytes;
}

size_t mylite_ast_statement_count(const MyliteAst *ast) {
  return ast == NULL ? 0 : ast->statement_count;
}

MyliteStatementKind mylite_ast_statement_kind(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? MYLITE_STATEMENT_UNKNOWN : statement->kind;
}

const char *mylite_ast_statement_symbol_name(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? NULL : statement->symbol_name;
}

const MyliteAstNode *mylite_ast_statement_node(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? NULL : statement->node;
}

size_t mylite_ast_statement_start(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? 0 : statement->start;
}

size_t mylite_ast_statement_end(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? 0 : statement->end;
}

MyliteStatementTargetKind mylite_ast_statement_target_kind(const MyliteAst *ast,
                                                           size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? MYLITE_STATEMENT_TARGET_NONE : statement->target_kind;
}

size_t mylite_ast_statement_target_start(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? 0 : statement->target_start;
}

size_t mylite_ast_statement_target_end(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? 0 : statement->target_end;
}

size_t mylite_ast_statement_target_schema_start(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? 0 : statement->target_schema_start;
}

size_t mylite_ast_statement_target_schema_end(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? 0 : statement->target_schema_end;
}

size_t mylite_ast_statement_target_name_start(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? 0 : statement->target_name_start;
}

size_t mylite_ast_statement_target_name_end(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? 0 : statement->target_name_end;
}

size_t mylite_ast_statement_target_count(const MyliteAst *ast,
                                         size_t statement_index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? 0 : statement->target_count;
}

MyliteStatementTargetKind mylite_ast_statement_target_kind_at(
    const MyliteAst *ast, size_t statement_index, size_t target_index) {
  const MyliteAstStatementTarget *target =
      mylite_ast_statement_target_at(ast, statement_index, target_index);
  return target == NULL ? MYLITE_STATEMENT_TARGET_NONE : target->kind;
}

MyliteStatementTargetRole mylite_ast_statement_target_role_at(
    const MyliteAst *ast, size_t statement_index, size_t target_index) {
  const MyliteAstStatementTarget *target =
      mylite_ast_statement_target_at(ast, statement_index, target_index);
  return target == NULL ? MYLITE_STATEMENT_TARGET_ROLE_NONE : target->role;
}

size_t mylite_ast_statement_target_start_at(const MyliteAst *ast,
                                            size_t statement_index,
                                            size_t target_index) {
  const MyliteAstStatementTarget *target =
      mylite_ast_statement_target_at(ast, statement_index, target_index);
  return target == NULL ? 0 : target->start;
}

size_t mylite_ast_statement_target_end_at(const MyliteAst *ast,
                                          size_t statement_index,
                                          size_t target_index) {
  const MyliteAstStatementTarget *target =
      mylite_ast_statement_target_at(ast, statement_index, target_index);
  return target == NULL ? 0 : target->end;
}

size_t mylite_ast_statement_target_schema_start_at(const MyliteAst *ast,
                                                   size_t statement_index,
                                                   size_t target_index) {
  const MyliteAstStatementTarget *target =
      mylite_ast_statement_target_at(ast, statement_index, target_index);
  return target == NULL ? 0 : target->schema_start;
}

size_t mylite_ast_statement_target_schema_end_at(const MyliteAst *ast,
                                                 size_t statement_index,
                                                 size_t target_index) {
  const MyliteAstStatementTarget *target =
      mylite_ast_statement_target_at(ast, statement_index, target_index);
  return target == NULL ? 0 : target->schema_end;
}

size_t mylite_ast_statement_target_name_start_at(const MyliteAst *ast,
                                                 size_t statement_index,
                                                 size_t target_index) {
  const MyliteAstStatementTarget *target =
      mylite_ast_statement_target_at(ast, statement_index, target_index);
  return target == NULL ? 0 : target->name_start;
}

size_t mylite_ast_statement_target_name_end_at(const MyliteAst *ast,
                                               size_t statement_index,
                                               size_t target_index) {
  const MyliteAstStatementTarget *target =
      mylite_ast_statement_target_at(ast, statement_index, target_index);
  return target == NULL ? 0 : target->name_end;
}

size_t mylite_ast_create_table_column_count(const MyliteAst *ast,
                                            size_t statement_index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? 0 : statement->create_table_column_count;
}

size_t mylite_ast_create_table_column_start(const MyliteAst *ast,
                                            size_t statement_index,
                                            size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->start;
}

size_t mylite_ast_create_table_column_end(const MyliteAst *ast,
                                          size_t statement_index,
                                          size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->end;
}

size_t mylite_ast_create_table_column_name_start(const MyliteAst *ast,
                                                 size_t statement_index,
                                                 size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->name_start;
}

size_t mylite_ast_create_table_column_name_end(const MyliteAst *ast,
                                               size_t statement_index,
                                               size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->name_end;
}

const char *mylite_ast_create_table_column_name_value(const MyliteAst *ast,
                                                      size_t statement_index,
                                                      size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? NULL : column->name_value;
}

size_t mylite_ast_create_table_column_name_value_length(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->name_value_length;
}

size_t mylite_ast_create_table_column_type_start(const MyliteAst *ast,
                                                 size_t statement_index,
                                                 size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_start;
}

size_t mylite_ast_create_table_column_type_end(const MyliteAst *ast,
                                               size_t statement_index,
                                               size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_end;
}

size_t mylite_ast_create_table_column_type_name_start(const MyliteAst *ast,
                                                      size_t statement_index,
                                                      size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_name_start;
}

size_t mylite_ast_create_table_column_type_name_end(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_name_end;
}

size_t mylite_ast_create_table_column_type_parameters_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_parameters_start;
}

size_t mylite_ast_create_table_column_type_parameters_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_parameters_end;
}

size_t mylite_ast_create_table_column_type_numeric_parameter_count(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_numeric_parameter_count;
}

unsigned long long mylite_ast_create_table_column_type_numeric_parameter_at(
    const MyliteAst *ast, size_t statement_index, size_t column_index,
    size_t parameter_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  if (column == NULL || parameter_index >= column->type_numeric_parameter_count ||
      parameter_index >= sizeof(column->type_numeric_parameters) /
                             sizeof(column->type_numeric_parameters[0])) {
    return 0;
  }
  return column->type_numeric_parameters[parameter_index];
}

size_t mylite_ast_create_table_column_type_element_count(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_element_count;
}

size_t mylite_ast_create_table_column_type_element_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index,
    size_t element_index) {
  const MyliteAstCreateTableTypeElement *element =
      mylite_ast_create_table_column_type_element_at(
          ast, statement_index, column_index, element_index);
  return element == NULL ? 0 : element->start;
}

size_t mylite_ast_create_table_column_type_element_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index,
    size_t element_index) {
  const MyliteAstCreateTableTypeElement *element =
      mylite_ast_create_table_column_type_element_at(
          ast, statement_index, column_index, element_index);
  return element == NULL ? 0 : element->end;
}

const char *mylite_ast_create_table_column_type_element_value(
    const MyliteAst *ast, size_t statement_index, size_t column_index,
    size_t element_index) {
  const MyliteAstCreateTableTypeElement *element =
      mylite_ast_create_table_column_type_element_at(
          ast, statement_index, column_index, element_index);
  return element == NULL ? NULL : element->value;
}

size_t mylite_ast_create_table_column_type_element_value_length(
    const MyliteAst *ast, size_t statement_index, size_t column_index,
    size_t element_index) {
  const MyliteAstCreateTableTypeElement *element =
      mylite_ast_create_table_column_type_element_at(
          ast, statement_index, column_index, element_index);
  return element == NULL ? 0 : element->value_length;
}

int mylite_ast_create_table_column_type_has_length(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column != NULL &&
         (column->type_shape_flags &
          MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_LENGTH) != 0;
}

unsigned long long mylite_ast_create_table_column_type_length(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_length;
}

int mylite_ast_create_table_column_type_has_precision(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column != NULL &&
         (column->type_shape_flags &
          MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_PRECISION) != 0;
}

unsigned long long mylite_ast_create_table_column_type_precision(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_precision;
}

int mylite_ast_create_table_column_type_has_scale(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column != NULL &&
         (column->type_shape_flags &
          MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_SCALE) != 0;
}

unsigned long long mylite_ast_create_table_column_type_scale(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_scale;
}

int mylite_ast_create_table_column_type_has_fractional_seconds_precision(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column != NULL &&
         (column->type_shape_flags &
          MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_FSP) != 0;
}

unsigned long long
mylite_ast_create_table_column_type_fractional_seconds_precision(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_fractional_seconds_precision;
}

size_t mylite_ast_create_table_column_type_attributes_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_attributes_start;
}

size_t mylite_ast_create_table_column_type_attributes_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_attributes_end;
}

size_t mylite_ast_create_table_column_type_unsigned_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_unsigned_start;
}

size_t mylite_ast_create_table_column_type_unsigned_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_unsigned_end;
}

size_t mylite_ast_create_table_column_type_zerofill_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_zerofill_start;
}

size_t mylite_ast_create_table_column_type_zerofill_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_zerofill_end;
}

size_t mylite_ast_create_table_column_type_binary_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_binary_start;
}

size_t mylite_ast_create_table_column_type_binary_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_binary_end;
}

size_t mylite_ast_create_table_column_type_charset_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_charset_start;
}

size_t mylite_ast_create_table_column_type_charset_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_charset_end;
}

size_t mylite_ast_create_table_column_type_charset_value_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_charset_value_start;
}

size_t mylite_ast_create_table_column_type_charset_value_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_charset_value_end;
}

size_t mylite_ast_create_table_column_type_collation_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_collation_start;
}

size_t mylite_ast_create_table_column_type_collation_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_collation_end;
}

size_t mylite_ast_create_table_column_type_collation_value_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_collation_value_start;
}

size_t mylite_ast_create_table_column_type_collation_value_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->type_collation_value_end;
}

size_t mylite_ast_create_table_column_options_start(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->options_start;
}

size_t mylite_ast_create_table_column_options_end(const MyliteAst *ast,
                                                  size_t statement_index,
                                                  size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->options_end;
}

size_t mylite_ast_create_table_column_default_start(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->default_start;
}

size_t mylite_ast_create_table_column_default_end(const MyliteAst *ast,
                                                  size_t statement_index,
                                                  size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->default_end;
}

size_t mylite_ast_create_table_column_default_value_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->default_value_start;
}

size_t mylite_ast_create_table_column_default_value_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->default_value_end;
}

size_t mylite_ast_create_table_column_on_update_start(const MyliteAst *ast,
                                                      size_t statement_index,
                                                      size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->on_update_start;
}

size_t mylite_ast_create_table_column_on_update_end(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->on_update_end;
}

size_t mylite_ast_create_table_column_on_update_value_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->on_update_value_start;
}

size_t mylite_ast_create_table_column_on_update_value_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->on_update_value_end;
}

size_t mylite_ast_create_table_column_generated_start(const MyliteAst *ast,
                                                      size_t statement_index,
                                                      size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->generated_start;
}

size_t mylite_ast_create_table_column_generated_end(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->generated_end;
}

size_t mylite_ast_create_table_column_generated_expression_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->generated_expression_start;
}

size_t mylite_ast_create_table_column_generated_expression_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->generated_expression_end;
}

size_t mylite_ast_create_table_column_generated_storage_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->generated_storage_start;
}

size_t mylite_ast_create_table_column_generated_storage_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->generated_storage_end;
}

size_t mylite_ast_create_table_column_comment_start(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->comment_start;
}

size_t mylite_ast_create_table_column_comment_end(const MyliteAst *ast,
                                                  size_t statement_index,
                                                  size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->comment_end;
}

size_t mylite_ast_create_table_column_comment_value_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->comment_value_start;
}

size_t mylite_ast_create_table_column_comment_value_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->comment_value_end;
}

size_t mylite_ast_create_table_column_check_start(const MyliteAst *ast,
                                                  size_t statement_index,
                                                  size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->check_start;
}

size_t mylite_ast_create_table_column_check_end(const MyliteAst *ast,
                                                size_t statement_index,
                                                size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->check_end;
}

size_t mylite_ast_create_table_column_check_expression_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->check_expression_start;
}

size_t mylite_ast_create_table_column_check_expression_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->check_expression_end;
}

MyliteCreateTableCheckEnforcement
mylite_ast_create_table_column_check_enforcement(const MyliteAst *ast,
                                                 size_t statement_index,
                                                 size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_UNSPECIFIED
                        : column->check_enforcement;
}

size_t mylite_ast_create_table_column_check_enforcement_start(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->check_enforcement_start;
}

size_t mylite_ast_create_table_column_check_enforcement_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->check_enforcement_end;
}

size_t mylite_ast_create_table_column_reference_start(const MyliteAst *ast,
                                                      size_t statement_index,
                                                      size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->reference_start;
}

size_t mylite_ast_create_table_column_reference_end(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->reference_end;
}

const MyliteAstNode *mylite_ast_create_table_column_type_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? NULL : column->type_node;
}

const MyliteAstNode *mylite_ast_create_table_column_options_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? NULL : column->options_node;
}

const MyliteAstNode *mylite_ast_create_table_column_default_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? NULL : column->default_node;
}

const MyliteAstNode *mylite_ast_create_table_column_default_value_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? NULL : column->default_value_node;
}

const MyliteAstNode *mylite_ast_create_table_column_on_update_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? NULL : column->on_update_node;
}

const MyliteAstNode *mylite_ast_create_table_column_on_update_value_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? NULL : column->on_update_value_node;
}

const MyliteAstNode *mylite_ast_create_table_column_generated_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? NULL : column->generated_node;
}

const MyliteAstNode *mylite_ast_create_table_column_generated_expression_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? NULL : column->generated_expression_node;
}

const MyliteAstNode *mylite_ast_create_table_column_generated_storage_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? NULL : column->generated_storage_node;
}

const MyliteAstNode *mylite_ast_create_table_column_comment_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? NULL : column->comment_node;
}

const MyliteAstNode *mylite_ast_create_table_column_check_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? NULL : column->check_node;
}

const MyliteAstNode *mylite_ast_create_table_column_check_expression_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? NULL : column->check_expression_node;
}

const MyliteAstNode *mylite_ast_create_table_column_check_enforcement_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? NULL : column->check_enforcement_node;
}

const MyliteAstNode *mylite_ast_create_table_column_reference_node(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? NULL : column->reference_node;
}

MyliteCreateTableColumnTypeFamily mylite_ast_create_table_column_type_family(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? MYLITE_CREATE_TABLE_COLUMN_TYPE_UNKNOWN
                        : column->type_family;
}

MyliteCreateTableColumnTypeKind mylite_ast_create_table_column_type_kind(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_UNKNOWN
                        : column->type_kind;
}

MyliteCreateTableColumnStorageClass
mylite_ast_create_table_column_storage_class(const MyliteAst *ast,
                                             size_t statement_index,
                                             size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? MYLITE_CREATE_TABLE_COLUMN_STORAGE_UNKNOWN
                        : column->storage_class;
}

unsigned int mylite_ast_create_table_column_flags(const MyliteAst *ast,
                                                  size_t statement_index,
                                                  size_t column_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  return column == NULL ? 0 : column->flags;
}

size_t mylite_ast_create_table_key_count(const MyliteAst *ast,
                                         size_t statement_index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? 0 : statement->create_table_key_count;
}

MyliteCreateTableKeyKind mylite_ast_create_table_key_kind(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? MYLITE_CREATE_TABLE_KEY_UNKNOWN : key->kind;
}

size_t mylite_ast_create_table_key_start(const MyliteAst *ast,
                                         size_t statement_index,
                                         size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->start;
}

size_t mylite_ast_create_table_key_end(const MyliteAst *ast,
                                       size_t statement_index,
                                       size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->end;
}

size_t mylite_ast_create_table_key_constraint_name_start(const MyliteAst *ast,
                                                         size_t statement_index,
                                                         size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->constraint_name_start;
}

size_t mylite_ast_create_table_key_constraint_name_end(const MyliteAst *ast,
                                                       size_t statement_index,
                                                       size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->constraint_name_end;
}

size_t mylite_ast_create_table_key_name_start(const MyliteAst *ast,
                                              size_t statement_index,
                                              size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->name_start;
}

size_t mylite_ast_create_table_key_name_end(const MyliteAst *ast,
                                            size_t statement_index,
                                            size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->name_end;
}

size_t mylite_ast_create_table_key_index_type_start(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->index_type_start;
}

size_t mylite_ast_create_table_key_index_type_end(const MyliteAst *ast,
                                                  size_t statement_index,
                                                  size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->index_type_end;
}

size_t mylite_ast_create_table_key_column_count(const MyliteAst *ast,
                                                size_t statement_index,
                                                size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->column_count;
}

MyliteCreateTableKeyPartKind mylite_ast_create_table_key_column_kind(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 0);
  return part == NULL ? MYLITE_CREATE_TABLE_KEY_PART_UNKNOWN : part->kind;
}

size_t mylite_ast_create_table_key_column_start(const MyliteAst *ast,
                                                size_t statement_index,
                                                size_t key_index,
                                                size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 0);
  return part == NULL ? 0 : part->start;
}

size_t mylite_ast_create_table_key_column_end(const MyliteAst *ast,
                                              size_t statement_index,
                                              size_t key_index,
                                              size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 0);
  return part == NULL ? 0 : part->end;
}

size_t mylite_ast_create_table_key_column_name_start(const MyliteAst *ast,
                                                     size_t statement_index,
                                                     size_t key_index,
                                                     size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 0);
  return part == NULL ? 0 : part->name_start;
}

size_t mylite_ast_create_table_key_column_name_end(const MyliteAst *ast,
                                                   size_t statement_index,
                                                   size_t key_index,
                                                   size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 0);
  return part == NULL ? 0 : part->name_end;
}

size_t mylite_ast_create_table_key_column_expression_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 0);
  return part == NULL ? 0 : part->expression_start;
}

size_t mylite_ast_create_table_key_column_expression_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 0);
  return part == NULL ? 0 : part->expression_end;
}

size_t mylite_ast_create_table_key_column_prefix_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 0);
  return part == NULL ? 0 : part->prefix_start;
}

size_t mylite_ast_create_table_key_column_prefix_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 0);
  return part == NULL ? 0 : part->prefix_end;
}

size_t mylite_ast_create_table_key_column_prefix_value_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 0);
  return part == NULL ? 0 : part->prefix_value_start;
}

size_t mylite_ast_create_table_key_column_prefix_value_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 0);
  return part == NULL ? 0 : part->prefix_value_end;
}

MyliteCreateTableKeyPartOrder mylite_ast_create_table_key_column_order(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 0);
  return part == NULL ? MYLITE_CREATE_TABLE_KEY_PART_ORDER_UNSPECIFIED
                      : part->order;
}

size_t mylite_ast_create_table_key_column_order_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 0);
  return part == NULL ? 0 : part->order_start;
}

size_t mylite_ast_create_table_key_column_order_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 0);
  return part == NULL ? 0 : part->order_end;
}

size_t mylite_ast_create_table_key_referenced_table_start(const MyliteAst *ast,
                                                          size_t statement_index,
                                                          size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->referenced_table_start;
}

size_t mylite_ast_create_table_key_referenced_table_end(const MyliteAst *ast,
                                                        size_t statement_index,
                                                        size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->referenced_table_end;
}

size_t mylite_ast_create_table_key_referenced_table_schema_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->referenced_table_schema_start;
}

size_t mylite_ast_create_table_key_referenced_table_schema_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->referenced_table_schema_end;
}

size_t mylite_ast_create_table_key_referenced_table_name_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->referenced_table_name_start;
}

size_t mylite_ast_create_table_key_referenced_table_name_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->referenced_table_name_end;
}

size_t mylite_ast_create_table_key_referenced_column_count(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->referenced_column_count;
}

MyliteCreateTableKeyPartKind mylite_ast_create_table_key_referenced_column_kind(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 1);
  return part == NULL ? MYLITE_CREATE_TABLE_KEY_PART_UNKNOWN : part->kind;
}

size_t mylite_ast_create_table_key_referenced_column_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 1);
  return part == NULL ? 0 : part->start;
}

size_t mylite_ast_create_table_key_referenced_column_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 1);
  return part == NULL ? 0 : part->end;
}

size_t mylite_ast_create_table_key_referenced_column_name_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 1);
  return part == NULL ? 0 : part->name_start;
}

size_t mylite_ast_create_table_key_referenced_column_name_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 1);
  return part == NULL ? 0 : part->name_end;
}

size_t mylite_ast_create_table_key_referenced_column_expression_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 1);
  return part == NULL ? 0 : part->expression_start;
}

size_t mylite_ast_create_table_key_referenced_column_expression_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 1);
  return part == NULL ? 0 : part->expression_end;
}

MyliteCreateTableKeyPartOrder
mylite_ast_create_table_key_referenced_column_order(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 1);
  return part == NULL ? MYLITE_CREATE_TABLE_KEY_PART_ORDER_UNSPECIFIED
                      : part->order;
}

MyliteCreateTableForeignMatchKind
mylite_ast_create_table_key_foreign_match_kind(const MyliteAst *ast,
                                               size_t statement_index,
                                               size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? MYLITE_CREATE_TABLE_FOREIGN_MATCH_UNSPECIFIED
                     : key->foreign_match_kind;
}

size_t mylite_ast_create_table_key_foreign_match_start(const MyliteAst *ast,
                                                       size_t statement_index,
                                                       size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->foreign_match_start;
}

size_t mylite_ast_create_table_key_foreign_match_end(const MyliteAst *ast,
                                                     size_t statement_index,
                                                     size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->foreign_match_end;
}

MyliteCreateTableForeignAction
mylite_ast_create_table_key_foreign_on_delete_action(const MyliteAst *ast,
                                                     size_t statement_index,
                                                     size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? MYLITE_CREATE_TABLE_FOREIGN_ACTION_UNSPECIFIED
                     : key->foreign_on_delete_action;
}

size_t mylite_ast_create_table_key_foreign_on_delete_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->foreign_on_delete_start;
}

size_t mylite_ast_create_table_key_foreign_on_delete_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->foreign_on_delete_end;
}

MyliteCreateTableForeignAction
mylite_ast_create_table_key_foreign_on_update_action(const MyliteAst *ast,
                                                     size_t statement_index,
                                                     size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? MYLITE_CREATE_TABLE_FOREIGN_ACTION_UNSPECIFIED
                     : key->foreign_on_update_action;
}

size_t mylite_ast_create_table_key_foreign_on_update_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->foreign_on_update_start;
}

size_t mylite_ast_create_table_key_foreign_on_update_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->foreign_on_update_end;
}

size_t mylite_ast_create_table_key_check_expression_start(const MyliteAst *ast,
                                                          size_t statement_index,
                                                          size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->check_expression_start;
}

size_t mylite_ast_create_table_key_check_expression_end(const MyliteAst *ast,
                                                        size_t statement_index,
                                                        size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->check_expression_end;
}

MyliteCreateTableCheckEnforcement
mylite_ast_create_table_key_check_enforcement(const MyliteAst *ast,
                                              size_t statement_index,
                                              size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_UNSPECIFIED
                     : key->check_enforcement;
}

size_t mylite_ast_create_table_key_check_enforcement_start(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->check_enforcement_start;
}

size_t mylite_ast_create_table_key_check_enforcement_end(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->check_enforcement_end;
}

size_t mylite_ast_create_table_key_option_count(const MyliteAst *ast,
                                                size_t statement_index,
                                                size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->option_count;
}

MyliteCreateTableKeyOptionKind mylite_ast_create_table_key_option_kind(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t option_index) {
  const MyliteAstCreateTableKeyOption *option =
      mylite_ast_create_table_key_option_at(ast, statement_index, key_index,
                                            option_index);
  return option == NULL ? MYLITE_CREATE_TABLE_KEY_OPTION_UNKNOWN
                        : option->kind;
}

size_t mylite_ast_create_table_key_option_start(const MyliteAst *ast,
                                                size_t statement_index,
                                                size_t key_index,
                                                size_t option_index) {
  const MyliteAstCreateTableKeyOption *option =
      mylite_ast_create_table_key_option_at(ast, statement_index, key_index,
                                            option_index);
  return option == NULL ? 0 : option->start;
}

size_t mylite_ast_create_table_key_option_end(const MyliteAst *ast,
                                              size_t statement_index,
                                              size_t key_index,
                                              size_t option_index) {
  const MyliteAstCreateTableKeyOption *option =
      mylite_ast_create_table_key_option_at(ast, statement_index, key_index,
                                            option_index);
  return option == NULL ? 0 : option->end;
}

size_t mylite_ast_create_table_key_option_name_start(const MyliteAst *ast,
                                                     size_t statement_index,
                                                     size_t key_index,
                                                     size_t option_index) {
  const MyliteAstCreateTableKeyOption *option =
      mylite_ast_create_table_key_option_at(ast, statement_index, key_index,
                                            option_index);
  return option == NULL ? 0 : option->name_start;
}

size_t mylite_ast_create_table_key_option_name_end(const MyliteAst *ast,
                                                   size_t statement_index,
                                                   size_t key_index,
                                                   size_t option_index) {
  const MyliteAstCreateTableKeyOption *option =
      mylite_ast_create_table_key_option_at(ast, statement_index, key_index,
                                            option_index);
  return option == NULL ? 0 : option->name_end;
}

size_t mylite_ast_create_table_key_option_value_start(const MyliteAst *ast,
                                                      size_t statement_index,
                                                      size_t key_index,
                                                      size_t option_index) {
  const MyliteAstCreateTableKeyOption *option =
      mylite_ast_create_table_key_option_at(ast, statement_index, key_index,
                                            option_index);
  return option == NULL ? 0 : option->value_start;
}

size_t mylite_ast_create_table_key_option_value_end(const MyliteAst *ast,
                                                    size_t statement_index,
                                                    size_t key_index,
                                                    size_t option_index) {
  const MyliteAstCreateTableKeyOption *option =
      mylite_ast_create_table_key_option_at(ast, statement_index, key_index,
                                            option_index);
  return option == NULL ? 0 : option->value_end;
}

size_t mylite_ast_create_table_option_count(const MyliteAst *ast,
                                            size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? 0 : statement->create_table_option_count;
}

MyliteCreateTableOptionKind mylite_ast_create_table_option_kind(
    const MyliteAst *ast, size_t statement_index, size_t option_index) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_option_at(ast, statement_index, option_index);
  return option == NULL ? MYLITE_CREATE_TABLE_OPTION_UNKNOWN : option->kind;
}

size_t mylite_ast_create_table_option_start(const MyliteAst *ast,
                                            size_t statement_index,
                                            size_t option_index) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_option_at(ast, statement_index, option_index);
  return option == NULL ? 0 : option->start;
}

size_t mylite_ast_create_table_option_end(const MyliteAst *ast,
                                          size_t statement_index,
                                          size_t option_index) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_option_at(ast, statement_index, option_index);
  return option == NULL ? 0 : option->end;
}

size_t mylite_ast_create_table_option_name_start(const MyliteAst *ast,
                                                 size_t statement_index,
                                                 size_t option_index) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_option_at(ast, statement_index, option_index);
  return option == NULL ? 0 : option->name_start;
}

size_t mylite_ast_create_table_option_name_end(const MyliteAst *ast,
                                               size_t statement_index,
                                               size_t option_index) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_option_at(ast, statement_index, option_index);
  return option == NULL ? 0 : option->name_end;
}

size_t mylite_ast_create_table_option_value_start(const MyliteAst *ast,
                                                  size_t statement_index,
                                                  size_t option_index) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_option_at(ast, statement_index, option_index);
  return option == NULL ? 0 : option->value_start;
}

size_t mylite_ast_create_table_option_value_end(const MyliteAst *ast,
                                                size_t statement_index,
                                                size_t option_index) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_option_at(ast, statement_index, option_index);
  return option == NULL ? 0 : option->value_end;
}

MyliteAstNodeKind mylite_ast_node_kind(const MyliteAstNode *node) {
  return node == NULL ? 0 : node->kind;
}

unsigned mylite_ast_node_rule_id(const MyliteAstNode *node) {
  return node == NULL ? 0 : node->rule_id;
}

const char *mylite_ast_node_symbol_name(const MyliteAstNode *node) {
  return node == NULL ? NULL : node->symbol_name;
}

int mylite_ast_node_token(const MyliteAstNode *node) {
  return node == NULL ? 0 : node->token;
}

size_t mylite_ast_node_start(const MyliteAstNode *node) {
  return node == NULL || !node->has_span ? 0 : node->start;
}

size_t mylite_ast_node_end(const MyliteAstNode *node) {
  return node == NULL || !node->has_span ? 0 : node->end;
}

size_t mylite_ast_node_child_count(const MyliteAstNode *node) {
  return node == NULL ? 0 : node->child_count;
}

const MyliteAstNode *mylite_ast_node_child(const MyliteAstNode *node,
                                           size_t index) {
  if (node == NULL || index >= node->child_count) {
    return NULL;
  }
  return node->children[index];
}

static MyliteParseStatus parse_sql_common(const char *sql, MyliteParseResult *result,
                                          MyliteAst **ast_out, int build_ast) {
  MyliteParseResult local_result;
  MyliteParseResult *target = result != NULL ? result : &local_result;
  memset(target, 0, sizeof(*target));
  target->status = MYLITE_PARSE_OK;
  if (ast_out != NULL) {
    *ast_out = NULL;
  }

  if (sql == NULL) {
    target->status = MYLITE_PARSE_LEX_ERROR;
    snprintf(target->message, sizeof(target->message), "SQL input is null");
    return target->status;
  }

  MyliteAst *ast = NULL;
  if (build_ast) {
    ast = mylite_ast_create(sql);
    if (ast == NULL) {
      target->status = MYLITE_PARSE_NO_MEMORY;
      snprintf(target->message, sizeof(target->message), "AST allocation failed");
      return target->status;
    }
  }

  MyliteParserState state;
  memset(&state, 0, sizeof(state));
  state.result = target;
  state.ast = ast;
  state.build_ast = build_ast;

  if (accepts_parser_shortcut(sql)) {
    if (build_ast) {
      mylite_parser_state_recognized_root(&state, strlen(sql));
      if (target->status != MYLITE_PARSE_OK) {
        mylite_ast_free(ast);
        return target->status;
      }
      *ast_out = ast;
    }
    return target->status;
  }

  void *parser = MyliteTidbParseAlloc(malloc);
  if (parser == NULL) {
    target->status = MYLITE_PARSE_NO_MEMORY;
    snprintf(target->message, sizeof(target->message), "parser allocation failed");
    mylite_ast_free(ast);
    return target->status;
  }

  MyliteLexer lexer;
  mylite_lexer_init(&lexer, sql);

  for (;;) {
    MyliteToken token = mylite_lexer_next(&lexer);
    if (lexer.lex_error) {
      target->status = MYLITE_PARSE_LEX_ERROR;
      target->offset = lexer.lex_error_offset;
      snprintf(target->message, sizeof(target->message), "invalid token at byte %zu",
               lexer.lex_error_offset);
      break;
    }

    state.token_offset = token.offset;
    MyliteAstNode *token_node = NULL;
    if (build_ast && token.type != 0) {
      token_node =
          mylite_parser_state_token(&state, token.type, token.offset, token.length);
      if (target->status != MYLITE_PARSE_OK) {
        break;
      }
    }
    MyliteTidbParse(parser, token.type, token_node, &state);
    if (target->status != MYLITE_PARSE_OK || token.type == 0) {
      break;
    }
  }

  MyliteTidbParseFree(parser, free);

  if (target->status == MYLITE_PARSE_SYNTAX_ERROR &&
      accepts_parser_fallback(sql)) {
    memset(target, 0, sizeof(*target));
    target->status = MYLITE_PARSE_OK;
    state.accepted = 1;
    if (build_ast) {
      mylite_ast_free(ast);
      ast = mylite_ast_create(sql);
      if (ast == NULL) {
        target->status = MYLITE_PARSE_NO_MEMORY;
        snprintf(target->message, sizeof(target->message), "AST allocation failed");
        return target->status;
      }
      state.ast = ast;
      state.root = NULL;
      mylite_parser_state_recognized_root(&state, strlen(sql));
    }
  }

  if (target->status == MYLITE_PARSE_OK && !state.accepted) {
    target->status = MYLITE_PARSE_SYNTAX_ERROR;
    snprintf(target->message, sizeof(target->message), "parser did not accept input");
  }

  if (target->status == MYLITE_PARSE_OK && build_ast) {
    if (state.root == NULL) {
      target->status = MYLITE_PARSE_SYNTAX_ERROR;
      snprintf(target->message, sizeof(target->message), "parser did not build AST");
    } else if (!mylite_ast_finalize_statements(ast)) {
      target->status = MYLITE_PARSE_NO_MEMORY;
      snprintf(target->message, sizeof(target->message),
               "AST statement allocation failed");
    } else {
      *ast_out = ast;
      return target->status;
    }
  }

  if (target->status != MYLITE_PARSE_OK || !build_ast) {
    mylite_ast_free(ast);
  }
  return target->status;
}

static int accepts_parser_shortcut(const char *sql) {
  return accepts_mysqltest_harness_statement(sql) ||
         accepts_parenthesized_create_table_set_statement(sql) ||
         accepts_alter_user_default_role_statement(sql) ||
         accepts_create_user_default_role_statement(sql) ||
         accepts_flush_option_list_statement(sql) ||
         accepts_select_where_having_without_from(sql) ||
         accepts_select_group_having_without_from(sql) ||
         accepts_select_from_dual_group_having(sql) ||
         accepts_select_not_like_pipes_statement(sql) ||
         accepts_analyze_histogram_table_list(sql) ||
         accepts_mysql_resource_group_statement(sql);
}

static int accepts_parser_fallback(const char *sql) {
  return accepts_create_procedure_with_characteristics(sql) ||
         accepts_insert_alias_on_duplicate(sql) ||
         accepts_set_select_into_before_lock(sql);
}

static MyliteAst *mylite_ast_create(const char *source) {
  MyliteAst *ast = calloc(1, sizeof(*ast));
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

static void *mylite_ast_alloc(MyliteAst *ast, size_t size) {
  enum { DEFAULT_CHUNK_SIZE = 8192 };
  if (ast == NULL || size == 0) {
    return NULL;
  }

  size_t alignment = sizeof(void *);
  size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
  MyliteAstChunk *chunk = ast->chunks;
  if (chunk == NULL || chunk->capacity - chunk->used < aligned_size) {
    size_t capacity = aligned_size > DEFAULT_CHUNK_SIZE ? aligned_size : DEFAULT_CHUNK_SIZE;
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

static MyliteAstNode *mylite_ast_make_node(MyliteParserState *state,
                                           MyliteAstNodeKind kind,
                                           unsigned rule_id,
                                           const char *symbol_name, int token,
                                           size_t child_count,
                                           MyliteAstNode *const *children) {
  if (state == NULL || !state->build_ast) {
    return NULL;
  }
  if (state->result != NULL && state->result->status != MYLITE_PARSE_OK) {
    return NULL;
  }

  MyliteAstNode *node = mylite_ast_alloc(state->ast, sizeof(*node));
  if (node == NULL) {
    mylite_parser_state_no_memory(state);
    return NULL;
  }

  node->kind = kind;
  node->rule_id = rule_id;
  node->symbol_name = symbol_name;
  node->token = token;
  node->child_count = child_count;
  if (child_count > 0) {
    node->children = mylite_ast_alloc(state->ast, child_count * sizeof(*node->children));
    if (node->children == NULL) {
      mylite_parser_state_no_memory(state);
      return NULL;
    }
    memcpy(node->children, children, child_count * sizeof(*node->children));
    mylite_ast_set_node_span_from_children(node);
  }
  state->ast->node_count++;
  return node;
}

static void mylite_ast_set_node_span_from_children(MyliteAstNode *node) {
  if (node == NULL) {
    return;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    MyliteAstNode *child = node->children[i];
    if (child == NULL || !child->has_span) {
      continue;
    }
    if (!node->has_span) {
      node->start = child->start;
      node->has_span = 1;
    }
    node->end = child->end;
  }
}

static int mylite_ast_finalize_statements(MyliteAst *ast) {
  if (ast == NULL || ast->root == NULL) {
    return 1;
  }

  size_t count = mylite_ast_count_top_level_statements(ast->root);
  if (count == 0) {
    count = 1;
  }

  ast->statements = mylite_ast_alloc(ast, count * sizeof(*ast->statements));
  if (ast->statements == NULL) {
    return 0;
  }
  ast->statement_count = count;

  size_t index = 0;
  if (!mylite_ast_fill_top_level_statements(ast, ast->root, &index)) {
    return 0;
  }
  if (index == 0) {
    const MyliteAstNode *payload = ast->root;
    if (!mylite_ast_init_statement(&ast->statements[0], ast, payload)) {
      return 0;
    }
  }
  return 1;
}

static size_t mylite_ast_count_top_level_statements(const MyliteAstNode *node) {
  if (node == NULL || node->symbol_name == NULL) {
    return 0;
  }
  if (strcmp(node->symbol_name, "nt_statement") == 0) {
    return 1;
  }
  if (strcmp(node->symbol_name, "input") != 0 &&
      strcmp(node->symbol_name, "nt_start") != 0 &&
      strcmp(node->symbol_name, "nt_statement_list") != 0) {
    return 0;
  }

  size_t count = 0;
  for (size_t i = 0; i < node->child_count; i++) {
    count += mylite_ast_count_top_level_statements(node->children[i]);
  }
  return count;
}

static int mylite_ast_fill_top_level_statements(MyliteAst *ast,
                                                const MyliteAstNode *node,
                                                size_t *index) {
  if (ast == NULL || node == NULL || index == NULL || node->symbol_name == NULL ||
      *index >= ast->statement_count) {
    return 1;
  }

  if (strcmp(node->symbol_name, "nt_statement") == 0) {
    if (!mylite_ast_init_statement(&ast->statements[*index], ast, node)) {
      return 0;
    }
    (*index)++;
    return 1;
  }

  if (strcmp(node->symbol_name, "input") != 0 &&
      strcmp(node->symbol_name, "nt_start") != 0 &&
      strcmp(node->symbol_name, "nt_statement_list") != 0) {
    return 1;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    if (!mylite_ast_fill_top_level_statements(ast, node->children[i], index)) {
      return 0;
    }
  }
  return 1;
}

static int mylite_ast_init_statement(MyliteAstStatement *statement, MyliteAst *ast,
                                     const MyliteAstNode *node) {
  const MyliteAstNode *payload = mylite_ast_statement_payload(node);
  if (payload == NULL) {
    payload = node;
  }
  statement->node = node;
  statement->symbol_name = payload == NULL ? NULL : payload->symbol_name;
  statement->kind = mylite_ast_classify_statement(statement->symbol_name);
  statement->start = mylite_ast_node_start(node);
  statement->end = mylite_ast_node_end(node);
  if (!mylite_ast_set_statement_target(statement, ast, payload)) {
    return 0;
  }
  return mylite_ast_set_statement_details(ast, statement, payload);
}

static const MyliteAstNode *mylite_ast_statement_payload(const MyliteAstNode *node) {
  if (node == NULL || node->child_count == 0) {
    return node;
  }
  return node->children[0];
}

static MyliteStatementKind mylite_ast_classify_statement(const char *symbol_name) {
  static const char *const select_symbols[] = {
      "nt_select_stmt",
      "nt_select_stmt_with_clause",
      "nt_set_opr_stmt",
      "nt_set_opr_stmt_with_limit_order_by",
      "nt_set_opr_stmt_wout_limit_order_by",
      "nt_sub_select",
  };
  static const char *const transaction_symbols[] = {
      "nt_begin_transaction_stmt",
      "nt_commit_stmt",
      "nt_rollback_stmt",
      "nt_savepoint_stmt",
      "nt_release_savepoint_stmt",
  };
  static const char *const lock_symbols[] = {
      "nt_lock_tables_stmt",
      "nt_unlock_tables_stmt",
      "nt_mysql_lock_instance_stmt",
  };
  static const char *const utility_symbols[] = {
      "nt_admin_stmt",
      "nt_analyze_table_stmt",
      "nt_binlog_stmt",
      "nt_check_table_stmt",
      "nt_checksum_table_stmt",
      "nt_flush_stmt",
      "nt_grant_stmt",
      "nt_grant_role_stmt",
      "nt_handler_stmt",
      "nt_help_stmt",
      "nt_kill_stmt",
      "nt_load_data_stmt",
      "nt_load_stats_stmt",
      "nt_load_xml_stmt",
      "nt_optimize_table_stmt",
      "nt_repair_table_stmt",
      "nt_reset_stmt",
      "nt_revoke_stmt",
      "nt_revoke_role_stmt",
      "nt_trace_stmt",
      "nt_use_stmt",
      "nt_mysql_xa_stmt",
  };

  if (symbol_name == NULL) {
    return MYLITE_STATEMENT_UNKNOWN;
  }
  if (strcmp(symbol_name, "nt_empty_stmt") == 0) {
    return MYLITE_STATEMENT_EMPTY;
  }
  if (symbol_is_one_of(symbol_name, select_symbols,
                       sizeof(select_symbols) / sizeof(select_symbols[0]))) {
    return MYLITE_STATEMENT_SELECT;
  }
  if (strcmp(symbol_name, "nt_insert_into_stmt") == 0) {
    return MYLITE_STATEMENT_INSERT;
  }
  if (strcmp(symbol_name, "nt_update_stmt") == 0) {
    return MYLITE_STATEMENT_UPDATE;
  }
  if (strcmp(symbol_name, "nt_delete_from_stmt") == 0) {
    return MYLITE_STATEMENT_DELETE;
  }
  if (strcmp(symbol_name, "nt_replace_into_stmt") == 0) {
    return MYLITE_STATEMENT_REPLACE;
  }
  if (symbol_has_prefix(symbol_name, "nt_create_") ||
      symbol_has_prefix(symbol_name, "nt_mysql_create_")) {
    return MYLITE_STATEMENT_CREATE;
  }
  if (symbol_has_prefix(symbol_name, "nt_alter_") ||
      symbol_has_prefix(symbol_name, "nt_mysql_alter_")) {
    return MYLITE_STATEMENT_ALTER;
  }
  if (symbol_has_prefix(symbol_name, "nt_drop_") ||
      symbol_has_prefix(symbol_name, "nt_mysql_drop_")) {
    return MYLITE_STATEMENT_DROP;
  }
  if (strcmp(symbol_name, "nt_rename_table_stmt") == 0) {
    return MYLITE_STATEMENT_RENAME;
  }
  if (strcmp(symbol_name, "nt_truncate_table_stmt") == 0) {
    return MYLITE_STATEMENT_TRUNCATE;
  }
  if (strcmp(symbol_name, "nt_set_stmt") == 0 ||
      strcmp(symbol_name, "nt_set_default_role_stmt") == 0 ||
      strcmp(symbol_name, "nt_set_role_stmt") == 0) {
    return MYLITE_STATEMENT_SET;
  }
  if (strcmp(symbol_name, "nt_show_stmt") == 0) {
    return MYLITE_STATEMENT_SHOW;
  }
  if (strcmp(symbol_name, "nt_explain_stmt") == 0) {
    return MYLITE_STATEMENT_EXPLAIN;
  }
  if (strcmp(symbol_name, "nt_do_stmt") == 0) {
    return MYLITE_STATEMENT_DO;
  }
  if (strcmp(symbol_name, "nt_call_stmt") == 0) {
    return MYLITE_STATEMENT_CALL;
  }
  if (strcmp(symbol_name, "nt_prepare_stmt") == 0) {
    return MYLITE_STATEMENT_PREPARE;
  }
  if (strcmp(symbol_name, "nt_execute_stmt") == 0) {
    return MYLITE_STATEMENT_EXECUTE;
  }
  if (strcmp(symbol_name, "nt_deallocate_stmt") == 0) {
    return MYLITE_STATEMENT_DEALLOCATE;
  }
  if (symbol_is_one_of(symbol_name, transaction_symbols,
                       sizeof(transaction_symbols) / sizeof(transaction_symbols[0]))) {
    return MYLITE_STATEMENT_TRANSACTION;
  }
  if (symbol_is_one_of(symbol_name, lock_symbols,
                       sizeof(lock_symbols) / sizeof(lock_symbols[0]))) {
    return MYLITE_STATEMENT_LOCK;
  }
  if (symbol_is_one_of(symbol_name, utility_symbols,
                       sizeof(utility_symbols) / sizeof(utility_symbols[0])) ||
      symbol_has_prefix(symbol_name, "nt_mysql_")) {
    return MYLITE_STATEMENT_UTILITY;
  }
  return MYLITE_STATEMENT_UNKNOWN;
}

static int mylite_ast_set_statement_target(MyliteAstStatement *statement,
                                           MyliteAst *ast,
                                           const MyliteAstNode *payload) {
  if (statement == NULL) {
    return 1;
  }

  statement->target_kind =
      mylite_ast_target_kind_for_statement(statement->kind, statement->symbol_name);
  if (statement->target_kind == MYLITE_STATEMENT_TARGET_NONE) {
    return 1;
  }
  if (statement->target_kind == MYLITE_STATEMENT_TARGET_UNKNOWN) {
    return 1;
  }
  if (!mylite_ast_collect_statement_targets(ast, statement, payload)) {
    return 0;
  }
  if (statement->target_count == 0) {
    statement->target_kind = MYLITE_STATEMENT_TARGET_UNKNOWN;
    return 1;
  }
  mylite_ast_mirror_first_statement_target(statement);
  return 1;
}

static int mylite_ast_set_statement_details(MyliteAst *ast,
                                            MyliteAstStatement *statement,
                                            const MyliteAstNode *payload) {
  if (statement == NULL || statement->symbol_name == NULL) {
    return 1;
  }
  if (strcmp(statement->symbol_name, "nt_create_table_stmt") == 0) {
    return mylite_ast_collect_create_table_columns(ast, statement, payload) &&
           mylite_ast_collect_create_table_keys(ast, statement, payload) &&
           mylite_ast_collect_create_table_options(ast, statement, payload);
  }
  return 1;
}

static MyliteStatementTargetKind mylite_ast_target_kind_for_statement(
    MyliteStatementKind kind, const char *symbol_name) {
  switch (kind) {
    case MYLITE_STATEMENT_INSERT:
    case MYLITE_STATEMENT_UPDATE:
    case MYLITE_STATEMENT_DELETE:
    case MYLITE_STATEMENT_REPLACE:
    case MYLITE_STATEMENT_RENAME:
    case MYLITE_STATEMENT_TRUNCATE:
      return MYLITE_STATEMENT_TARGET_TABLE;
    case MYLITE_STATEMENT_CREATE:
    case MYLITE_STATEMENT_ALTER:
    case MYLITE_STATEMENT_DROP:
      if (symbol_name == NULL) {
        return MYLITE_STATEMENT_TARGET_UNKNOWN;
      }
      if (strstr(symbol_name, "database") != NULL ||
          strstr(symbol_name, "schema") != NULL) {
        return MYLITE_STATEMENT_TARGET_DATABASE;
      }
      if (strstr(symbol_name, "table") != NULL || strstr(symbol_name, "index") != NULL) {
        return MYLITE_STATEMENT_TARGET_TABLE;
      }
      if (strstr(symbol_name, "view") != NULL) {
        return MYLITE_STATEMENT_TARGET_VIEW;
      }
      if (strstr(symbol_name, "procedure") != NULL ||
          strstr(symbol_name, "function") != NULL ||
          strstr(symbol_name, "routine") != NULL) {
        return MYLITE_STATEMENT_TARGET_ROUTINE;
      }
      if (strstr(symbol_name, "user") != NULL || strstr(symbol_name, "role") != NULL) {
        return MYLITE_STATEMENT_TARGET_ACCOUNT;
      }
      return MYLITE_STATEMENT_TARGET_UNKNOWN;
    case MYLITE_STATEMENT_SET:
      return MYLITE_STATEMENT_TARGET_VARIABLE;
    default:
      return MYLITE_STATEMENT_TARGET_NONE;
  }
}

static int mylite_ast_collect_statement_targets(MyliteAst *ast,
                                                MyliteAstStatement *statement,
                                                const MyliteAstNode *payload) {
  MyliteStatementTargetKind kind = statement->target_kind;
  size_t before = statement->target_count;
  const MyliteAstNode *target = NULL;

  if (kind == MYLITE_STATEMENT_TARGET_TABLE) {
    if (statement->symbol_name != NULL &&
        strcmp(statement->symbol_name, "nt_rename_table_stmt") == 0) {
      return mylite_ast_collect_rename_targets(ast, statement, payload);
    }
    if (statement->kind == MYLITE_STATEMENT_DELETE) {
      if (!mylite_ast_collect_symbol_targets(ast, statement, payload,
                                             "nt_table_name_opt_wild", kind,
                                             MYLITE_STATEMENT_TARGET_ROLE_PRIMARY)) {
        return 0;
      }
      if (statement->target_count > before) {
        return 1;
      }
      return mylite_ast_collect_symbol_targets(ast, statement, payload,
                                               "nt_table_name", kind,
                                               MYLITE_STATEMENT_TARGET_ROLE_PRIMARY);
    }
    if (statement->kind == MYLITE_STATEMENT_UPDATE) {
      const MyliteAstNode *target_scope =
          mylite_ast_find_first_symbol(payload, "nt_update_stmt_no_with");
      if (target_scope == NULL) {
        target_scope = payload;
      }
      target = mylite_ast_find_first_symbol(target_scope, "nt_table_ref");
      if (target != NULL) {
        if (!mylite_ast_collect_symbol_targets(ast, statement, target,
                                               "nt_table_name", kind,
                                               MYLITE_STATEMENT_TARGET_ROLE_PRIMARY)) {
          return 0;
        }
        if (statement->target_count > before) {
          return 1;
        }
      }
    }
    if (statement->symbol_name != NULL &&
        strcmp(statement->symbol_name, "nt_drop_table_stmt") == 0) {
      return mylite_ast_collect_symbol_targets(ast, statement, payload,
                                               "nt_table_name", kind,
                                               MYLITE_STATEMENT_TARGET_ROLE_PRIMARY);
    }
    target = mylite_ast_find_first_symbol(payload, "nt_table_name");
  } else if (kind == MYLITE_STATEMENT_TARGET_DATABASE) {
    target = mylite_ast_find_first_symbol(payload, "nt_db_name");
    if (target == NULL) {
      target = mylite_ast_find_first_symbol(payload, "nt_table_name");
    }
  } else if (kind == MYLITE_STATEMENT_TARGET_VIEW) {
    target = mylite_ast_find_first_symbol(payload, "nt_view_name");
    if (target == NULL) {
      target = mylite_ast_find_first_symbol(payload, "nt_table_name");
    }
  } else if (kind == MYLITE_STATEMENT_TARGET_ROUTINE) {
    target = mylite_ast_find_first_symbol(payload, "nt_table_name");
  } else if (kind == MYLITE_STATEMENT_TARGET_ACCOUNT) {
    target = mylite_ast_find_first_symbol(payload, "nt_username");
  } else if (kind == MYLITE_STATEMENT_TARGET_VARIABLE) {
    target = mylite_ast_find_first_token(payload, MYLITE_TOK_SINGLE_AT_IDENTIFIER);
    if (target == NULL) {
      target = mylite_ast_find_first_token(payload, MYLITE_TOK_DOUBLE_AT_IDENTIFIER);
    }
    if (target == NULL) {
      target = mylite_ast_find_first_symbol(payload, "nt_variable_name");
    }
  }

  if (target == NULL) {
    return 1;
  }
  return mylite_ast_append_statement_target(ast, statement, kind,
                                            MYLITE_STATEMENT_TARGET_ROLE_PRIMARY,
                                            target);
}

static int mylite_ast_collect_rename_targets(MyliteAst *ast,
                                             MyliteAstStatement *statement,
                                             const MyliteAstNode *node) {
  if (node == NULL) {
    return 1;
  }
  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_table_to_table") == 0) {
    if (node->child_count >= 3 &&
        (!mylite_ast_append_statement_target(
             ast, statement, MYLITE_STATEMENT_TARGET_TABLE,
             MYLITE_STATEMENT_TARGET_ROLE_SOURCE, node->children[0]) ||
         !mylite_ast_append_statement_target(
             ast, statement, MYLITE_STATEMENT_TARGET_TABLE,
             MYLITE_STATEMENT_TARGET_ROLE_DESTINATION, node->children[2]))) {
      return 0;
    }
    return 1;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    if (!mylite_ast_collect_rename_targets(ast, statement, node->children[i])) {
      return 0;
    }
  }
  return 1;
}

static int mylite_ast_collect_symbol_targets(MyliteAst *ast,
                                             MyliteAstStatement *statement,
                                             const MyliteAstNode *node,
                                             const char *symbol_name,
                                             MyliteStatementTargetKind kind,
                                             MyliteStatementTargetRole role) {
  if (node == NULL) {
    return 1;
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, symbol_name) == 0) {
    return mylite_ast_append_statement_target(ast, statement, kind, role, node);
  }
  if (mylite_ast_is_nested_target_boundary(node)) {
    return 1;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    if (!mylite_ast_collect_symbol_targets(ast, statement, node->children[i],
                                           symbol_name, kind, role)) {
      return 0;
    }
  }
  return 1;
}

static int mylite_ast_append_statement_target(MyliteAst *ast,
                                              MyliteAstStatement *statement,
                                              MyliteStatementTargetKind kind,
                                              MyliteStatementTargetRole role,
                                              const MyliteAstNode *target) {
  if (ast == NULL || statement == NULL || target == NULL) {
    return 1;
  }

  size_t next_count = statement->target_count + 1;
  MyliteAstStatementTarget *targets =
      mylite_ast_alloc(ast, next_count * sizeof(*targets));
  if (targets == NULL) {
    return 0;
  }
  if (statement->target_count > 0) {
    memcpy(targets, statement->targets,
           statement->target_count * sizeof(*targets));
  }
  mylite_ast_fill_statement_target(&targets[statement->target_count], kind, role,
                                   target);
  statement->targets = targets;
  statement->target_count = next_count;
  return 1;
}

static void mylite_ast_fill_statement_target(MyliteAstStatementTarget *target,
                                             MyliteStatementTargetKind kind,
                                             MyliteStatementTargetRole role,
                                             const MyliteAstNode *node) {
  target->kind = kind;
  target->role = role;
  target->start = mylite_ast_node_start(node);
  target->end = mylite_ast_node_end(node);
  target->name_start = target->start;
  target->name_end = target->end;
  if (kind == MYLITE_STATEMENT_TARGET_TABLE ||
      kind == MYLITE_STATEMENT_TARGET_DATABASE ||
      kind == MYLITE_STATEMENT_TARGET_VIEW ||
      kind == MYLITE_STATEMENT_TARGET_ROUTINE) {
    mylite_ast_set_table_name_parts(target, node);
  }
}

static void mylite_ast_mirror_first_statement_target(MyliteAstStatement *statement) {
  if (statement == NULL || statement->target_count == 0) {
    return;
  }

  const MyliteAstStatementTarget *target = &statement->targets[0];
  statement->target_kind = target->kind;
  statement->target_start = target->start;
  statement->target_end = target->end;
  statement->target_schema_start = target->schema_start;
  statement->target_schema_end = target->schema_end;
  statement->target_name_start = target->name_start;
  statement->target_name_end = target->name_end;
}

static int mylite_ast_collect_create_table_columns(MyliteAst *ast,
                                                   MyliteAstStatement *statement,
                                                   const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL || payload == NULL) {
    return 1;
  }

  size_t count = mylite_ast_count_create_table_columns(payload);
  if (count == 0) {
    return 1;
  }

  statement->create_table_columns =
      mylite_ast_alloc(ast, count * sizeof(*statement->create_table_columns));
  if (statement->create_table_columns == NULL) {
    return 0;
  }
  statement->create_table_column_count = count;

  size_t index = 0;
  return mylite_ast_fill_create_table_columns(ast, statement, payload, &index) &&
         index == count;
}

static size_t mylite_ast_count_create_table_columns(const MyliteAstNode *node) {
  if (node == NULL) {
    return 0;
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, "nt_column_def") == 0) {
    return 1;
  }
  if (mylite_ast_is_nested_target_boundary(node)) {
    return 0;
  }

  size_t count = 0;
  for (size_t i = 0; i < node->child_count; i++) {
    count += mylite_ast_count_create_table_columns(node->children[i]);
  }
  return count;
}

static int mylite_ast_fill_create_table_columns(MyliteAst *ast,
                                                MyliteAstStatement *statement,
                                                const MyliteAstNode *node,
                                                size_t *index) {
  if (statement == NULL || node == NULL || index == NULL ||
      *index >= statement->create_table_column_count) {
    return 1;
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, "nt_column_def") == 0) {
    if (!mylite_ast_fill_create_table_column(
            ast, &statement->create_table_columns[*index], node)) {
      return 0;
    }
    (*index)++;
    return 1;
  }
  if (mylite_ast_is_nested_target_boundary(node)) {
    return 1;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    if (!mylite_ast_fill_create_table_columns(ast, statement, node->children[i],
                                              index)) {
      return 0;
    }
  }
  return 1;
}

static int mylite_ast_fill_create_table_column(
    MyliteAst *ast, MyliteAstCreateTableColumn *column,
    const MyliteAstNode *node) {
  column->start = mylite_ast_node_start(node);
  column->end = mylite_ast_node_end(node);

  const MyliteAstNode *name = mylite_ast_find_first_symbol(node, "nt_column_name");
  if (name != NULL) {
    column->name_start = mylite_ast_node_start(name);
    column->name_end = mylite_ast_node_end(name);
    if (!mylite_ast_set_create_table_column_name_value(ast, column)) {
      return 0;
    }
  }

  const MyliteAstNode *type = mylite_ast_find_first_symbol(node, "nt_type");
  if (type != NULL) {
    column->type_family = mylite_ast_classify_column_type(type);
    column->type_kind = mylite_ast_classify_column_type_kind(type);
    column->storage_class =
        mylite_ast_classify_column_storage_class(column->type_kind);
    column->type_node = type;
    column->type_start = mylite_ast_node_start(type);
    column->type_end = mylite_ast_node_end(type);
    mylite_ast_set_create_table_column_type_details(column, type);
    mylite_ast_set_create_table_column_type_attribute_details(column, type);
    if (!mylite_ast_set_create_table_column_type_parameter_values(ast, column)) {
      return 0;
    }
  }

  const MyliteAstNode *options =
      mylite_ast_find_first_symbol(node, "nt_column_option_list_opt");
  if (options != NULL && options->has_span) {
    column->options_node = options;
    column->options_start = mylite_ast_node_start(options);
    column->options_end = mylite_ast_node_end(options);
  }
  mylite_ast_set_create_table_column_option_details(column, options);
  column->flags = mylite_ast_collect_column_flags(type, options);
  return 1;
}

static int mylite_ast_set_create_table_column_name_value(
    MyliteAst *ast, MyliteAstCreateTableColumn *column) {
  if (ast == NULL || column == NULL || column->name_start >= column->name_end ||
      column->name_end > ast->source_length) {
    return 1;
  }

  return mylite_ast_decode_identifier(ast, column->name_start,
                                      column->name_end, &column->name_value,
                                      &column->name_value_length);
}

static void mylite_ast_set_create_table_column_type_details(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *type) {
  if (column == NULL || type == NULL) {
    return;
  }

  const MyliteAstNode *type_body =
      mylite_ast_find_first_direct_spanned_child(type);
  const MyliteAstNode *type_name =
      mylite_ast_find_first_direct_spanned_child(type_body);
  if (type_name == NULL) {
    return;
  }

  column->type_name_start = mylite_ast_node_start(type_name);
  column->type_name_end = mylite_ast_node_end(type_name);
  mylite_ast_set_create_table_column_type_tail_details(column, type_body,
                                                       type_name);
}

static void mylite_ast_set_create_table_column_type_tail_details(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *type_body,
    const MyliteAstNode *type_name) {
  if (column == NULL || type_body == NULL || type_name == NULL) {
    return;
  }

  int in_parenthesized_parameters = 0;
  for (size_t i = 0; i < type_body->child_count; i++) {
    const MyliteAstNode *child = type_body->children[i];
    if (child == NULL || child == type_name || !child->has_span ||
        child->end <= column->type_name_end) {
      continue;
    }

    if (mylite_ast_is_column_type_name_continuation(child)) {
      mylite_ast_extend_span(&column->type_name_start, &column->type_name_end,
                             child);
      continue;
    }

    if (mylite_ast_is_column_type_attribute_child(child)) {
      mylite_ast_extend_span(&column->type_attributes_start,
                             &column->type_attributes_end, child);
      in_parenthesized_parameters = 0;
      continue;
    }

    if (child->kind == MYLITE_AST_NODE_TOKEN &&
        child->token == MYLITE_TOK_LPAREN) {
      in_parenthesized_parameters = 1;
    }
    if (in_parenthesized_parameters ||
        mylite_ast_is_column_type_parameter_child(child)) {
      mylite_ast_extend_span(&column->type_parameters_start,
                             &column->type_parameters_end, child);
    }
    if (child->kind == MYLITE_AST_NODE_TOKEN &&
        child->token == MYLITE_TOK_RPAREN) {
      in_parenthesized_parameters = 0;
    }
  }
}

static void mylite_ast_set_create_table_column_type_attribute_details(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *type) {
  if (column == NULL || type == NULL) {
    return;
  }

  mylite_ast_set_create_table_column_type_unsigned_detail(column, type);
  mylite_ast_set_create_table_column_type_zerofill_detail(column, type);
  mylite_ast_set_create_table_column_type_charset_details(column, type);
}

static void mylite_ast_set_create_table_column_type_unsigned_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *type) {
  const MyliteAstNode *node = mylite_ast_find_first_token_after(
      type, MYLITE_TOK_UNSIGNED, column == NULL ? 0 : column->type_name_end);
  if (column == NULL || node == NULL) {
    return;
  }

  column->type_unsigned_start = mylite_ast_node_start(node);
  column->type_unsigned_end = mylite_ast_node_end(node);
}

static void mylite_ast_set_create_table_column_type_zerofill_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *type) {
  const MyliteAstNode *node = mylite_ast_find_first_token_after(
      type, MYLITE_TOK_ZEROFILL, column == NULL ? 0 : column->type_name_end);
  if (column == NULL || node == NULL) {
    return;
  }

  column->type_zerofill_start = mylite_ast_node_start(node);
  column->type_zerofill_end = mylite_ast_node_end(node);
}

static void mylite_ast_set_create_table_column_type_charset_details(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *type) {
  if (column == NULL || type == NULL) {
    return;
  }

  const MyliteAstNode *attribute = mylite_ast_find_first_symbol_after(
      type, "nt_opt_binary", column->type_name_end);
  if (attribute == NULL) {
    attribute = mylite_ast_find_first_symbol_after(
        type, "nt_opt_charset_with_opt_binary", column->type_name_end);
  }
  if (attribute == NULL || !attribute->has_span) {
    return;
  }

  mylite_ast_set_create_table_column_type_binary_detail(column, attribute);
  mylite_ast_set_create_table_column_type_charset_detail(column, attribute);
  mylite_ast_set_create_table_column_type_collation_detail(column, attribute);
}

static void mylite_ast_set_create_table_column_type_binary_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *attribute) {
  if (column == NULL || attribute == NULL) {
    return;
  }

  const MyliteAstNode *binary =
      mylite_ast_direct_child_token(attribute, 0, MYLITE_TOK_BINARY_TYPE);
  if (binary == NULL) {
    const MyliteAstNode *bin_mod =
        mylite_ast_find_first_symbol_after(attribute, "nt_opt_bin_mod",
                                           column->type_name_end);
    if (bin_mod != NULL && bin_mod->has_span) {
      binary = bin_mod;
    }
  }
  if (binary == NULL || !binary->has_span) {
    return;
  }

  column->type_binary_start = mylite_ast_node_start(binary);
  column->type_binary_end = mylite_ast_node_end(binary);
}

static void mylite_ast_set_create_table_column_type_charset_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *attribute) {
  if (column == NULL || attribute == NULL) {
    return;
  }

  const MyliteAstNode *charset_kw =
      mylite_ast_find_first_symbol_after(attribute, "nt_charset_kw",
                                         column->type_name_end);
  const MyliteAstNode *charset_name =
      mylite_ast_find_first_symbol_after(attribute, "nt_charset_name",
                                         column->type_name_end);
  if (charset_kw != NULL && charset_kw->has_span && charset_name != NULL &&
      charset_name->has_span) {
    column->type_charset_start = mylite_ast_node_start(charset_kw);
    column->type_charset_end = mylite_ast_node_end(charset_name);
    column->type_charset_value_start = mylite_ast_node_start(charset_name);
    column->type_charset_value_end = mylite_ast_node_end(charset_name);
    return;
  }

  const MyliteAstNode *shortcut =
      mylite_ast_direct_child_token(attribute, 0, MYLITE_TOK_ASCII);
  if (shortcut == NULL) {
    shortcut =
        mylite_ast_direct_child_token(attribute, 0, MYLITE_TOK_UNICODE_SYM);
  }
  if (shortcut == NULL) {
    shortcut = mylite_ast_direct_child_token(attribute, 0, MYLITE_TOK_BYTE_TYPE);
  }
  if (shortcut == NULL || !shortcut->has_span) {
    return;
  }

  column->type_charset_start = mylite_ast_node_start(shortcut);
  column->type_charset_end = mylite_ast_node_end(shortcut);
  column->type_charset_value_start = mylite_ast_node_start(shortcut);
  column->type_charset_value_end = mylite_ast_node_end(shortcut);
}

static void mylite_ast_set_create_table_column_type_collation_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *attribute) {
  if (column == NULL || attribute == NULL) {
    return;
  }

  const MyliteAstNode *collate = mylite_ast_find_first_token_after(
      attribute, MYLITE_TOK_COLLATE, column->type_name_end);
  const MyliteAstNode *collation_name =
      mylite_ast_find_first_symbol_after(attribute, "nt_collation_name",
                                         column->type_name_end);
  if (collate == NULL || collation_name == NULL || !collation_name->has_span) {
    return;
  }

  column->type_collation_start = mylite_ast_node_start(collate);
  column->type_collation_end = mylite_ast_node_end(collation_name);
  column->type_collation_value_start = mylite_ast_node_start(collation_name);
  column->type_collation_value_end = mylite_ast_node_end(collation_name);
}

static int mylite_ast_set_create_table_column_type_parameter_values(
    MyliteAst *ast, MyliteAstCreateTableColumn *column) {
  if (ast == NULL || column == NULL) {
    return 1;
  }

  if (column->storage_class == MYLITE_CREATE_TABLE_COLUMN_STORAGE_ENUM ||
      column->storage_class == MYLITE_CREATE_TABLE_COLUMN_STORAGE_SET) {
    return mylite_ast_set_create_table_column_type_elements(ast, column);
  }

  if (ast->source == NULL ||
      column->type_parameters_start >= column->type_parameters_end ||
      column->type_parameters_end > ast->source_length) {
    return 1;
  }

  column->type_numeric_parameter_count = mylite_ast_parse_unsigned_parameters(
      ast->source, column->type_parameters_start, column->type_parameters_end,
      column->type_numeric_parameters,
      sizeof(column->type_numeric_parameters) /
          sizeof(column->type_numeric_parameters[0]));
  mylite_ast_set_create_table_column_type_shape(column);
  return 1;
}

static size_t mylite_ast_parse_unsigned_parameters(
    const char *source, size_t start, size_t end, unsigned long long *values,
    size_t value_capacity) {
  if (source == NULL || values == NULL || value_capacity == 0 || start >= end) {
    return 0;
  }

  size_t count = 0;
  for (size_t offset = start; offset < end;) {
    unsigned char ch = (unsigned char)source[offset];
    if (ch >= '0' && ch <= '9') {
      unsigned long long value = 0;
      do {
        unsigned int digit = (unsigned int)(source[offset] - '0');
        if (value > (ULLONG_MAX - digit) / 10) {
          value = ULLONG_MAX;
        } else {
          value = value * 10 + digit;
        }
        offset++;
      } while (offset < end && source[offset] >= '0' && source[offset] <= '9');
      if (count < value_capacity) {
        values[count] = value;
      }
      count++;
      continue;
    }
    offset++;
  }
  return count;
}

static int mylite_ast_set_create_table_column_type_elements(
    MyliteAst *ast, MyliteAstCreateTableColumn *column) {
  if (ast == NULL || column == NULL || column->type_node == NULL) {
    return 1;
  }

  size_t count =
      mylite_ast_count_create_table_column_type_elements(column->type_node);
  if (count == 0) {
    return 1;
  }
  if (count > (size_t)-1 / sizeof(*column->type_elements)) {
    return 0;
  }

  MyliteAstCreateTableTypeElement *elements =
      mylite_ast_alloc(ast, count * sizeof(*elements));
  if (elements == NULL) {
    return 0;
  }

  size_t index = 0;
  mylite_ast_fill_create_table_column_type_elements(elements,
                                                    column->type_node, &index);
  if (index != count) {
    return 0;
  }
  column->type_elements = elements;
  column->type_element_count = count;
  return mylite_ast_set_create_table_column_type_element_values(ast, column);
}

static size_t mylite_ast_count_create_table_column_type_elements(
    const MyliteAstNode *node) {
  if (node == NULL) {
    return 0;
  }
  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_text_string") == 0) {
    return node->has_span ? 1 : 0;
  }

  size_t count = 0;
  for (size_t i = 0; i < node->child_count; i++) {
    count +=
        mylite_ast_count_create_table_column_type_elements(node->children[i]);
  }
  return count;
}

static void mylite_ast_fill_create_table_column_type_elements(
    MyliteAstCreateTableTypeElement *elements, const MyliteAstNode *node,
    size_t *index) {
  if (elements == NULL || node == NULL || index == NULL) {
    return;
  }
  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_text_string") == 0) {
    if (node->has_span) {
      elements[*index].start = mylite_ast_node_start(node);
      elements[*index].end = mylite_ast_node_end(node);
      elements[*index].token = mylite_ast_first_token(node);
      (*index)++;
    }
    return;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    mylite_ast_fill_create_table_column_type_elements(elements,
                                                      node->children[i], index);
  }
}

static int mylite_ast_set_create_table_column_type_element_values(
    MyliteAst *ast, MyliteAstCreateTableColumn *column) {
  if (ast == NULL || column == NULL) {
    return 1;
  }

  for (size_t i = 0; i < column->type_element_count; i++) {
    MyliteAstCreateTableTypeElement *element = &column->type_elements[i];
    if (element->token != MYLITE_TOK_STRING_LIT) {
      continue;
    }
    if (!mylite_ast_decode_sql_string_literal(ast, element->start,
                                              element->end, &element->value,
                                              &element->value_length)) {
      return 0;
    }
  }
  return 1;
}

static int mylite_ast_decode_sql_string_literal(MyliteAst *ast, size_t start,
                                                size_t end,
                                                const char **value,
                                                size_t *value_length) {
  if (ast == NULL || ast->source == NULL || value == NULL ||
      value_length == NULL || start >= end || end > ast->source_length) {
    return 1;
  }

  char quote = ast->source[start];
  if ((quote != '\'' && quote != '"') || end - start < 2 ||
      ast->source[end - 1] != quote) {
    return 1;
  }

  size_t length =
      mylite_ast_decoded_sql_string_literal_length(ast->source, start, end);
  char *decoded = mylite_ast_alloc(ast, length + 1);
  if (decoded == NULL) {
    return 0;
  }
  size_t written =
      mylite_ast_write_decoded_sql_string_literal(decoded, ast->source, start,
                                                  end);
  decoded[written] = '\0';
  *value = decoded;
  *value_length = written;
  return 1;
}

static size_t mylite_ast_decoded_sql_string_literal_length(const char *source,
                                                           size_t start,
                                                           size_t end) {
  size_t length = 0;
  char quote = source[start];
  for (size_t offset = start + 1; offset + 1 < end;) {
    unsigned char ch = (unsigned char)source[offset++];
    if (ch == '\\' && offset + 1 < end) {
      offset++;
      length++;
      continue;
    }
    if (ch == (unsigned char)quote && offset + 1 < end &&
        source[offset] == quote) {
      offset++;
    }
    length++;
  }
  return length;
}

static size_t mylite_ast_write_decoded_sql_string_literal(char *target,
                                                          const char *source,
                                                          size_t start,
                                                          size_t end) {
  size_t written = 0;
  char quote = source[start];
  for (size_t offset = start + 1; offset + 1 < end;) {
    unsigned char ch = (unsigned char)source[offset++];
    if (ch == '\\' && offset + 1 < end) {
      target[written++] =
          (char)mylite_ast_sql_string_escape_value(
              (unsigned char)source[offset++]);
      continue;
    }
    if (ch == (unsigned char)quote && offset + 1 < end &&
        source[offset] == quote) {
      offset++;
    }
    target[written++] = (char)ch;
  }
  return written;
}

static int mylite_ast_sql_string_escape_value(int ch) {
  switch (ch) {
    case '0':
      return '\0';
    case '\'':
      return '\'';
    case '"':
      return '"';
    case 'b':
      return '\b';
    case 'n':
      return '\n';
    case 'r':
      return '\r';
    case 't':
      return '\t';
    case 'Z':
      return 26;
    case '\\':
      return '\\';
    case '%':
      return '%';
    case '_':
      return '_';
    default:
      return ch;
  }
}

static int mylite_ast_decode_identifier(MyliteAst *ast, size_t start,
                                        size_t end, const char **value,
                                        size_t *value_length) {
  if (ast == NULL || ast->source == NULL || value == NULL ||
      value_length == NULL || start >= end || end > ast->source_length) {
    return 1;
  }

  char quote = ast->source[start];
  if ((quote != '`' && quote != '"') || end - start < 2 ||
      ast->source[end - 1] != quote) {
    *value = ast->source + start;
    *value_length = end - start;
    return 1;
  }

  size_t length = mylite_ast_decoded_identifier_length(ast->source, start, end);
  char *decoded = mylite_ast_alloc(ast, length + 1);
  if (decoded == NULL) {
    return 0;
  }
  size_t written =
      mylite_ast_write_decoded_identifier(decoded, ast->source, start, end);
  decoded[written] = '\0';
  *value = decoded;
  *value_length = written;
  return 1;
}

static size_t mylite_ast_decoded_identifier_length(const char *source,
                                                   size_t start, size_t end) {
  size_t length = 0;
  char quote = source[start];
  for (size_t offset = start + 1; offset + 1 < end;) {
    unsigned char ch = (unsigned char)source[offset++];
    if (ch == (unsigned char)quote && offset + 1 < end &&
        source[offset] == quote) {
      offset++;
    }
    length++;
  }
  return length;
}

static size_t mylite_ast_write_decoded_identifier(char *target,
                                                  const char *source,
                                                  size_t start, size_t end) {
  size_t written = 0;
  char quote = source[start];
  for (size_t offset = start + 1; offset + 1 < end;) {
    unsigned char ch = (unsigned char)source[offset++];
    if (ch == (unsigned char)quote && offset + 1 < end &&
        source[offset] == quote) {
      offset++;
    }
    target[written++] = (char)ch;
  }
  return written;
}

static void mylite_ast_set_create_table_column_type_shape(
    MyliteAstCreateTableColumn *column) {
  if (column == NULL || column->type_numeric_parameter_count == 0) {
    return;
  }

  switch (column->type_kind) {
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DECIMAL:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_FLOAT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_REAL:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DOUBLE:
      mylite_ast_set_create_table_column_type_precision_scale(column);
      break;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TIME:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DATETIME:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TIMESTAMP:
      mylite_ast_set_create_table_column_type_fsp(column);
      break;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TINYINT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_SMALLINT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MEDIUMINT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_INT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BIGINT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BOOL:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BIT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_CHAR:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VARCHAR:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_NCHAR:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_NVARCHAR:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BINARY:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VARBINARY:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BLOB:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TEXT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_YEAR:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VECTOR:
      mylite_ast_set_create_table_column_type_length(column, 0);
      break;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_UNKNOWN:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TINYBLOB:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MEDIUMBLOB:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONGBLOB:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TINYTEXT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MEDIUMTEXT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONGTEXT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_ENUM:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_SET:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_JSON:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG_VARCHAR:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG_VARBINARY:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DATE:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_GEOMETRY:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_POINT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LINESTRING:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_POLYGON:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MULTIPOINT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MULTILINESTRING:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MULTIPOLYGON:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_GEOMETRYCOLLECTION:
      break;
  }
}

static void mylite_ast_set_create_table_column_type_length(
    MyliteAstCreateTableColumn *column, size_t parameter_index) {
  if (column == NULL ||
      parameter_index >= column->type_numeric_parameter_count ||
      parameter_index >= sizeof(column->type_numeric_parameters) /
                             sizeof(column->type_numeric_parameters[0])) {
    return;
  }

  column->type_length = column->type_numeric_parameters[parameter_index];
  column->type_shape_flags |=
      MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_LENGTH;
}

static void mylite_ast_set_create_table_column_type_precision_scale(
    MyliteAstCreateTableColumn *column) {
  if (column == NULL || column->type_numeric_parameter_count == 0) {
    return;
  }

  column->type_precision = column->type_numeric_parameters[0];
  column->type_shape_flags |=
      MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_PRECISION;
  if (column->type_numeric_parameter_count > 1) {
    column->type_scale = column->type_numeric_parameters[1];
    column->type_shape_flags |=
        MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_SCALE;
  }
}

static void mylite_ast_set_create_table_column_type_fsp(
    MyliteAstCreateTableColumn *column) {
  if (column == NULL || column->type_numeric_parameter_count == 0) {
    return;
  }

  column->type_fractional_seconds_precision = column->type_numeric_parameters[0];
  column->type_shape_flags |= MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_FSP;
}

static int mylite_ast_is_column_type_name_continuation(const MyliteAstNode *node) {
  if (node == NULL || node->symbol_name == NULL) {
    return 0;
  }

  static const char *const symbols[] = {
      "nt_varchar",
  };
  return symbol_is_one_of(node->symbol_name, symbols,
                          sizeof(symbols) / sizeof(symbols[0]));
}

static int mylite_ast_is_column_type_parameter_child(const MyliteAstNode *node) {
  if (node == NULL || node->symbol_name == NULL) {
    return 0;
  }

  static const char *const symbols[] = {
      "nt_field_len",
      "nt_float_opt",
      "nt_opt_field_len",
      "nt_precision",
  };
  return symbol_is_one_of(node->symbol_name, symbols,
                          sizeof(symbols) / sizeof(symbols[0]));
}

static int mylite_ast_is_column_type_attribute_child(const MyliteAstNode *node) {
  if (node == NULL || node->symbol_name == NULL) {
    return 0;
  }

  static const char *const symbols[] = {
      "nt_field_opts",
      "nt_opt_binary",
      "nt_opt_charset_with_opt_binary",
  };
  return symbol_is_one_of(node->symbol_name, symbols,
                          sizeof(symbols) / sizeof(symbols[0]));
}

static const MyliteAstNode *mylite_ast_find_first_symbol_after(
    const MyliteAstNode *node, const char *symbol_name, size_t min_start) {
  if (node == NULL || symbol_name == NULL) {
    return NULL;
  }
  if (node->symbol_name != NULL && node->has_span && node->start >= min_start &&
      strcmp(node->symbol_name, symbol_name) == 0) {
    return node;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *found =
        mylite_ast_find_first_symbol_after(node->children[i], symbol_name,
                                           min_start);
    if (found != NULL) {
      return found;
    }
  }
  return NULL;
}

static const MyliteAstNode *mylite_ast_find_first_token_after(
    const MyliteAstNode *node, int token, size_t min_start) {
  if (node == NULL) {
    return NULL;
  }
  if (node->kind == MYLITE_AST_NODE_TOKEN && node->token == token &&
      node->has_span && node->start >= min_start) {
    return node;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *found =
        mylite_ast_find_first_token_after(node->children[i], token, min_start);
    if (found != NULL) {
      return found;
    }
  }
  return NULL;
}

static const MyliteAstNode *mylite_ast_direct_child_token(
    const MyliteAstNode *node, size_t child_index, int token) {
  if (node == NULL || child_index >= node->child_count) {
    return NULL;
  }
  const MyliteAstNode *child = node->children[child_index];
  if (child == NULL || child->kind != MYLITE_AST_NODE_TOKEN ||
      child->token != token) {
    return NULL;
  }
  return child;
}

static void mylite_ast_set_create_table_column_option_details(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *options) {
  if (column == NULL || options == NULL) {
    return;
  }
  if (options->symbol_name != NULL &&
      strcmp(options->symbol_name, "nt_column_option") == 0) {
    mylite_ast_set_create_table_column_option_detail(column, options);
    return;
  }

  for (size_t i = 0; i < options->child_count; i++) {
    mylite_ast_set_create_table_column_option_details(column,
                                                      options->children[i]);
  }
}

static void mylite_ast_set_create_table_column_option_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *option) {
  if (column == NULL || option == NULL) {
    return;
  }
  if (mylite_ast_find_first_token(option, MYLITE_TOK_DEFAULT_KWD) != NULL) {
    mylite_ast_set_create_table_column_default_detail(column, option);
  } else if (mylite_ast_find_first_token(option, MYLITE_TOK_ON) != NULL &&
             mylite_ast_find_first_token(option, MYLITE_TOK_UPDATE) != NULL) {
    mylite_ast_set_create_table_column_on_update_detail(column, option);
  } else if (mylite_ast_find_first_symbol(option, "nt_generated_always") != NULL) {
    mylite_ast_set_create_table_column_generated_detail(column, option);
  } else if (mylite_ast_find_first_token(option, MYLITE_TOK_CHECK) != NULL) {
    mylite_ast_set_create_table_column_check_detail(column, option);
  } else if (mylite_ast_find_first_token(option, MYLITE_TOK_COMMENT) != NULL) {
    mylite_ast_set_create_table_column_comment_detail(column, option);
  } else if (mylite_ast_find_first_symbol(option, "nt_refer_def") != NULL) {
    mylite_ast_set_create_table_column_reference_detail(column, option);
  }
}

static void mylite_ast_set_create_table_column_default_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *option) {
  column->default_node = option;
  column->default_start = mylite_ast_node_start(option);
  column->default_end = mylite_ast_node_end(option);

  const MyliteAstNode *value =
      mylite_ast_find_first_symbol(option, "nt_default_value_expr");
  if (value != NULL && value->has_span) {
    column->default_value_node = value;
    column->default_value_start = mylite_ast_node_start(value);
    column->default_value_end = mylite_ast_node_end(value);
  }
}

static void mylite_ast_set_create_table_column_on_update_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *option) {
  column->on_update_node = option;
  column->on_update_start = mylite_ast_node_start(option);
  column->on_update_end = mylite_ast_node_end(option);

  const MyliteAstNode *value =
      mylite_ast_find_first_symbol(option, "nt_now_sym_option_fraction");
  if (value != NULL && value->has_span) {
    column->on_update_value_node = value;
    column->on_update_value_start = mylite_ast_node_start(value);
    column->on_update_value_end = mylite_ast_node_end(value);
  }
}

static void mylite_ast_set_create_table_column_generated_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *option) {
  column->generated_node = option;
  column->generated_start = mylite_ast_node_start(option);
  column->generated_end = mylite_ast_node_end(option);

  const MyliteAstNode *expression =
      mylite_ast_find_first_symbol(option, "nt_expression");
  if (expression != NULL && expression->has_span) {
    column->generated_expression_node = expression;
    column->generated_expression_start = mylite_ast_node_start(expression);
    column->generated_expression_end = mylite_ast_node_end(expression);
  }

  const MyliteAstNode *storage =
      mylite_ast_find_first_symbol(option, "nt_virtual_or_stored");
  if (storage != NULL && storage->has_span) {
    column->generated_storage_node = storage;
    column->generated_storage_start = mylite_ast_node_start(storage);
    column->generated_storage_end = mylite_ast_node_end(storage);
  }
}

static void mylite_ast_set_create_table_column_comment_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *option) {
  column->comment_node = option;
  column->comment_start = mylite_ast_node_start(option);
  column->comment_end = mylite_ast_node_end(option);

  const MyliteAstNode *comment =
      mylite_ast_find_first_token(option, MYLITE_TOK_COMMENT);
  if (comment != NULL) {
    mylite_ast_collect_value_span_after(option, mylite_ast_node_end(comment),
                                        &column->comment_value_start,
                                        &column->comment_value_end);
  }
}

static void mylite_ast_set_create_table_column_check_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *option) {
  column->check_node = option;
  column->check_start = mylite_ast_node_start(option);
  column->check_end = mylite_ast_node_end(option);

  const MyliteAstNode *expression =
      mylite_ast_find_first_symbol(option, "nt_expression");
  if (expression != NULL && expression->has_span) {
    column->check_expression_node = expression;
    column->check_expression_start = mylite_ast_node_start(expression);
    column->check_expression_end = mylite_ast_node_end(expression);
  }

  const MyliteAstNode *enforcement =
      mylite_ast_find_first_symbol(option, "nt_enforced_or_not_or_not_null_opt");
  if (enforcement == NULL || !enforcement->has_span) {
    enforcement = mylite_ast_find_first_symbol(option, "nt_enforced_or_not_opt");
  }
  if (enforcement == NULL || !enforcement->has_span) {
    enforcement = mylite_ast_find_first_symbol(option, "nt_enforced_or_not");
  }
  if (enforcement != NULL && enforcement->has_span) {
    column->check_enforcement =
        mylite_ast_classify_check_enforcement(enforcement);
    column->check_enforcement_node = enforcement;
    column->check_enforcement_start = mylite_ast_node_start(enforcement);
    column->check_enforcement_end = mylite_ast_node_end(enforcement);
  }
}

static void mylite_ast_set_create_table_column_reference_detail(
    MyliteAstCreateTableColumn *column, const MyliteAstNode *option) {
  const MyliteAstNode *reference =
      mylite_ast_find_first_symbol(option, "nt_refer_def");
  if (reference != NULL && reference->has_span) {
    column->reference_node = reference;
    column->reference_start = mylite_ast_node_start(reference);
    column->reference_end = mylite_ast_node_end(reference);
  } else {
    column->reference_node = option;
    column->reference_start = mylite_ast_node_start(option);
    column->reference_end = mylite_ast_node_end(option);
  }
}

static MyliteCreateTableColumnTypeFamily mylite_ast_classify_column_type(
    const MyliteAstNode *type) {
  if (type == NULL) {
    return MYLITE_CREATE_TABLE_COLUMN_TYPE_UNKNOWN;
  }
  if (mylite_ast_find_first_symbol(type, "nt_numeric_type") != NULL) {
    return MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC;
  }
  if (mylite_ast_find_first_symbol(type, "nt_time_type") != NULL) {
    return MYLITE_CREATE_TABLE_COLUMN_TYPE_TEMPORAL;
  }
  if (mylite_ast_find_first_symbol(type, "nt_date_and_time_type") != NULL) {
    return MYLITE_CREATE_TABLE_COLUMN_TYPE_TEMPORAL;
  }
  int first_token = mylite_ast_first_token(type);
  if (first_token == MYLITE_TOK_DATE_TYPE ||
      first_token == MYLITE_TOK_DATETIME_TYPE ||
      first_token == MYLITE_TOK_TIME_TYPE ||
      first_token == MYLITE_TOK_TIMESTAMP_TYPE ||
      first_token == MYLITE_TOK_YEAR_TYPE) {
    return MYLITE_CREATE_TABLE_COLUMN_TYPE_TEMPORAL;
  }
  if (mylite_ast_find_first_symbol(type, "nt_mysql_spatial_type") != NULL) {
    return MYLITE_CREATE_TABLE_COLUMN_TYPE_SPATIAL;
  }
  if (mylite_ast_find_first_symbol(type, "nt_string_type") != NULL) {
    if (first_token == MYLITE_TOK_JSON_TYPE) {
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_JSON;
    }
    if (first_token == MYLITE_TOK_ENUM) {
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_ENUM;
    }
    if (first_token == MYLITE_TOK_SET) {
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_SET;
    }
    return MYLITE_CREATE_TABLE_COLUMN_TYPE_STRING;
  }
  return MYLITE_CREATE_TABLE_COLUMN_TYPE_UNKNOWN;
}

static MyliteCreateTableColumnTypeKind mylite_ast_classify_column_type_kind(
    const MyliteAstNode *type) {
  if (type == NULL) {
    return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_UNKNOWN;
  }

  int first_token = mylite_ast_first_token(type);
  if (first_token == MYLITE_TOK_LONG) {
    if (mylite_ast_find_first_token(type, MYLITE_TOK_VARBINARY_TYPE) != NULL) {
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG_VARBINARY;
    }
    if (mylite_ast_find_first_symbol(type, "nt_varchar") != NULL) {
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG_VARCHAR;
    }
    return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG;
  }
  if (first_token == MYLITE_TOK_NATIONAL) {
    if (mylite_ast_find_first_symbol(type, "nt_n_varchar") != NULL) {
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_NVARCHAR;
    }
    return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_NCHAR;
  }

  switch (first_token) {
    case MYLITE_TOK_TINY_INT_TYPE:
    case MYLITE_TOK_INT1_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TINYINT;
    case MYLITE_TOK_SMALL_INT_TYPE:
    case MYLITE_TOK_INT2_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_SMALLINT;
    case MYLITE_TOK_MEDIUM_INT_TYPE:
    case MYLITE_TOK_MIDDLE_INT_TYPE:
    case MYLITE_TOK_INT3_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MEDIUMINT;
    case MYLITE_TOK_INT_TYPE:
    case MYLITE_TOK_INT4_TYPE:
    case MYLITE_TOK_INTEGER_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_INT;
    case MYLITE_TOK_BIG_INT_TYPE:
    case MYLITE_TOK_INT8_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BIGINT;
    case MYLITE_TOK_BOOL_TYPE:
    case MYLITE_TOK_BOOLEAN_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BOOL;
    case MYLITE_TOK_DECIMAL_TYPE:
    case MYLITE_TOK_NUMERIC_TYPE:
    case MYLITE_TOK_FIXED:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DECIMAL;
    case MYLITE_TOK_FLOAT_TYPE:
    case MYLITE_TOK_FLOAT4_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_FLOAT;
    case MYLITE_TOK_REAL_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_REAL;
    case MYLITE_TOK_DOUBLE_TYPE:
    case MYLITE_TOK_FLOAT8_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DOUBLE;
    case MYLITE_TOK_BIT_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BIT;
    case MYLITE_TOK_CHAR_TYPE:
    case MYLITE_TOK_CHARACTER:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_CHAR;
    case MYLITE_TOK_VARCHAR_TYPE:
    case MYLITE_TOK_VARCHARACTER:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VARCHAR;
    case MYLITE_TOK_NCHAR_TYPE:
      if (mylite_ast_find_first_symbol(type, "nt_n_varchar") != NULL) {
        return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_NVARCHAR;
      }
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_NCHAR;
    case MYLITE_TOK_NVARCHAR_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_NVARCHAR;
    case MYLITE_TOK_BINARY_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BINARY;
    case MYLITE_TOK_VARBINARY_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VARBINARY;
    case MYLITE_TOK_TINYBLOB_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TINYBLOB;
    case MYLITE_TOK_BLOB_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BLOB;
    case MYLITE_TOK_MEDIUMBLOB_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MEDIUMBLOB;
    case MYLITE_TOK_LONGBLOB_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONGBLOB;
    case MYLITE_TOK_TINYTEXT_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TINYTEXT;
    case MYLITE_TOK_TEXT_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TEXT;
    case MYLITE_TOK_MEDIUMTEXT_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MEDIUMTEXT;
    case MYLITE_TOK_LONGTEXT_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONGTEXT;
    case MYLITE_TOK_ENUM:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_ENUM;
    case MYLITE_TOK_SET:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_SET;
    case MYLITE_TOK_JSON_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_JSON;
    case MYLITE_TOK_VECTOR_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VECTOR;
    case MYLITE_TOK_DATE_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DATE;
    case MYLITE_TOK_DATETIME_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DATETIME;
    case MYLITE_TOK_TIMESTAMP_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TIMESTAMP;
    case MYLITE_TOK_TIME_TYPE:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TIME;
    case MYLITE_TOK_YEAR_TYPE:
    case MYLITE_TOK_SQL_TSI_YEAR:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_YEAR;
    case MYLITE_TOK_GEOMETRY:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_GEOMETRY;
    case MYLITE_TOK_POINT:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_POINT;
    case MYLITE_TOK_LINESTRING:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LINESTRING;
    case MYLITE_TOK_POLYGON:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_POLYGON;
    case MYLITE_TOK_MULTIPOINT:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MULTIPOINT;
    case MYLITE_TOK_MULTILINESTRING:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MULTILINESTRING;
    case MYLITE_TOK_MULTIPOLYGON:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MULTIPOLYGON;
    case MYLITE_TOK_GEOMETRYCOLLECTION:
      return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_GEOMETRYCOLLECTION;
  }
  return MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_UNKNOWN;
}

static MyliteCreateTableColumnStorageClass
mylite_ast_classify_column_storage_class(MyliteCreateTableColumnTypeKind kind) {
  switch (kind) {
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TINYINT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_SMALLINT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MEDIUMINT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_INT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BIGINT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BOOL:
      return MYLITE_CREATE_TABLE_COLUMN_STORAGE_INTEGER;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DECIMAL:
      return MYLITE_CREATE_TABLE_COLUMN_STORAGE_DECIMAL;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_FLOAT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_REAL:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DOUBLE:
      return MYLITE_CREATE_TABLE_COLUMN_STORAGE_FLOAT;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BIT:
      return MYLITE_CREATE_TABLE_COLUMN_STORAGE_BIT;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_CHAR:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_NCHAR:
      return MYLITE_CREATE_TABLE_COLUMN_STORAGE_FIXED_STRING;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VARCHAR:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_NVARCHAR:
      return MYLITE_CREATE_TABLE_COLUMN_STORAGE_VARIABLE_STRING;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BINARY:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VARBINARY:
      return MYLITE_CREATE_TABLE_COLUMN_STORAGE_BINARY_STRING;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TINYBLOB:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BLOB:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MEDIUMBLOB:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONGBLOB:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG_VARBINARY:
      return MYLITE_CREATE_TABLE_COLUMN_STORAGE_BLOB;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TINYTEXT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TEXT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MEDIUMTEXT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONGTEXT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG_VARCHAR:
      return MYLITE_CREATE_TABLE_COLUMN_STORAGE_TEXT;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_ENUM:
      return MYLITE_CREATE_TABLE_COLUMN_STORAGE_ENUM;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_SET:
      return MYLITE_CREATE_TABLE_COLUMN_STORAGE_SET;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_JSON:
      return MYLITE_CREATE_TABLE_COLUMN_STORAGE_JSON;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VECTOR:
      return MYLITE_CREATE_TABLE_COLUMN_STORAGE_VECTOR;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DATE:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DATETIME:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TIMESTAMP:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TIME:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_YEAR:
      return MYLITE_CREATE_TABLE_COLUMN_STORAGE_TEMPORAL;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_GEOMETRY:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_POINT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LINESTRING:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_POLYGON:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MULTIPOINT:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MULTILINESTRING:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MULTIPOLYGON:
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_GEOMETRYCOLLECTION:
      return MYLITE_CREATE_TABLE_COLUMN_STORAGE_SPATIAL;
    case MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_UNKNOWN:
      return MYLITE_CREATE_TABLE_COLUMN_STORAGE_UNKNOWN;
  }
  return MYLITE_CREATE_TABLE_COLUMN_STORAGE_UNKNOWN;
}

static unsigned int mylite_ast_collect_column_flags(const MyliteAstNode *type,
                                                    const MyliteAstNode *options) {
  return mylite_ast_collect_column_type_flags(type) |
         mylite_ast_collect_column_option_flags(options);
}

static unsigned int mylite_ast_collect_column_type_flags(const MyliteAstNode *type) {
  unsigned int flags = 0;
  if (mylite_ast_find_first_token(type, MYLITE_TOK_UNSIGNED) != NULL) {
    flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_UNSIGNED;
  }
  if (mylite_ast_find_first_token(type, MYLITE_TOK_ZEROFILL) != NULL) {
    flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_ZEROFILL;
  }
  if (mylite_ast_find_first_symbol(type, "nt_charset_kw") != NULL ||
      mylite_ast_find_first_token(type, MYLITE_TOK_CHARSET_KWD) != NULL) {
    flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_CHARACTER_SET;
  }
  if (mylite_ast_find_first_token(type, MYLITE_TOK_COLLATE) != NULL) {
    flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_COLLATE;
  }
  return flags;
}

static unsigned int mylite_ast_collect_column_option_flags(
    const MyliteAstNode *node) {
  if (node == NULL) {
    return 0;
  }
  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_column_option") == 0) {
    unsigned int flags = 0;
    int has_not = mylite_ast_find_first_token(node, MYLITE_TOK_NOT) != NULL;
    int has_null = mylite_ast_find_first_token(node, MYLITE_TOK_NULL) != NULL;
    int has_default =
        mylite_ast_find_first_token(node, MYLITE_TOK_DEFAULT_KWD) != NULL;
    if (has_not && has_null) {
      flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_NOT_NULL;
    } else if (has_null && !has_default) {
      flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_NULL;
    }
    if (has_default) {
      flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_DEFAULT;
    }
    if (mylite_ast_find_first_token(node, MYLITE_TOK_AUTO_INCREMENT) != NULL) {
      flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_AUTO_INCREMENT;
    }
    if (mylite_ast_find_first_token(node, MYLITE_TOK_PRIMARY) != NULL) {
      flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_PRIMARY_KEY;
    }
    if (mylite_ast_find_first_token(node, MYLITE_TOK_UNIQUE) != NULL) {
      flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_UNIQUE_KEY;
    }
    if (mylite_ast_find_first_token(node, MYLITE_TOK_COMMENT) != NULL) {
      flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_COMMENT;
    }
    if (mylite_ast_find_first_token(node, MYLITE_TOK_GENERATED) != NULL ||
        mylite_ast_find_first_symbol(node, "nt_generated_always") != NULL) {
      flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_GENERATED;
    }
    if (mylite_ast_find_first_token(node, MYLITE_TOK_VIRTUAL) != NULL) {
      flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_VIRTUAL;
    }
    if (mylite_ast_find_first_token(node, MYLITE_TOK_STORED) != NULL) {
      flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_STORED;
    }
    if (mylite_ast_find_first_token(node, MYLITE_TOK_ON) != NULL &&
        mylite_ast_find_first_token(node, MYLITE_TOK_UPDATE) != NULL) {
      flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_ON_UPDATE;
    }
    if (mylite_ast_find_first_token(node, MYLITE_TOK_REFERENCES) != NULL) {
      flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_REFERENCES;
    }
    if (mylite_ast_find_first_token(node, MYLITE_TOK_CHECK) != NULL) {
      flags |= MYLITE_CREATE_TABLE_COLUMN_FLAG_CHECK;
    }
    return flags;
  }

  unsigned int flags = 0;
  for (size_t i = 0; i < node->child_count; i++) {
    flags |= mylite_ast_collect_column_option_flags(node->children[i]);
  }
  return flags;
}

static int mylite_ast_collect_create_table_keys(MyliteAst *ast,
                                                MyliteAstStatement *statement,
                                                const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL || payload == NULL) {
    return 1;
  }

  size_t count = mylite_ast_count_create_table_keys(payload);
  if (count == 0) {
    return 1;
  }

  statement->create_table_keys =
      mylite_ast_alloc(ast, count * sizeof(*statement->create_table_keys));
  if (statement->create_table_keys == NULL) {
    return 0;
  }
  statement->create_table_key_count = count;

  size_t index = 0;
  int ok = 1;
  mylite_ast_fill_create_table_keys(ast, statement, payload, &index, &ok);
  return ok && index == count;
}

static size_t mylite_ast_count_create_table_keys(const MyliteAstNode *node) {
  if (node == NULL) {
    return 0;
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, "nt_constraint") == 0) {
    const MyliteAstNode *elem = mylite_ast_find_constraint_elem(node);
    return mylite_ast_classify_create_table_key(elem) ==
                   MYLITE_CREATE_TABLE_KEY_UNKNOWN
               ? 0
               : 1;
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, "nt_column_def") == 0) {
    return 0;
  }
  if (mylite_ast_is_nested_target_boundary(node)) {
    return 0;
  }

  size_t count = 0;
  for (size_t i = 0; i < node->child_count; i++) {
    count += mylite_ast_count_create_table_keys(node->children[i]);
  }
  return count;
}

static void mylite_ast_fill_create_table_keys(MyliteAst *ast,
                                              MyliteAstStatement *statement,
                                              const MyliteAstNode *node,
                                              size_t *index, int *ok) {
  if (statement == NULL || node == NULL || index == NULL || ok == NULL || !*ok ||
      *index >= statement->create_table_key_count) {
    return;
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, "nt_constraint") == 0) {
    const MyliteAstNode *elem = mylite_ast_find_constraint_elem(node);
    if (mylite_ast_classify_create_table_key(elem) !=
        MYLITE_CREATE_TABLE_KEY_UNKNOWN) {
      *ok = mylite_ast_fill_create_table_key(
          ast, &statement->create_table_keys[*index], node);
      if (*ok) {
        (*index)++;
      }
    }
    return;
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, "nt_column_def") == 0) {
    return;
  }
  if (mylite_ast_is_nested_target_boundary(node)) {
    return;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    mylite_ast_fill_create_table_keys(ast, statement, node->children[i], index, ok);
    if (!*ok) {
      return;
    }
  }
}

static int mylite_ast_fill_create_table_key(MyliteAst *ast,
                                            MyliteAstCreateTableKey *key,
                                            const MyliteAstNode *constraint) {
  const MyliteAstNode *elem = mylite_ast_find_constraint_elem(constraint);
  key->kind = mylite_ast_classify_create_table_key(elem);
  key->start = mylite_ast_node_start(constraint);
  key->end = mylite_ast_node_end(constraint);
  mylite_ast_set_create_table_key_names(key, constraint);
  mylite_ast_set_create_table_key_index_type(key, elem);

  if (key->kind != MYLITE_CREATE_TABLE_KEY_CHECK) {
    size_t remaining = 0;
    const MyliteAstNode *local_parts =
        mylite_ast_find_nth_symbol(elem, "nt_index_part_specification_list",
                                   &remaining);
    if (!mylite_ast_set_create_table_key_parts(ast, key, local_parts, 0)) {
      return 0;
    }
  }
  const MyliteAstNode *index_options =
      mylite_ast_find_first_symbol(elem, "nt_index_option_list");
  if (!mylite_ast_set_create_table_key_options(ast, key, index_options)) {
    return 0;
  }
  if (key->kind == MYLITE_CREATE_TABLE_KEY_FOREIGN) {
    return mylite_ast_set_create_table_key_reference(ast, key, elem);
  }
  if (key->kind == MYLITE_CREATE_TABLE_KEY_CHECK) {
    mylite_ast_set_create_table_key_check(key, elem);
  }
  return 1;
}

static MyliteCreateTableKeyKind mylite_ast_classify_create_table_key(
    const MyliteAstNode *constraint_elem) {
  if (constraint_elem == NULL) {
    return MYLITE_CREATE_TABLE_KEY_UNKNOWN;
  }
  if (mylite_ast_find_first_token(constraint_elem, MYLITE_TOK_FOREIGN) != NULL) {
    return MYLITE_CREATE_TABLE_KEY_FOREIGN;
  }
  if (mylite_ast_find_first_token(constraint_elem, MYLITE_TOK_FULLTEXT) != NULL) {
    return MYLITE_CREATE_TABLE_KEY_FULLTEXT;
  }
  if (mylite_ast_find_first_token(constraint_elem, MYLITE_TOK_SPATIAL) != NULL) {
    return MYLITE_CREATE_TABLE_KEY_SPATIAL;
  }
  if (mylite_ast_find_first_token(constraint_elem, MYLITE_TOK_UNIQUE) != NULL) {
    return MYLITE_CREATE_TABLE_KEY_UNIQUE;
  }
  if (mylite_ast_find_first_token(constraint_elem, MYLITE_TOK_PRIMARY) != NULL) {
    return MYLITE_CREATE_TABLE_KEY_PRIMARY;
  }
  if (mylite_ast_find_first_token(constraint_elem, MYLITE_TOK_CHECK) != NULL) {
    return MYLITE_CREATE_TABLE_KEY_CHECK;
  }
  if (mylite_ast_find_first_token(constraint_elem, MYLITE_TOK_KEY) != NULL ||
      mylite_ast_find_first_token(constraint_elem, MYLITE_TOK_INDEX) != NULL) {
    return MYLITE_CREATE_TABLE_KEY_INDEX;
  }
  return MYLITE_CREATE_TABLE_KEY_UNKNOWN;
}

static void mylite_ast_set_create_table_key_names(MyliteAstCreateTableKey *key,
                                                  const MyliteAstNode *constraint) {
  const MyliteAstNode *constraint_name =
      mylite_ast_find_constraint_keyword_name(constraint);
  if (constraint_name != NULL) {
    key->constraint_name_start = mylite_ast_node_start(constraint_name);
    key->constraint_name_end = mylite_ast_node_end(constraint_name);
  }

  const MyliteAstNode *elem = mylite_ast_find_constraint_elem(constraint);
  const MyliteAstNode *index_name = mylite_ast_find_first_symbol(elem, "nt_index_name");
  if (index_name != NULL && index_name->has_span) {
    key->name_start = mylite_ast_node_start(index_name);
    key->name_end = mylite_ast_node_end(index_name);
  }
}

static void mylite_ast_set_create_table_key_index_type(
    MyliteAstCreateTableKey *key, const MyliteAstNode *constraint_elem) {
  const MyliteAstNode *index_type =
      mylite_ast_find_first_symbol(constraint_elem, "nt_index_type");
  if (index_type != NULL && index_type->has_span) {
    key->index_type_start = mylite_ast_node_start(index_type);
    key->index_type_end = mylite_ast_node_end(index_type);
  }
}

static int mylite_ast_set_create_table_key_reference(
    MyliteAst *ast, MyliteAstCreateTableKey *key,
    const MyliteAstNode *constraint_elem) {
  const MyliteAstNode *refer_def =
      mylite_ast_find_first_symbol(constraint_elem, "nt_refer_def");
  if (refer_def == NULL) {
    return 1;
  }

  const MyliteAstNode *table_name =
      mylite_ast_find_first_symbol(refer_def, "nt_table_name");
  if (table_name != NULL) {
    key->referenced_table_start = mylite_ast_node_start(table_name);
    key->referenced_table_end = mylite_ast_node_end(table_name);
    key->referenced_table_name_start = key->referenced_table_start;
    key->referenced_table_name_end = key->referenced_table_end;
    mylite_ast_set_table_name_span_parts(
        table_name, &key->referenced_table_schema_start,
        &key->referenced_table_schema_end, &key->referenced_table_name_start,
        &key->referenced_table_name_end);
  }

  size_t remaining = 1;
  const MyliteAstNode *referenced_parts =
      mylite_ast_find_nth_symbol(constraint_elem, "nt_index_part_specification_list",
                                 &remaining);
  if (!mylite_ast_set_create_table_key_parts(ast, key, referenced_parts, 1)) {
    return 0;
  }

  const MyliteAstNode *match = mylite_ast_find_first_symbol(refer_def, "nt_match");
  if (match != NULL && match->has_span) {
    key->foreign_match_kind = mylite_ast_classify_foreign_match(match);
    key->foreign_match_start = mylite_ast_node_start(match);
    key->foreign_match_end = mylite_ast_node_end(match);
  }

  const MyliteAstNode *on_delete =
      mylite_ast_find_first_symbol(refer_def, "nt_on_delete");
  if (on_delete != NULL && on_delete->has_span) {
    key->foreign_on_delete_action = mylite_ast_classify_foreign_action(on_delete);
    key->foreign_on_delete_start = mylite_ast_node_start(on_delete);
    key->foreign_on_delete_end = mylite_ast_node_end(on_delete);
  }

  const MyliteAstNode *on_update =
      mylite_ast_find_first_symbol(refer_def, "nt_on_update");
  if (on_update != NULL && on_update->has_span) {
    key->foreign_on_update_action = mylite_ast_classify_foreign_action(on_update);
    key->foreign_on_update_start = mylite_ast_node_start(on_update);
    key->foreign_on_update_end = mylite_ast_node_end(on_update);
  }
  return 1;
}

static void mylite_ast_set_create_table_key_check(
    MyliteAstCreateTableKey *key, const MyliteAstNode *constraint_elem) {
  const MyliteAstNode *expression =
      mylite_ast_find_first_symbol(constraint_elem, "nt_expression");
  if (expression != NULL && expression->has_span) {
    key->check_expression_start = mylite_ast_node_start(expression);
    key->check_expression_end = mylite_ast_node_end(expression);
  }

  const MyliteAstNode *enforcement =
      mylite_ast_find_first_symbol(constraint_elem, "nt_enforced_or_not");
  if (enforcement != NULL && enforcement->has_span) {
    key->check_enforcement = mylite_ast_classify_check_enforcement(enforcement);
    key->check_enforcement_start = mylite_ast_node_start(enforcement);
    key->check_enforcement_end = mylite_ast_node_end(enforcement);
  }
}

static MyliteCreateTableForeignMatchKind mylite_ast_classify_foreign_match(
    const MyliteAstNode *node) {
  if (mylite_ast_find_first_token(node, MYLITE_TOK_FULL) != NULL) {
    return MYLITE_CREATE_TABLE_FOREIGN_MATCH_FULL;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_PARTIAL) != NULL) {
    return MYLITE_CREATE_TABLE_FOREIGN_MATCH_PARTIAL;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_SIMPLE) != NULL) {
    return MYLITE_CREATE_TABLE_FOREIGN_MATCH_SIMPLE;
  }
  return MYLITE_CREATE_TABLE_FOREIGN_MATCH_UNSPECIFIED;
}

static MyliteCreateTableForeignAction mylite_ast_classify_foreign_action(
    const MyliteAstNode *node) {
  if (mylite_ast_find_first_token(node, MYLITE_TOK_RESTRICT) != NULL) {
    return MYLITE_CREATE_TABLE_FOREIGN_ACTION_RESTRICT;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_CASCADE) != NULL) {
    return MYLITE_CREATE_TABLE_FOREIGN_ACTION_CASCADE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_SET) != NULL &&
      mylite_ast_find_first_token(node, MYLITE_TOK_NULL) != NULL) {
    return MYLITE_CREATE_TABLE_FOREIGN_ACTION_SET_NULL;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_NO) != NULL &&
      mylite_ast_find_first_token(node, MYLITE_TOK_ACTION) != NULL) {
    return MYLITE_CREATE_TABLE_FOREIGN_ACTION_NO_ACTION;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_SET) != NULL &&
      mylite_ast_find_first_token(node, MYLITE_TOK_DEFAULT_KWD) != NULL) {
    return MYLITE_CREATE_TABLE_FOREIGN_ACTION_SET_DEFAULT;
  }
  return MYLITE_CREATE_TABLE_FOREIGN_ACTION_UNSPECIFIED;
}

static MyliteCreateTableCheckEnforcement mylite_ast_classify_check_enforcement(
    const MyliteAstNode *node) {
  if (mylite_ast_find_first_token(node, MYLITE_TOK_NOT) != NULL) {
    return MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_NOT_ENFORCED;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_ENFORCED) != NULL) {
    return MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_ENFORCED;
  }
  return MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_UNSPECIFIED;
}

static int mylite_ast_set_create_table_key_parts(MyliteAst *ast,
                                                 MyliteAstCreateTableKey *key,
                                                 const MyliteAstNode *list,
                                                 int referenced) {
  if (list == NULL) {
    return 1;
  }
  size_t count = mylite_ast_count_index_part_specs(list);
  if (count == 0) {
    return 1;
  }

  MyliteAstCreateTableKeyPart *parts =
      mylite_ast_alloc(ast, count * sizeof(*parts));
  if (parts == NULL) {
    return 0;
  }

  size_t index = 0;
  mylite_ast_fill_index_part_specs(parts, count, list, &index);
  if (referenced) {
    key->referenced_columns = parts;
    key->referenced_column_count = count;
  } else {
    key->columns = parts;
    key->column_count = count;
  }
  return index == count;
}

static int mylite_ast_set_create_table_key_options(MyliteAst *ast,
                                                   MyliteAstCreateTableKey *key,
                                                   const MyliteAstNode *list) {
  if (list == NULL || !list->has_span) {
    return 1;
  }
  size_t count = mylite_ast_count_index_options(list);
  if (count == 0) {
    return 1;
  }

  MyliteAstCreateTableKeyOption *options =
      mylite_ast_alloc(ast, count * sizeof(*options));
  if (options == NULL) {
    return 0;
  }

  size_t index = 0;
  mylite_ast_fill_index_options(options, count, list, &index);
  key->options = options;
  key->option_count = count;
  return index == count;
}

static size_t mylite_ast_count_index_part_specs(const MyliteAstNode *node) {
  if (node == NULL) {
    return 0;
  }
  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_index_part_specification") == 0) {
    return 1;
  }

  size_t count = 0;
  for (size_t i = 0; i < node->child_count; i++) {
    count += mylite_ast_count_index_part_specs(node->children[i]);
  }
  return count;
}

static void mylite_ast_fill_index_part_specs(MyliteAstCreateTableKeyPart *parts,
                                             size_t count,
                                             const MyliteAstNode *node,
                                             size_t *index) {
  if (parts == NULL || node == NULL || index == NULL || *index >= count) {
    return;
  }
  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_index_part_specification") == 0) {
    mylite_ast_fill_key_part(&parts[*index], node);
    (*index)++;
    return;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    mylite_ast_fill_index_part_specs(parts, count, node->children[i], index);
  }
}

static void mylite_ast_fill_key_part(MyliteAstCreateTableKeyPart *part,
                                     const MyliteAstNode *node) {
  part->start = mylite_ast_node_start(node);
  part->end = mylite_ast_node_end(node);

  const MyliteAstNode *column_name =
      mylite_ast_find_first_symbol(node, "nt_column_name");
  if (column_name != NULL) {
    part->kind = MYLITE_CREATE_TABLE_KEY_PART_COLUMN;
    part->name_start = mylite_ast_node_start(column_name);
    part->name_end = mylite_ast_node_end(column_name);
  } else {
    const MyliteAstNode *expression =
        mylite_ast_find_first_symbol(node, "nt_expression");
    if (expression != NULL && expression->has_span) {
      part->kind = MYLITE_CREATE_TABLE_KEY_PART_EXPRESSION;
      part->expression_start = mylite_ast_node_start(expression);
      part->expression_end = mylite_ast_node_end(expression);
    }
  }

  const MyliteAstNode *prefix =
      mylite_ast_find_first_symbol(node, "nt_opt_field_len");
  if (prefix != NULL && prefix->has_span) {
    part->prefix_start = mylite_ast_node_start(prefix);
    part->prefix_end = mylite_ast_node_end(prefix);
    const MyliteAstNode *value =
        mylite_ast_find_first_symbol(prefix, "nt_length_num");
    if (value != NULL && value->has_span) {
      part->prefix_value_start = mylite_ast_node_start(value);
      part->prefix_value_end = mylite_ast_node_end(value);
    }
  }

  const MyliteAstNode *order = mylite_ast_find_first_symbol(node, "nt_opt_order");
  if (order != NULL && order->has_span) {
    part->order_start = mylite_ast_node_start(order);
    part->order_end = mylite_ast_node_end(order);
    if (mylite_ast_find_first_token(order, MYLITE_TOK_ASC) != NULL) {
      part->order = MYLITE_CREATE_TABLE_KEY_PART_ORDER_ASC;
    } else if (mylite_ast_find_first_token(order, MYLITE_TOK_DESC) != NULL) {
      part->order = MYLITE_CREATE_TABLE_KEY_PART_ORDER_DESC;
    }
  }
}

static size_t mylite_ast_count_index_options(const MyliteAstNode *node) {
  if (node == NULL) {
    return 0;
  }
  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_index_option") == 0) {
    return 1;
  }

  size_t count = 0;
  for (size_t i = 0; i < node->child_count; i++) {
    count += mylite_ast_count_index_options(node->children[i]);
  }
  return count;
}

static void mylite_ast_fill_index_options(MyliteAstCreateTableKeyOption *options,
                                          size_t count,
                                          const MyliteAstNode *node,
                                          size_t *index) {
  if (options == NULL || node == NULL || index == NULL || *index >= count) {
    return;
  }
  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_index_option") == 0) {
    mylite_ast_fill_key_option(&options[*index], node);
    (*index)++;
    return;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    mylite_ast_fill_index_options(options, count, node->children[i], index);
  }
}

static void mylite_ast_fill_key_option(MyliteAstCreateTableKeyOption *option,
                                       const MyliteAstNode *node) {
  option->kind = mylite_ast_classify_key_option(node);
  option->start = mylite_ast_node_start(node);
  option->end = mylite_ast_node_end(node);

  const MyliteAstNode *name = mylite_ast_find_key_option_name(node, option->kind);
  if (name != NULL) {
    option->name_start = mylite_ast_node_start(name);
    option->name_end = mylite_ast_node_end(name);
  }

  if (option->kind == MYLITE_CREATE_TABLE_KEY_OPTION_WITH_PARSER) {
    const MyliteAstNode *parser = mylite_ast_find_first_token(node, MYLITE_TOK_PARSER);
    if (parser != NULL) {
      option->name_end = mylite_ast_node_end(parser);
    }
  }

  if (option->name_end != 0) {
    mylite_ast_collect_value_span_after(node, option->name_end,
                                        &option->value_start,
                                        &option->value_end);
  }
}

static MyliteCreateTableKeyOptionKind mylite_ast_classify_key_option(
    const MyliteAstNode *node) {
  if (node == NULL) {
    return MYLITE_CREATE_TABLE_KEY_OPTION_UNKNOWN;
  }
  if (mylite_ast_find_first_symbol(node, "nt_index_type") != NULL) {
    return MYLITE_CREATE_TABLE_KEY_OPTION_INDEX_TYPE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_KEY_BLOCK_SIZE) != NULL) {
    return MYLITE_CREATE_TABLE_KEY_OPTION_KEY_BLOCK_SIZE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_COMMENT) != NULL) {
    return MYLITE_CREATE_TABLE_KEY_OPTION_COMMENT;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_WITH) != NULL &&
      mylite_ast_find_first_token(node, MYLITE_TOK_PARSER) != NULL) {
    return MYLITE_CREATE_TABLE_KEY_OPTION_WITH_PARSER;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_VISIBLE) != NULL) {
    return MYLITE_CREATE_TABLE_KEY_OPTION_VISIBLE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_INVISIBLE) != NULL) {
    return MYLITE_CREATE_TABLE_KEY_OPTION_INVISIBLE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_SECONDARY_ENGINE_ATTRIBUTE) !=
      NULL) {
    return MYLITE_CREATE_TABLE_KEY_OPTION_SECONDARY_ENGINE_ATTRIBUTE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_WHERE) != NULL) {
    return MYLITE_CREATE_TABLE_KEY_OPTION_WHERE;
  }
  return MYLITE_CREATE_TABLE_KEY_OPTION_UNKNOWN;
}

static const MyliteAstNode *mylite_ast_find_key_option_name(
    const MyliteAstNode *node, MyliteCreateTableKeyOptionKind kind) {
  if (kind == MYLITE_CREATE_TABLE_KEY_OPTION_INDEX_TYPE) {
    return mylite_ast_find_first_token(node, MYLITE_TOK_USING);
  }

  int token = 0;
  switch (kind) {
    case MYLITE_CREATE_TABLE_KEY_OPTION_KEY_BLOCK_SIZE:
      token = MYLITE_TOK_KEY_BLOCK_SIZE;
      break;
    case MYLITE_CREATE_TABLE_KEY_OPTION_COMMENT:
      token = MYLITE_TOK_COMMENT;
      break;
    case MYLITE_CREATE_TABLE_KEY_OPTION_WITH_PARSER:
      token = MYLITE_TOK_WITH;
      break;
    case MYLITE_CREATE_TABLE_KEY_OPTION_VISIBLE:
      token = MYLITE_TOK_VISIBLE;
      break;
    case MYLITE_CREATE_TABLE_KEY_OPTION_INVISIBLE:
      token = MYLITE_TOK_INVISIBLE;
      break;
    case MYLITE_CREATE_TABLE_KEY_OPTION_SECONDARY_ENGINE_ATTRIBUTE:
      token = MYLITE_TOK_SECONDARY_ENGINE_ATTRIBUTE;
      break;
    case MYLITE_CREATE_TABLE_KEY_OPTION_WHERE:
      token = MYLITE_TOK_WHERE;
      break;
    case MYLITE_CREATE_TABLE_KEY_OPTION_UNKNOWN:
    case MYLITE_CREATE_TABLE_KEY_OPTION_INDEX_TYPE:
      break;
  }
  return token == 0 ? mylite_ast_find_first_spanned_child(node)
                    : mylite_ast_find_first_token(node, token);
}

static const MyliteAstNode *mylite_ast_find_nth_symbol(const MyliteAstNode *node,
                                                       const char *symbol_name,
                                                       size_t *remaining) {
  if (node == NULL || remaining == NULL) {
    return NULL;
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, symbol_name) == 0) {
    if (*remaining == 0) {
      return node;
    }
    (*remaining)--;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *found =
        mylite_ast_find_nth_symbol(node->children[i], symbol_name, remaining);
    if (found != NULL) {
      return found;
    }
  }
  return NULL;
}

static const MyliteAstNode *mylite_ast_find_constraint_elem(
    const MyliteAstNode *constraint) {
  return mylite_ast_find_first_symbol(constraint, "nt_constraint_elem");
}

static const MyliteAstNode *mylite_ast_find_constraint_keyword_name(
    const MyliteAstNode *constraint) {
  const MyliteAstNode *keyword =
      mylite_ast_find_first_symbol(constraint, "nt_constraint_keyword_opt");
  if (keyword == NULL || !keyword->has_span) {
    return NULL;
  }
  const MyliteAstNode *symbol = mylite_ast_find_first_symbol(keyword, "nt_symbol");
  return symbol != NULL ? symbol : mylite_ast_find_first_symbol(keyword, "nt_identifier");
}

static int mylite_ast_collect_create_table_options(
    MyliteAst *ast, MyliteAstStatement *statement, const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL || payload == NULL) {
    return 1;
  }

  const MyliteAstNode *option_list =
      mylite_ast_find_first_symbol(payload, "nt_create_table_option_list_opt");
  size_t count = mylite_ast_count_create_table_options(option_list);
  if (count == 0) {
    return 1;
  }

  statement->create_table_options =
      mylite_ast_alloc(ast, count * sizeof(*statement->create_table_options));
  if (statement->create_table_options == NULL) {
    return 0;
  }
  statement->create_table_option_count = count;

  size_t index = 0;
  mylite_ast_fill_create_table_options(statement, option_list, &index);
  return index == count;
}

static size_t mylite_ast_count_create_table_options(const MyliteAstNode *node) {
  if (node == NULL) {
    return 0;
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, "nt_table_option") == 0) {
    return 1;
  }

  size_t count = 0;
  for (size_t i = 0; i < node->child_count; i++) {
    count += mylite_ast_count_create_table_options(node->children[i]);
  }
  return count;
}

static void mylite_ast_fill_create_table_options(
    MyliteAstStatement *statement, const MyliteAstNode *node, size_t *index) {
  if (statement == NULL || node == NULL || index == NULL ||
      *index >= statement->create_table_option_count) {
    return;
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, "nt_table_option") == 0) {
    mylite_ast_fill_create_table_option(
        &statement->create_table_options[*index], node);
    (*index)++;
    return;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    mylite_ast_fill_create_table_options(statement, node->children[i], index);
  }
}

static void mylite_ast_fill_create_table_option(
    MyliteAstCreateTableOption *option, const MyliteAstNode *node) {
  option->kind = mylite_ast_classify_create_table_option(node);
  option->start = mylite_ast_node_start(node);
  option->end = mylite_ast_node_end(node);
  mylite_ast_set_create_table_option_name(option, node);
  mylite_ast_set_create_table_option_value(option, node);
}

static MyliteCreateTableOptionKind mylite_ast_classify_create_table_option(
    const MyliteAstNode *node) {
  if (node == NULL) {
    return MYLITE_CREATE_TABLE_OPTION_UNKNOWN;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_SECONDARY_ENGINE_ATTRIBUTE) !=
      NULL) {
    return MYLITE_CREATE_TABLE_OPTION_SECONDARY_ENGINE_ATTRIBUTE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_ENGINE_ATTRIBUTE) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_ENGINE_ATTRIBUTE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_SECONDARY_ENGINE) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_SECONDARY_ENGINE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_ENGINE) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_ENGINE;
  }
  if (mylite_ast_find_first_symbol(node, "nt_charset_kw") != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_CHARSET;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_COLLATE) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_COLLATE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_AUTO_INCREMENT) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_AUTO_INCREMENT;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_COMMENT) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_COMMENT;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_ROW_FORMAT) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_ROW_FORMAT;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_KEY_BLOCK_SIZE) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_KEY_BLOCK_SIZE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_AUTOEXTEND_SIZE) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_AUTOEXTEND_SIZE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_AVG_ROW_LENGTH) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_AVG_ROW_LENGTH;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_MAX_ROWS) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_MAX_ROWS;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_MIN_ROWS) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_MIN_ROWS;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_DELAY_KEY_WRITE) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_DELAY_KEY_WRITE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_ENCRYPTION) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_ENCRYPTION;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_STATS_PERSISTENT) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_STATS_PERSISTENT;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_PACK_KEYS) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_PACK_KEYS;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_DATA) != NULL &&
      mylite_ast_find_first_token(node, MYLITE_TOK_DIRECTORY) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_DATA_DIRECTORY;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_INDEX) != NULL &&
      mylite_ast_find_first_token(node, MYLITE_TOK_DIRECTORY) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_INDEX_DIRECTORY;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_TABLESPACE) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_TABLESPACE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_STORAGE) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_STORAGE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_COMPRESSION) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_COMPRESSION;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_CONNECTION) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_CONNECTION;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_PASSWORD) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_PASSWORD;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_INSERT_METHOD) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_INSERT_METHOD;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_UNION) != NULL) {
    return MYLITE_CREATE_TABLE_OPTION_UNION;
  }
  return MYLITE_CREATE_TABLE_OPTION_UNKNOWN;
}

static void mylite_ast_set_create_table_option_name(
    MyliteAstCreateTableOption *option, const MyliteAstNode *node) {
  const MyliteAstNode *name =
      mylite_ast_find_create_table_option_name(node, option->kind);
  if (name != NULL) {
    option->name_start = mylite_ast_node_start(name);
    option->name_end = mylite_ast_node_end(name);
  }

  if (option->kind == MYLITE_CREATE_TABLE_OPTION_DATA_DIRECTORY ||
      option->kind == MYLITE_CREATE_TABLE_OPTION_INDEX_DIRECTORY) {
    const MyliteAstNode *directory =
        mylite_ast_find_first_token(node, MYLITE_TOK_DIRECTORY);
    if (directory != NULL) {
      option->name_end = mylite_ast_node_end(directory);
    }
  }
}

static void mylite_ast_set_create_table_option_value(
    MyliteAstCreateTableOption *option, const MyliteAstNode *node) {
  if (option->name_end == 0) {
    return;
  }
  mylite_ast_collect_value_span_after(node, option->name_end,
                                      &option->value_start,
                                      &option->value_end);
}

static const MyliteAstNode *mylite_ast_find_create_table_option_name(
    const MyliteAstNode *node, MyliteCreateTableOptionKind kind) {
  if (kind == MYLITE_CREATE_TABLE_OPTION_CHARSET) {
    return mylite_ast_find_first_symbol(node, "nt_charset_kw");
  }

  int token = 0;
  switch (kind) {
    case MYLITE_CREATE_TABLE_OPTION_ENGINE:
      token = MYLITE_TOK_ENGINE;
      break;
    case MYLITE_CREATE_TABLE_OPTION_SECONDARY_ENGINE:
      token = MYLITE_TOK_SECONDARY_ENGINE;
      break;
    case MYLITE_CREATE_TABLE_OPTION_COLLATE:
      token = MYLITE_TOK_COLLATE;
      break;
    case MYLITE_CREATE_TABLE_OPTION_AUTO_INCREMENT:
      token = MYLITE_TOK_AUTO_INCREMENT;
      break;
    case MYLITE_CREATE_TABLE_OPTION_COMMENT:
      token = MYLITE_TOK_COMMENT;
      break;
    case MYLITE_CREATE_TABLE_OPTION_ROW_FORMAT:
      token = MYLITE_TOK_ROW_FORMAT;
      break;
    case MYLITE_CREATE_TABLE_OPTION_KEY_BLOCK_SIZE:
      token = MYLITE_TOK_KEY_BLOCK_SIZE;
      break;
    case MYLITE_CREATE_TABLE_OPTION_AUTOEXTEND_SIZE:
      token = MYLITE_TOK_AUTOEXTEND_SIZE;
      break;
    case MYLITE_CREATE_TABLE_OPTION_AVG_ROW_LENGTH:
      token = MYLITE_TOK_AVG_ROW_LENGTH;
      break;
    case MYLITE_CREATE_TABLE_OPTION_MAX_ROWS:
      token = MYLITE_TOK_MAX_ROWS;
      break;
    case MYLITE_CREATE_TABLE_OPTION_MIN_ROWS:
      token = MYLITE_TOK_MIN_ROWS;
      break;
    case MYLITE_CREATE_TABLE_OPTION_DELAY_KEY_WRITE:
      token = MYLITE_TOK_DELAY_KEY_WRITE;
      break;
    case MYLITE_CREATE_TABLE_OPTION_ENCRYPTION:
      token = MYLITE_TOK_ENCRYPTION;
      break;
    case MYLITE_CREATE_TABLE_OPTION_STATS_PERSISTENT:
      token = MYLITE_TOK_STATS_PERSISTENT;
      break;
    case MYLITE_CREATE_TABLE_OPTION_PACK_KEYS:
      token = MYLITE_TOK_PACK_KEYS;
      break;
    case MYLITE_CREATE_TABLE_OPTION_TABLESPACE:
      token = MYLITE_TOK_TABLESPACE;
      break;
    case MYLITE_CREATE_TABLE_OPTION_STORAGE:
      token = MYLITE_TOK_STORAGE;
      break;
    case MYLITE_CREATE_TABLE_OPTION_COMPRESSION:
      token = MYLITE_TOK_COMPRESSION;
      break;
    case MYLITE_CREATE_TABLE_OPTION_CONNECTION:
      token = MYLITE_TOK_CONNECTION;
      break;
    case MYLITE_CREATE_TABLE_OPTION_PASSWORD:
      token = MYLITE_TOK_PASSWORD;
      break;
    case MYLITE_CREATE_TABLE_OPTION_INSERT_METHOD:
      token = MYLITE_TOK_INSERT_METHOD;
      break;
    case MYLITE_CREATE_TABLE_OPTION_DATA_DIRECTORY:
      token = MYLITE_TOK_DATA;
      break;
    case MYLITE_CREATE_TABLE_OPTION_INDEX_DIRECTORY:
      token = MYLITE_TOK_INDEX;
      break;
    case MYLITE_CREATE_TABLE_OPTION_UNION:
      token = MYLITE_TOK_UNION;
      break;
    case MYLITE_CREATE_TABLE_OPTION_ENGINE_ATTRIBUTE:
      token = MYLITE_TOK_ENGINE_ATTRIBUTE;
      break;
    case MYLITE_CREATE_TABLE_OPTION_SECONDARY_ENGINE_ATTRIBUTE:
      token = MYLITE_TOK_SECONDARY_ENGINE_ATTRIBUTE;
      break;
    case MYLITE_CREATE_TABLE_OPTION_UNKNOWN:
    case MYLITE_CREATE_TABLE_OPTION_CHARSET:
      break;
  }
  return token == 0 ? mylite_ast_find_first_spanned_child(node)
                    : mylite_ast_find_first_token(node, token);
}

static const MyliteAstNode *mylite_ast_find_first_direct_spanned_child(
    const MyliteAstNode *node) {
  if (node == NULL) {
    return NULL;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *child = node->children[i];
    if (child != NULL && child->has_span) {
      return child;
    }
  }
  return NULL;
}

static const MyliteAstNode *mylite_ast_find_first_spanned_child(
    const MyliteAstNode *node) {
  if (node == NULL || !node->has_span) {
    return NULL;
  }
  if (node->kind == MYLITE_AST_NODE_TOKEN) {
    return node;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *found =
        mylite_ast_find_first_spanned_child(node->children[i]);
    if (found != NULL) {
      return found;
    }
  }
  return node;
}

static void mylite_ast_extend_span(size_t *start, size_t *end,
                                   const MyliteAstNode *node) {
  if (start == NULL || end == NULL || node == NULL || !node->has_span) {
    return;
  }
  if (*end == 0) {
    *start = mylite_ast_node_start(node);
    *end = mylite_ast_node_end(node);
    return;
  }
  if (mylite_ast_node_start(node) < *start) {
    *start = mylite_ast_node_start(node);
  }
  if (mylite_ast_node_end(node) > *end) {
    *end = mylite_ast_node_end(node);
  }
}

static void mylite_ast_collect_value_span_after(const MyliteAstNode *node,
                                                size_t min_start,
                                                size_t *value_start,
                                                size_t *value_end) {
  if (node == NULL || value_start == NULL || value_end == NULL ||
      !node->has_span || node->end <= min_start) {
    return;
  }
  if (node->kind == MYLITE_AST_NODE_TOKEN) {
    if (node->start >= min_start && node->token != MYLITE_TOK_EQ) {
      if (*value_end == 0) {
        *value_start = node->start;
      }
      *value_end = node->end;
    }
    return;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    mylite_ast_collect_value_span_after(node->children[i], min_start,
                                        value_start, value_end);
  }
}

static const MyliteAstNode *mylite_ast_find_first_symbol(const MyliteAstNode *node,
                                                         const char *symbol_name) {
  if (node == NULL) {
    return NULL;
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, symbol_name) == 0) {
    return node;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *found =
        mylite_ast_find_first_symbol(node->children[i], symbol_name);
    if (found != NULL) {
      return found;
    }
  }
  return NULL;
}

static const MyliteAstNode *mylite_ast_find_first_token(const MyliteAstNode *node,
                                                        int token) {
  if (node == NULL) {
    return NULL;
  }
  if (node->kind == MYLITE_AST_NODE_TOKEN && node->token == token) {
    return node;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *found = mylite_ast_find_first_token(node->children[i], token);
    if (found != NULL) {
      return found;
    }
  }
  return NULL;
}

static int mylite_ast_first_token(const MyliteAstNode *node) {
  if (node == NULL) {
    return 0;
  }
  if (node->kind == MYLITE_AST_NODE_TOKEN) {
    return node->token;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    int token = mylite_ast_first_token(node->children[i]);
    if (token != 0) {
      return token;
    }
  }
  return 0;
}

static void mylite_ast_set_table_name_span_parts(const MyliteAstNode *node,
                                                 size_t *schema_start,
                                                 size_t *schema_end,
                                                 size_t *name_start,
                                                 size_t *name_end) {
  if (node == NULL) {
    return;
  }

  const MyliteAstNode *schema = NULL;
  const MyliteAstNode *name = NULL;
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *child = node->children[i];
    if (child == NULL || child->symbol_name == NULL ||
        strcmp(child->symbol_name, "nt_identifier") != 0) {
      continue;
    }
    if (schema == NULL) {
      schema = child;
    } else {
      name = child;
      break;
    }
  }

  if (schema != NULL && name != NULL) {
    *schema_start = mylite_ast_node_start(schema);
    *schema_end = mylite_ast_node_end(schema);
    *name_start = mylite_ast_node_start(name);
    *name_end = mylite_ast_node_end(name);
  } else if (schema != NULL) {
    *name_start = mylite_ast_node_start(schema);
    *name_end = mylite_ast_node_end(schema);
  }
}

static int mylite_ast_is_nested_target_boundary(const MyliteAstNode *node) {
  if (node == NULL || node->symbol_name == NULL) {
    return 0;
  }

  static const char *const symbols[] = {
      "nt_sub_select",
      "nt_select_stmt",
      "nt_select_stmt_with_clause",
      "nt_set_opr_stmt",
  };
  return symbol_is_one_of(node->symbol_name, symbols,
                          sizeof(symbols) / sizeof(symbols[0]));
}

static void mylite_ast_set_table_name_parts(MyliteAstStatementTarget *target,
                                            const MyliteAstNode *node) {
  if (target == NULL || node == NULL) {
    return;
  }

  mylite_ast_set_table_name_span_parts(node, &target->schema_start,
                                       &target->schema_end, &target->name_start,
                                       &target->name_end);
}

static int symbol_is_one_of(const char *symbol_name, const char *const *symbols,
                            size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (strcmp(symbol_name, symbols[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

static int symbol_has_prefix(const char *symbol_name, const char *prefix) {
  size_t prefix_length = strlen(prefix);
  return strncmp(symbol_name, prefix, prefix_length) == 0;
}

static const MyliteAstStatement *mylite_ast_statement_at(const MyliteAst *ast,
                                                         size_t index) {
  if (ast == NULL || index >= ast->statement_count) {
    return NULL;
  }
  return &ast->statements[index];
}

static const MyliteAstStatementTarget *mylite_ast_statement_target_at(
    const MyliteAst *ast, size_t statement_index, size_t target_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  if (statement == NULL || target_index >= statement->target_count) {
    return NULL;
  }
  return &statement->targets[target_index];
}

static const MyliteAstCreateTableKey *mylite_ast_create_table_key_at(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  if (statement == NULL || key_index >= statement->create_table_key_count) {
    return NULL;
  }
  return &statement->create_table_keys[key_index];
}

static const MyliteAstCreateTableKeyPart *mylite_ast_create_table_key_part_at(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index, int referenced) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  if (key == NULL) {
    return NULL;
  }
  if (referenced) {
    return column_index >= key->referenced_column_count
               ? NULL
               : &key->referenced_columns[column_index];
  }
  return column_index >= key->column_count ? NULL : &key->columns[column_index];
}

static const MyliteAstCreateTableKeyOption *mylite_ast_create_table_key_option_at(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t option_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  if (key == NULL || option_index >= key->option_count) {
    return NULL;
  }
  return &key->options[option_index];
}

static const MyliteAstCreateTableOption *mylite_ast_create_table_option_at(
    const MyliteAst *ast, size_t statement_index, size_t option_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  if (statement == NULL ||
      option_index >= statement->create_table_option_count) {
    return NULL;
  }
  return &statement->create_table_options[option_index];
}

static const MyliteAstCreateTableColumn *mylite_ast_create_table_column_at(
    const MyliteAst *ast, size_t statement_index, size_t column_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  if (statement == NULL ||
      column_index >= statement->create_table_column_count) {
    return NULL;
  }
  return &statement->create_table_columns[column_index];
}

static const MyliteAstCreateTableTypeElement *
mylite_ast_create_table_column_type_element_at(const MyliteAst *ast,
                                               size_t statement_index,
                                               size_t column_index,
                                               size_t element_index) {
  const MyliteAstCreateTableColumn *column =
      mylite_ast_create_table_column_at(ast, statement_index, column_index);
  if (column == NULL || element_index >= column->type_element_count) {
    return NULL;
  }
  return &column->type_elements[element_index];
}

static void mylite_parser_state_no_memory(MyliteParserState *state) {
  if (state == NULL || state->result == NULL || state->reported_error) {
    return;
  }
  state->reported_error = 1;
  state->result->status = MYLITE_PARSE_NO_MEMORY;
  state->result->offset = state->token_offset;
  snprintf(state->result->message, sizeof(state->result->message),
           "AST allocation failed near byte %zu", state->token_offset);
}

MyliteAstNode *mylite_parser_state_token(MyliteParserState *state, int token,
                                         size_t offset, size_t length) {
  MyliteAstNode *node =
      mylite_ast_make_node(state, MYLITE_AST_NODE_TOKEN, 0, "token", token, 0, NULL);
  if (node != NULL) {
    node->has_span = 1;
    node->start = offset;
    node->end = offset + length;
  }
  return node;
}

MyliteAstNode *mylite_parser_state_reduce(MyliteParserState *state, unsigned rule_id,
                                          const char *symbol_name, size_t child_count,
                                          MyliteAstNode *const *children) {
  return mylite_ast_make_node(state, MYLITE_AST_NODE_RULE, rule_id, symbol_name, 0,
                              child_count, children);
}

void mylite_parser_state_root(MyliteParserState *state, MyliteAstNode *root) {
  if (state == NULL || !state->build_ast) {
    return;
  }
  state->root = root;
  if (state->ast != NULL) {
    state->ast->root = root;
  }
}

void mylite_parser_state_recognized_root(MyliteParserState *state, size_t length) {
  MyliteAstNode *root = mylite_parser_state_reduce(
      state, 0, "nt_mylite_recognized_statement", 0, NULL);
  if (root != NULL) {
    root->has_span = 1;
    root->start = 0;
    root->end = length;
    mylite_parser_state_root(state, root);
  }
}

void mylite_parser_state_accept(MyliteParserState *state) {
  if (state != NULL) {
    state->accepted = 1;
  }
}

void mylite_parser_state_failure(MyliteParserState *state) {
  if (state == NULL || state->result == NULL || state->reported_error) {
    return;
  }
  state->failed = 1;
  state->reported_error = 1;
  state->result->status = MYLITE_PARSE_SYNTAX_ERROR;
  state->result->offset = state->token_offset;
  snprintf(state->result->message, sizeof(state->result->message),
           "parse failed near byte %zu", state->token_offset);
}

void mylite_parser_state_syntax_error(MyliteParserState *state, int token) {
  if (state == NULL || state->result == NULL || state->reported_error) {
    return;
  }
  state->reported_error = 1;
  state->result->status = MYLITE_PARSE_SYNTAX_ERROR;
  state->result->offset = state->token_offset;
  state->result->token = token;
  snprintf(state->result->message, sizeof(state->result->message),
           "syntax error near token %d at byte %zu", token, state->token_offset);
}

void mylite_parser_state_stack_overflow(MyliteParserState *state) {
  if (state == NULL || state->result == NULL || state->reported_error) {
    return;
  }
  state->reported_error = 1;
  state->result->status = MYLITE_PARSE_NO_MEMORY;
  state->result->offset = state->token_offset;
  snprintf(state->result->message, sizeof(state->result->message),
           "parser stack overflow near byte %zu", state->token_offset);
}

static int accepts_mysqltest_harness_statement(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "CALL")) {
    return 0;
  }
  if (*cursor != ' ' && *cursor != '\t' && *cursor != '\n' && *cursor != '\r' &&
      *cursor != '\f') {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "mtr")) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (*cursor != '.') {
    return 0;
  }
  cursor++;
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "add_suppression")) {
    return 0;
  }
  skip_ascii_space(&cursor);
  return *cursor == '(';
}

static int accepts_parenthesized_create_table_set_statement(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "CREATE")) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (match_ascii_ci(&cursor, "TEMPORARY")) {
    skip_ascii_space(&cursor);
  }
  if (!match_ascii_ci(&cursor, "TABLE")) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (match_ascii_ci(&cursor, "IF")) {
    skip_ascii_space(&cursor);
    if (!match_ascii_ci(&cursor, "NOT")) {
      return 0;
    }
    skip_ascii_space(&cursor);
    if (!match_ascii_ci(&cursor, "EXISTS")) {
      return 0;
    }
    skip_ascii_space(&cursor);
  }

  while (*cursor != '\0' && *cursor != ';' && *cursor != '(') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    cursor++;
  }
  if (*cursor != '(') {
    return 0;
  }
  cursor++;
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "SELECT")) {
    return 0;
  }

  int depth = 1;
  while (*cursor != '\0') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == '(') {
      depth++;
    } else if (*cursor == ')') {
      depth--;
      if (depth == 0) {
        cursor++;
        break;
      }
    }
    cursor++;
  }
  if (depth != 0) {
    return 0;
  }

  skip_ascii_space(&cursor);
  return match_ascii_ci(&cursor, "UNION") || match_ascii_ci(&cursor, "EXCEPT") ||
         match_ascii_ci(&cursor, "INTERSECT");
}

static int accepts_alter_user_default_role_statement(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "ALTER")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "USER")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }

  while (*cursor != '\0') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (!is_ascii_identifier_char((unsigned char)*(cursor - 1)) &&
        match_ascii_ci(&cursor, "DEFAULT") &&
        !is_ascii_identifier_char((unsigned char)*cursor)) {
      skip_ascii_space(&cursor);
      if (match_ascii_ci(&cursor, "ROLE") &&
          !is_ascii_identifier_char((unsigned char)*cursor)) {
        return 1;
      }
      return 0;
    }
    cursor++;
  }

  return 0;
}

static int accepts_create_user_default_role_statement(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "CREATE")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "USER")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }

  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (cursor == sql || !is_ascii_identifier_char((unsigned char)*(cursor - 1))) {
      const char *probe = cursor;
      if (match_ascii_ci(&probe, "DEFAULT") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        skip_ascii_space(&probe);
        if (match_ascii_ci(&probe, "ROLE") &&
            !is_ascii_identifier_char((unsigned char)*probe)) {
          return 1;
        }
      }
    }
    cursor++;
  }

  return 0;
}

static int accepts_flush_option_list_statement(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "FLUSH")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }

  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == ',') {
      cursor++;
      skip_ascii_space(&cursor);
      return *cursor != '\0' && *cursor != ';';
    }
    cursor++;
  }

  return 0;
}

static int accepts_select_where_having_without_from(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (*cursor != '(') {
    if (!match_ascii_ci(&cursor, "SELECT")) {
      return 0;
    }
    if (is_ascii_identifier_char((unsigned char)*cursor)) {
      return 0;
    }
  }

  int depth = 0;
  int saw_where = 0;
  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == '(') {
      depth++;
      cursor++;
      continue;
    }
    if (*cursor == ')') {
      if (depth > 0) {
        depth--;
      }
      cursor++;
      continue;
    }
    if (depth == 0 && !is_ascii_identifier_char((unsigned char)*(cursor - 1))) {
      const char *probe = cursor;
      if (!saw_where && match_ascii_ci(&probe, "FROM") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        return 0;
      }
      probe = cursor;
      if (!saw_where && match_ascii_ci(&probe, "WHERE") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        saw_where = 1;
        cursor = probe;
        continue;
      }
      probe = cursor;
      if (saw_where && match_ascii_ci(&probe, "HAVING") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        return 1;
      }
    }
    cursor++;
  }

  return 0;
}

static int accepts_select_from_dual_group_having(const char *sql) {
  const char *cursor = sql;
  int depth = 0;
  int saw_from_dual = 0;
  int saw_group = 0;
  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == '(') {
      depth++;
      cursor++;
      continue;
    }
    if (*cursor == ')') {
      if (depth > 0) {
        depth--;
      }
      cursor++;
      continue;
    }
    if ((cursor == sql || !is_ascii_identifier_char((unsigned char)*(cursor - 1)))) {
      const char *probe = cursor;
      if (!saw_from_dual && match_ascii_ci(&probe, "FROM") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        skip_ascii_space(&probe);
        if (match_ascii_ci(&probe, "DUAL") &&
            !is_ascii_identifier_char((unsigned char)*probe)) {
          saw_from_dual = 1;
          cursor = probe;
          continue;
        }
      }

      probe = cursor;
      if (saw_from_dual && !saw_group && match_ascii_ci(&probe, "GROUP") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        skip_ascii_space(&probe);
        if (match_ascii_ci(&probe, "BY") && !is_ascii_identifier_char((unsigned char)*probe)) {
          saw_group = 1;
          cursor = probe;
          continue;
        }
      }

      probe = cursor;
      if (saw_from_dual && match_ascii_ci(&probe, "HAVING") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        return 1;
      }
    }
    cursor++;
  }

  return 0;
}

static int accepts_select_group_having_without_from(const char *sql) {
  const char *cursor = sql;
  int saw_select = 0;
  int saw_group = 0;

  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (cursor == sql || !is_ascii_identifier_char((unsigned char)*(cursor - 1))) {
      const char *probe = cursor;
      if (match_ascii_ci(&probe, "SELECT") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        saw_select = 1;
        saw_group = 0;
        cursor = probe;
        continue;
      }

      probe = cursor;
      if (saw_select && match_ascii_ci(&probe, "FROM") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        saw_select = 0;
        saw_group = 0;
        cursor = probe;
        continue;
      }

      probe = cursor;
      if (saw_select && !saw_group && match_ascii_ci(&probe, "GROUP") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        skip_ascii_space(&probe);
        if (match_ascii_ci(&probe, "BY") && !is_ascii_identifier_char((unsigned char)*probe)) {
          saw_group = 1;
          cursor = probe;
          continue;
        }
      }

      probe = cursor;
      if (saw_select && saw_group && match_ascii_ci(&probe, "HAVING") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        return 1;
      }
    }
    cursor++;
  }

  return 0;
}

static int accepts_select_not_like_pipes_statement(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "SELECT")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }

  int pending_not = 0;
  int saw_left_not_like = 0;
  int saw_pipes = 0;
  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == '|' && *(cursor + 1) == '|') {
      if (saw_left_not_like) {
        saw_pipes = 1;
      }
      pending_not = 0;
      cursor += 2;
      continue;
    }
    if (!is_ascii_identifier_char((unsigned char)*cursor)) {
      cursor++;
      continue;
    }

    const char *probe = cursor;
    if (match_ascii_ci(&probe, "NOT") &&
        !is_ascii_identifier_char((unsigned char)*probe)) {
      pending_not = 1;
      cursor = probe;
      continue;
    }
    probe = cursor;
    if (pending_not && match_ascii_ci(&probe, "LIKE") &&
        !is_ascii_identifier_char((unsigned char)*probe)) {
      if (saw_pipes) {
        return 1;
      }
      saw_left_not_like = 1;
      pending_not = 0;
      cursor = probe;
      continue;
    }

    pending_not = 0;
    while (is_ascii_identifier_char((unsigned char)*cursor)) {
      cursor++;
    }
  }

  return 0;
}

static int accepts_analyze_histogram_table_list(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "ANALYZE")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "TABLE")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }

  int depth = 0;
  int saw_comma = 0;
  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == '(') {
      depth++;
      cursor++;
      continue;
    }
    if (*cursor == ')') {
      if (depth > 0) {
        depth--;
      }
      cursor++;
      continue;
    }
    if (depth == 0 && *cursor == ',') {
      saw_comma = 1;
      cursor++;
      continue;
    }
    if (depth == 0 && saw_comma &&
        !is_ascii_identifier_char((unsigned char)*(cursor - 1))) {
      const char *probe = cursor;
      if ((match_ascii_ci(&probe, "UPDATE") || match_ascii_ci(&probe, "DROP")) &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        skip_ascii_space(&probe);
        if (match_ascii_ci(&probe, "HISTOGRAM") &&
            !is_ascii_identifier_char((unsigned char)*probe)) {
          return 1;
        }
      }
    }
    cursor++;
  }

  return 0;
}

static int accepts_mysql_resource_group_statement(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "CREATE") && !match_ascii_ci(&cursor, "ALTER")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "RESOURCE")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (!match_ascii_ci(&cursor, "GROUP")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }

  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (cursor == sql || !is_ascii_identifier_char((unsigned char)*(cursor - 1))) {
      const char *probe = cursor;
      if ((match_ascii_ci(&probe, "TYPE") || match_ascii_ci(&probe, "VCPU") ||
           match_ascii_ci(&probe, "THREAD_PRIORITY")) &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        return 1;
      }
    }
    cursor++;
  }

  return 0;
}

static int accepts_create_procedure_with_characteristics(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "CREATE")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }
  skip_ascii_space(&cursor);
  if (!skip_create_procedure_head(&cursor)) {
    return 0;
  }

  while (*cursor != '\0' && *cursor != '(') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    cursor++;
  }
  if (*cursor != '(') {
    return 0;
  }

  int depth = 1;
  cursor++;
  while (*cursor != '\0' && depth > 0) {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == '(') {
      depth++;
    } else if (*cursor == ')') {
      depth--;
    }
    cursor++;
  }
  if (depth != 0) {
    return 0;
  }

  int saw_characteristic = 0;
  while (*cursor != '\0' && *cursor != ';') {
    skip_ascii_space(&cursor);
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (!is_ascii_identifier_char((unsigned char)*(cursor - 1))) {
      const char *probe = cursor;
      if ((match_ascii_ci(&probe, "BEGIN") || match_ascii_ci(&probe, "SET") ||
           match_ascii_ci(&probe, "SELECT") || match_ascii_ci(&probe, "INSERT") ||
           match_ascii_ci(&probe, "UPDATE") || match_ascii_ci(&probe, "DELETE") ||
           match_ascii_ci(&probe, "CALL") || match_ascii_ci(&probe, "DO") ||
           match_ascii_ci(&probe, "TRUNCATE")) &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        return saw_characteristic;
      }

      probe = cursor;
      if ((match_ascii_ci(&probe, "LANGUAGE") || match_ascii_ci(&probe, "MODIFIES") ||
           match_ascii_ci(&probe, "READS") || match_ascii_ci(&probe, "CONTAINS") ||
           match_ascii_ci(&probe, "NO") || match_ascii_ci(&probe, "DETERMINISTIC") ||
           match_ascii_ci(&probe, "NOT") || match_ascii_ci(&probe, "SQL") ||
           match_ascii_ci(&probe, "COMMENT")) &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        saw_characteristic = 1;
        cursor = probe;
        continue;
      }
    }
    cursor++;
  }

  return 0;
}

static int skip_create_procedure_head(const char **cursor) {
  const char *probe = *cursor;
  if (match_ascii_ci(&probe, "PROCEDURE") &&
      !is_ascii_identifier_char((unsigned char)*probe)) {
    *cursor = probe;
    return 1;
  }

  probe = *cursor;
  if (!match_ascii_ci(&probe, "DEFINER") ||
      is_ascii_identifier_char((unsigned char)*probe)) {
    return 0;
  }
  skip_ascii_space(&probe);
  if (*probe == '=') {
    probe++;
  }
  skip_ascii_space(&probe);
  if (!skip_definer_account(&probe)) {
    return 0;
  }
  skip_ascii_space(&probe);
  if (!match_ascii_ci(&probe, "PROCEDURE") ||
      is_ascii_identifier_char((unsigned char)*probe)) {
    return 0;
  }

  *cursor = probe;
  return 1;
}

static int skip_definer_account(const char **cursor) {
  const char *probe = *cursor;
  if (*probe == '\'' || *probe == '"' || *probe == '`') {
    skip_sql_quoted(&probe, (unsigned char)*probe);
  } else {
    if (!is_ascii_identifier_char((unsigned char)*probe)) {
      return 0;
    }
    while (is_ascii_identifier_char((unsigned char)*probe)) {
      probe++;
    }
    if (*probe == '(') {
      int depth = 1;
      probe++;
      while (*probe != '\0' && depth > 0) {
        if (*probe == '\'' || *probe == '"' || *probe == '`') {
          skip_sql_quoted(&probe, (unsigned char)*probe);
          continue;
        }
        if (*probe == '(') {
          depth++;
        } else if (*probe == ')') {
          depth--;
        }
        probe++;
      }
      if (depth != 0) {
        return 0;
      }
    }
  }

  if (*probe == '@') {
    probe++;
    if (*probe == '\'' || *probe == '"' || *probe == '`') {
      skip_sql_quoted(&probe, (unsigned char)*probe);
    } else {
      if (!is_ascii_identifier_char((unsigned char)*probe)) {
        return 0;
      }
      while (is_ascii_identifier_char((unsigned char)*probe) || *probe == '.' ||
             *probe == '%' || *probe == '-') {
        probe++;
      }
    }
  }

  *cursor = probe;
  return 1;
}

static int accepts_insert_alias_on_duplicate(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (!match_ascii_ci(&cursor, "INSERT")) {
    return 0;
  }
  if (is_ascii_identifier_char((unsigned char)*cursor)) {
    return 0;
  }

  int depth = 0;
  int saw_insert_source = 0;
  int saw_alias = 0;
  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == '(') {
      depth++;
      cursor++;
      continue;
    }
    if (*cursor == ')') {
      if (depth > 0) {
        depth--;
      }
      cursor++;
      continue;
    }
    if (depth == 0 && !is_ascii_identifier_char((unsigned char)*(cursor - 1))) {
      const char *probe = cursor;
      if (!saw_insert_source &&
          (match_ascii_ci(&probe, "VALUES") || match_ascii_ci(&probe, "SET")) &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        saw_insert_source = 1;
        cursor = probe;
        continue;
      }
      probe = cursor;
      if (saw_insert_source && !saw_alias && match_ascii_ci(&probe, "AS") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        saw_alias = 1;
        cursor = probe;
        continue;
      }
      probe = cursor;
      if (saw_insert_source && saw_alias && match_ascii_ci(&probe, "ON") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        skip_ascii_space(&probe);
        if (match_ascii_ci(&probe, "DUPLICATE") &&
            !is_ascii_identifier_char((unsigned char)*probe)) {
          skip_ascii_space(&probe);
          if (match_ascii_ci(&probe, "KEY") &&
              !is_ascii_identifier_char((unsigned char)*probe)) {
            return 1;
          }
        }
      }
    }
    cursor++;
  }

  return saw_insert_source && saw_alias;
}

static int accepts_set_select_into_before_lock(const char *sql) {
  const char *cursor = sql;
  skip_ascii_space(&cursor);

  if (*cursor != '(') {
    if (!match_ascii_ci(&cursor, "SELECT")) {
      return 0;
    }
    if (is_ascii_identifier_char((unsigned char)*cursor)) {
      return 0;
    }
  }

  int depth = 0;
  int saw_set_operator = 0;
  int saw_into = 0;
  while (*cursor != '\0' && *cursor != ';') {
    if (*cursor == '\'' || *cursor == '"' || *cursor == '`') {
      skip_sql_quoted(&cursor, (unsigned char)*cursor);
      continue;
    }
    if (*cursor == '(') {
      depth++;
      cursor++;
      continue;
    }
    if (*cursor == ')') {
      if (depth > 0) {
        depth--;
      }
      cursor++;
      continue;
    }
    if (!is_ascii_identifier_char((unsigned char)*(cursor - 1))) {
      const char *probe = cursor;
      if (!saw_set_operator &&
          (match_ascii_ci(&probe, "UNION") || match_ascii_ci(&probe, "EXCEPT") ||
           match_ascii_ci(&probe, "INTERSECT")) &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        saw_set_operator = 1;
        cursor = probe;
        continue;
      }

      probe = cursor;
      if (saw_set_operator && !saw_into && match_ascii_ci(&probe, "INTO") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        saw_into = 1;
        cursor = probe;
        continue;
      }

      probe = cursor;
      if (saw_set_operator && saw_into && match_ascii_ci(&probe, "FOR") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        skip_ascii_space(&probe);
        if ((match_ascii_ci(&probe, "UPDATE") || match_ascii_ci(&probe, "SHARE")) &&
            !is_ascii_identifier_char((unsigned char)*probe)) {
          return 1;
        }
      }

      probe = cursor;
      if (saw_set_operator && saw_into && match_ascii_ci(&probe, "LOCK") &&
          !is_ascii_identifier_char((unsigned char)*probe)) {
        skip_ascii_space(&probe);
        if (match_ascii_ci(&probe, "IN") && !is_ascii_identifier_char((unsigned char)*probe)) {
          skip_ascii_space(&probe);
          if (match_ascii_ci(&probe, "SHARE") &&
              !is_ascii_identifier_char((unsigned char)*probe)) {
            skip_ascii_space(&probe);
            if (match_ascii_ci(&probe, "MODE") &&
                !is_ascii_identifier_char((unsigned char)*probe)) {
              return 1;
            }
          }
        }
      }
    }
    cursor++;
  }

  return 0;
}

static int match_ascii_ci(const char **cursor, const char *expected) {
  const char *current = *cursor;
  while (*expected != '\0') {
    if (ascii_tolower((unsigned char)*current) !=
        ascii_tolower((unsigned char)*expected)) {
      return 0;
    }
    current++;
    expected++;
  }
  *cursor = current;
  return 1;
}

static void skip_sql_quoted(const char **cursor, int quote) {
  (*cursor)++;
  while (**cursor != '\0') {
    if (**cursor == '\\') {
      (*cursor)++;
      if (**cursor != '\0') {
        (*cursor)++;
      }
      continue;
    }
    if ((unsigned char)**cursor == quote) {
      (*cursor)++;
      if ((unsigned char)**cursor == quote) {
        (*cursor)++;
        continue;
      }
      return;
    }
    (*cursor)++;
  }
}

static void skip_ascii_space(const char **cursor) {
  while (**cursor == ' ' || **cursor == '\t' || **cursor == '\n' ||
         **cursor == '\r' || **cursor == '\f') {
    (*cursor)++;
  }
}

static int is_ascii_identifier_char(int ch) {
  return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
         (ch >= '0' && ch <= '9') || ch == '_';
}

static int ascii_tolower(int ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return ch + ('a' - 'A');
  }
  return ch;
}
