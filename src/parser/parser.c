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
  const char *schema_value;
  size_t schema_value_length;
  const char *name_value;
  size_t name_value_length;
} MyliteAstStatementTarget;

typedef enum MyliteAstCreateTableColumnTypeShapeFlag {
  MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_LENGTH = 1u << 0,
  MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_PRECISION = 1u << 1,
  MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_SCALE = 1u << 2,
  MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_FSP = 1u << 3
} MyliteAstCreateTableColumnTypeShapeFlag;

struct MyliteAstCreateTableColumnTypeElement {
  const char *value;
  size_t value_length;
  size_t start;
  size_t end;
  int token;
};

struct MyliteAstCreateTableColumn {
  unsigned long long type_length;
  unsigned long long type_precision;
  unsigned long long type_scale;
  unsigned long long type_fractional_seconds_precision;
  MyliteCreateTableColumnTypeFamily type_family;
  MyliteCreateTableColumnTypeKind type_kind;
  MyliteCreateTableColumnStorageClass storage_class;
  MyliteCreateTableColumnNullability nullability;
  MyliteCreateTableColumnGeneratedStorage generated_storage_kind;
  MyliteCreateTableColumnValueKind default_value_kind;
  MyliteCreateTableColumnValueKind on_update_value_kind;
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
  MyliteAstCreateTableColumnTypeElement *type_elements;
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
  const char *type_charset_value;
  size_t type_charset_value_length;
  size_t type_collation_start;
  size_t type_collation_end;
  size_t type_collation_value_start;
  size_t type_collation_value_end;
  const char *type_collation_value;
  size_t type_collation_value_length;
  size_t options_start;
  size_t options_end;
  const MyliteAstNode *options_node;
  size_t default_start;
  size_t default_end;
  const MyliteAstNode *default_node;
  size_t default_value_start;
  size_t default_value_end;
  const MyliteAstNode *default_value_node;
  const char *default_value;
  size_t default_value_length;
  unsigned long long default_unsigned_integer_value;
  int has_default_unsigned_integer_value;
  size_t on_update_start;
  size_t on_update_end;
  const MyliteAstNode *on_update_node;
  size_t on_update_value_start;
  size_t on_update_value_end;
  const MyliteAstNode *on_update_value_node;
  const char *on_update_value;
  size_t on_update_value_length;
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
  const char *comment_value;
  size_t comment_value_length;
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
};

struct MyliteAstCreateTableKeyPart {
  MyliteCreateTableKeyPartKind kind;
  MyliteCreateTableKeyPartOrder order;
  size_t start;
  size_t end;
  size_t name_start;
  size_t name_end;
  const char *name_value;
  size_t name_value_length;
  size_t expression_start;
  size_t expression_end;
  size_t prefix_start;
  size_t prefix_end;
  size_t prefix_value_start;
  size_t prefix_value_end;
  size_t order_start;
  size_t order_end;
};

struct MyliteAstCreateTableKeyOption {
  MyliteCreateTableKeyOptionKind kind;
  MyliteCreateTableKeyOptionValueKind value_kind;
  MyliteCreateTableIndexType index_type_kind;
  unsigned long long unsigned_integer_value;
  int has_unsigned_integer_value;
  size_t start;
  size_t end;
  size_t name_start;
  size_t name_end;
  size_t value_start;
  size_t value_end;
  const char *value;
  size_t value_length;
};

struct MyliteAstCreateTableKey {
  MyliteCreateTableKeyKind kind;
  MyliteCreateTableIndexType index_type_kind;
  MyliteCreateTableKeyVisibility visibility;
  size_t start;
  size_t end;
  size_t constraint_name_start;
  size_t constraint_name_end;
  const char *constraint_name_value;
  size_t constraint_name_value_length;
  size_t name_start;
  size_t name_end;
  const char *name_value;
  size_t name_value_length;
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
  const char *referenced_table_schema_value;
  size_t referenced_table_schema_value_length;
  const char *referenced_table_name_value;
  size_t referenced_table_name_value_length;
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
  const MyliteAstCreateTableKeyOption *comment_option;
  const MyliteAstCreateTableKeyOption *parser_option;
  const MyliteAstCreateTableKeyOption *key_block_size_option;
};

struct MyliteAstCreateTableOption {
  MyliteCreateTableOptionKind kind;
  MyliteCreateTableOptionValueKind value_kind;
  unsigned long long unsigned_integer_value;
  int has_unsigned_integer_value;
  size_t start;
  size_t end;
  size_t name_start;
  size_t name_end;
  size_t value_start;
  size_t value_end;
  const char *value;
  size_t value_length;
};

struct MyliteAstCreateTable {
  const MyliteAstNode *node;
  size_t start;
  size_t end;
  size_t target_start;
  size_t target_end;
  size_t schema_start;
  size_t schema_end;
  const char *schema_value;
  size_t schema_value_length;
  size_t name_start;
  size_t name_end;
  const char *name_value;
  size_t name_value_length;
  const MyliteAstCreateTableColumn *columns;
  size_t column_count;
  const MyliteAstCreateTableKey *keys;
  size_t key_count;
  const MyliteAstCreateTableOption *options;
  size_t option_count;
  const MyliteAstCreateTableOption *engine_option;
  const MyliteAstCreateTableOption *charset_option;
  const MyliteAstCreateTableOption *collation_option;
  const MyliteAstCreateTableOption *comment_option;
  const MyliteAstCreateTableOption *auto_increment_option;
};

struct MyliteAstDatabaseOption {
  const MyliteAstNode *node;
  MyliteDatabaseOptionKind kind;
  MyliteDatabaseOptionValueKind value_kind;
  size_t start;
  size_t end;
  size_t name_start;
  size_t name_end;
  size_t value_start;
  size_t value_end;
  const char *value;
  size_t value_length;
};

struct MyliteAstCreateDatabase {
  const MyliteAstNode *node;
  const MyliteAstStatementTarget *target;
  MyliteAstDatabaseOption *options;
  size_t option_count;
  const MyliteAstDatabaseOption *charset_option;
  const MyliteAstDatabaseOption *collation_option;
  const MyliteAstDatabaseOption *encryption_option;
  size_t start;
  size_t end;
  int has_if_not_exists;
  int uses_schema_keyword;
};

struct MyliteAstViewColumn {
  size_t start;
  size_t end;
  const char *name_value;
  size_t name_value_length;
};

struct MyliteAstCreateView {
  const MyliteAstNode *node;
  const MyliteAstStatementTarget *target;
  MyliteAstViewColumn *columns;
  size_t column_count;
  const MyliteAstNode *select_node;
  MyliteCreateViewAlgorithm algorithm;
  MyliteViewSqlSecurity sql_security;
  MyliteViewCheckOption check_option;
  size_t start;
  size_t end;
  size_t definer_start;
  size_t definer_end;
  size_t select_start;
  size_t select_end;
  int has_or_replace;
};

struct MyliteAstAlterTableSpec {
  const MyliteAstNode *node;
  MyliteAstCreateTableColumn column;
  MyliteAstCreateTableKey key;
  MyliteAstCreateTableColumn *columns;
  size_t column_count;
  MyliteAstCreateTableKey *keys;
  size_t key_count;
  MyliteAlterTableSpecKind kind;
  size_t start;
  size_t end;
  size_t name_start;
  size_t name_end;
  const char *name_value;
  size_t name_value_length;
  size_t secondary_name_start;
  size_t secondary_name_end;
  const char *secondary_name_value;
  size_t secondary_name_value_length;
  size_t table_start;
  size_t table_end;
  size_t table_schema_start;
  size_t table_schema_end;
  const char *table_schema_value;
  size_t table_schema_value_length;
  size_t table_name_start;
  size_t table_name_end;
  const char *table_name_value;
  size_t table_name_value_length;
  int has_if_exists;
  int has_if_not_exists;
  int has_column;
  int has_key;
};

struct MyliteAstAlterTable {
  const MyliteAstNode *node;
  const MyliteAstStatementTarget *target;
  MyliteAstAlterTableSpec *specs;
  size_t spec_count;
  MyliteAstCreateTableOption *options;
  size_t option_count;
  size_t start;
  size_t end;
};

struct MyliteAstCreateIndex {
  const MyliteAstNode *node;
  const MyliteAstStatementTarget *target;
  MyliteAstCreateTableKey key;
  size_t start;
  size_t end;
};

struct MyliteAstDropIndex {
  const MyliteAstNode *node;
  const MyliteAstStatementTarget *target;
  const char *name_value;
  size_t name_value_length;
  size_t start;
  size_t end;
  size_t name_start;
  size_t name_end;
  int has_if_exists;
  int is_hypothetical;
};

struct MyliteAstDropDatabase {
  const MyliteAstNode *node;
  const MyliteAstStatementTarget *target;
  size_t start;
  size_t end;
  int has_if_exists;
  int uses_schema_keyword;
};

struct MyliteAstDropTable {
  const MyliteAstNode *node;
  const MyliteAstStatementTarget *targets;
  size_t target_count;
  size_t start;
  size_t end;
  int is_temporary;
  int has_if_exists;
};

struct MyliteAstDropView {
  const MyliteAstNode *node;
  const MyliteAstStatementTarget *targets;
  size_t target_count;
  MyliteDropViewMode mode;
  size_t start;
  size_t end;
  int has_if_exists;
};

struct MyliteAstExpression {
  const MyliteAstNode *node;
  MyliteExpressionKind kind;
  MyliteExpressionLiteralKind literal_kind;
  size_t start;
  size_t end;
  size_t value_start;
  size_t value_end;
  const char *value;
  size_t value_length;
  unsigned long long unsigned_integer_value;
  int has_unsigned_integer_value;
};

struct MyliteAstSetAssignment {
  const MyliteAstNode *node;
  const MyliteAstNode *value_node;
  const MyliteAstNode *extend_value_node;
  MyliteAstExpression value_expression;
  MyliteSetAssignmentKind kind;
  MyliteSetVariableScope scope;
  MyliteSetAssignmentOperator operator_kind;
  size_t start;
  size_t end;
  size_t name_start;
  size_t name_end;
  size_t value_start;
  size_t value_end;
  size_t extend_value_start;
  size_t extend_value_end;
  const char *name_value;
  size_t name_value_length;
};

struct MyliteAstSetStatement {
  const MyliteAstNode *node;
  MyliteAstSetAssignment *assignments;
  size_t assignment_count;
  MyliteSetStatementForm form;
  size_t start;
  size_t end;
};

struct MyliteAstRenameTable {
  const MyliteAstNode *node;
  const MyliteAstStatementTarget *targets;
  size_t target_count;
  size_t start;
  size_t end;
};

struct MyliteAstTruncateTable {
  const MyliteAstNode *node;
  const MyliteAstStatementTarget *target;
  size_t start;
  size_t end;
  int has_table_keyword;
};

struct MyliteAstUseDatabase {
  const MyliteAstNode *node;
  const MyliteAstStatementTarget *target;
  size_t start;
  size_t end;
};

struct MyliteAstPreparedStatementVariable {
  const MyliteAstNode *node;
  size_t start;
  size_t end;
  size_t name_start;
  size_t name_end;
  const char *name_value;
  size_t name_value_length;
};

struct MyliteAstPrepareStatement {
  const MyliteAstNode *node;
  const MyliteAstNode *source_node;
  MylitePrepareStatementSourceKind source_kind;
  size_t start;
  size_t end;
  size_t name_start;
  size_t name_end;
  const char *name_value;
  size_t name_value_length;
  size_t source_start;
  size_t source_end;
  const char *source_value;
  size_t source_value_length;
};

struct MyliteAstExecuteStatement {
  const MyliteAstNode *node;
  MyliteAstPreparedStatementVariable *using_variables;
  size_t using_count;
  size_t start;
  size_t end;
  size_t name_start;
  size_t name_end;
  const char *name_value;
  size_t name_value_length;
};

struct MyliteAstDeallocateStatement {
  const MyliteAstNode *node;
  MyliteDeallocateStatementMode mode;
  size_t start;
  size_t end;
  size_t name_start;
  size_t name_end;
  const char *name_value;
  size_t name_value_length;
};

struct MyliteAstTransactionStatement {
  const MyliteAstNode *node;
  MyliteTransactionStatementKind kind;
  MyliteTransactionBeginForm begin_form;
  MyliteTransactionBeginMode begin_mode;
  MyliteTransactionAccessMode access_mode;
  size_t start;
  size_t end;
  size_t savepoint_name_start;
  size_t savepoint_name_end;
  const char *savepoint_name_value;
  size_t savepoint_name_value_length;
  int has_consistent_snapshot;
  int has_causal_consistency;
  int has_work_keyword;
  int has_chain;
  int has_no_chain;
  int has_release;
  int has_no_release;
  int has_savepoint_keyword;
};

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
  MyliteAstCreateTable *create_table;
  MyliteAstCreateDatabase *create_database;
  MyliteAstAlterTable *alter_table;
  MyliteAstCreateIndex *create_index;
  MyliteAstCreateView *create_view;
  MyliteAstDropDatabase *drop_database;
  MyliteAstDropIndex *drop_index;
  MyliteAstDropTable *drop_table;
  MyliteAstDropView *drop_view;
  MyliteAstPrepareStatement *prepare_statement;
  MyliteAstExecuteStatement *execute_statement;
  MyliteAstDeallocateStatement *deallocate_statement;
  MyliteAstRenameTable *rename_table;
  MyliteAstSetStatement *set_statement;
  MyliteAstTruncateTable *truncate_table;
  MyliteAstTransactionStatement *transaction_statement;
  MyliteAstUseDatabase *use_database;
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
static int mylite_ast_set_create_database_view(MyliteAst *ast,
                                               MyliteAstStatement *statement,
                                               const MyliteAstNode *payload);
static void mylite_ast_set_database_option_summary(
    MyliteAstCreateDatabase *create_database);
static int mylite_ast_set_create_table_view(MyliteAst *ast,
                                            MyliteAstStatement *statement,
                                            const MyliteAstNode *payload);
static void mylite_ast_set_create_table_option_summary(
    MyliteAstCreateTable *create_table);
static const MyliteAstStatementTarget *mylite_ast_create_table_target(
    const MyliteAstStatement *statement);
static int mylite_ast_set_alter_table_view(MyliteAst *ast,
                                           MyliteAstStatement *statement,
                                           const MyliteAstNode *payload);
static const MyliteAstStatementTarget *mylite_ast_alter_table_target(
    const MyliteAstStatement *statement);
static size_t mylite_ast_count_alter_table_specs(const MyliteAstNode *node);
static void mylite_ast_fill_alter_table_specs(MyliteAst *ast,
                                              MyliteAstAlterTable *alter_table,
                                              const MyliteAstNode *node,
                                              size_t *index, int *ok);
static int mylite_ast_fill_alter_table_spec(MyliteAst *ast,
                                            MyliteAstAlterTableSpec *spec,
                                            const MyliteAstNode *node);
static int mylite_ast_set_alter_table_spec_payload(
    MyliteAst *ast, MyliteAstAlterTableSpec *spec, const MyliteAstNode *node);
static int mylite_ast_set_alter_table_spec_column(
    MyliteAst *ast, MyliteAstAlterTableSpec *spec, const MyliteAstNode *node);
static int mylite_ast_set_alter_table_spec_key(
    MyliteAst *ast, MyliteAstAlterTableSpec *spec, const MyliteAstNode *node);
static int mylite_ast_set_alter_table_spec_table_elements(
    MyliteAst *ast, MyliteAstAlterTableSpec *spec, const MyliteAstNode *node);
static MyliteAlterTableSpecKind mylite_ast_classify_alter_table_spec(
    const MyliteAstNode *node);
static void mylite_ast_set_alter_table_spec_spans(
    MyliteAstAlterTableSpec *spec, const MyliteAstNode *node);
static int mylite_ast_set_alter_table_spec_values(
    MyliteAst *ast, MyliteAstAlterTableSpec *spec);
static void mylite_ast_set_alter_table_spec_name(
    MyliteAstAlterTableSpec *spec, const MyliteAstNode *node);
static void mylite_ast_set_alter_table_spec_secondary_name(
    MyliteAstAlterTableSpec *spec, const MyliteAstNode *node);
static void mylite_ast_set_alter_table_spec_table(
    MyliteAstAlterTableSpec *spec, const MyliteAstNode *node);
static const MyliteAstNode *mylite_ast_find_identifier_for_name(
    const MyliteAstNode *node);
static int mylite_ast_set_alter_table_options(MyliteAst *ast,
                                              MyliteAstAlterTable *alter_table,
                                              const MyliteAstNode *payload);
static void mylite_ast_fill_alter_table_options(
    MyliteAst *ast, MyliteAstAlterTable *alter_table,
    const MyliteAstNode *node, size_t *index, int *ok);
static int mylite_ast_set_create_index_view(MyliteAst *ast,
                                            MyliteAstStatement *statement,
                                            const MyliteAstNode *payload);
static int mylite_ast_set_create_index_key(MyliteAst *ast,
                                           MyliteAstCreateTableKey *key,
                                           const MyliteAstNode *payload);
static int mylite_ast_set_create_view_view(MyliteAst *ast,
                                           MyliteAstStatement *statement,
                                           const MyliteAstNode *payload);
static int mylite_ast_set_create_view_columns(MyliteAst *ast,
                                              MyliteAstCreateView *create_view,
                                              const MyliteAstNode *payload);
static size_t mylite_ast_count_view_columns(const MyliteAstNode *node);
static void mylite_ast_fill_view_columns(MyliteAst *ast,
                                         MyliteAstViewColumn *columns,
                                         size_t column_count,
                                         const MyliteAstNode *node,
                                         size_t *index, int *ok);
static int mylite_ast_fill_view_column(MyliteAst *ast,
                                       MyliteAstViewColumn *column,
                                       const MyliteAstNode *node);
static MyliteCreateViewAlgorithm mylite_ast_classify_create_view_algorithm(
    const MyliteAstNode *node);
static MyliteViewSqlSecurity mylite_ast_classify_view_sql_security(
    const MyliteAstNode *node);
static MyliteViewCheckOption mylite_ast_classify_view_check_option(
    const MyliteAstNode *node);
static int mylite_ast_set_drop_index_view(MyliteAst *ast,
                                          MyliteAstStatement *statement,
                                          const MyliteAstNode *payload);
static int mylite_ast_set_drop_database_view(MyliteAst *ast,
                                             MyliteAstStatement *statement,
                                             const MyliteAstNode *payload);
static int mylite_ast_set_drop_table_view(MyliteAst *ast,
                                          MyliteAstStatement *statement,
                                          const MyliteAstNode *payload);
static int mylite_ast_set_drop_view_view(MyliteAst *ast,
                                         MyliteAstStatement *statement,
                                         const MyliteAstNode *payload);
static MyliteDropViewMode mylite_ast_classify_drop_view_mode(
    const MyliteAstNode *node);
static int mylite_ast_set_prepare_statement_view(MyliteAst *ast,
                                                 MyliteAstStatement *statement,
                                                 const MyliteAstNode *payload);
static int mylite_ast_set_prepare_statement_source(
    MyliteAst *ast, MyliteAstPrepareStatement *prepare_statement,
    const MyliteAstNode *node);
static int mylite_ast_set_execute_statement_view(MyliteAst *ast,
                                                 MyliteAstStatement *statement,
                                                 const MyliteAstNode *payload);
static int mylite_ast_set_execute_statement_variables(
    MyliteAst *ast, MyliteAstExecuteStatement *execute_statement,
    const MyliteAstNode *node);
static size_t mylite_ast_count_user_variables(const MyliteAstNode *node);
static void mylite_ast_fill_user_variables(
    MyliteAst *ast, MyliteAstPreparedStatementVariable *variables,
    size_t variable_count, const MyliteAstNode *node, size_t *index, int *ok);
static int mylite_ast_fill_user_variable(
    MyliteAst *ast, MyliteAstPreparedStatementVariable *variable,
    const MyliteAstNode *node);
static int mylite_ast_set_deallocate_statement_view(
    MyliteAst *ast, MyliteAstStatement *statement,
    const MyliteAstNode *payload);
static MyliteDeallocateStatementMode mylite_ast_classify_deallocate_statement_mode(
    const MyliteAstNode *node);
static int mylite_ast_set_prepared_statement_name_value(
    MyliteAst *ast, const MyliteAstNode *node, size_t *name_start,
    size_t *name_end, const char **name_value, size_t *name_value_length);
static int mylite_ast_set_user_variable_name_value(
    MyliteAst *ast, const MyliteAstNode *node, size_t *name_start,
    size_t *name_end, const char **name_value, size_t *name_value_length);
static int mylite_ast_set_transaction_statement_view(
    MyliteAst *ast, MyliteAstStatement *statement,
    const MyliteAstNode *payload);
static MyliteTransactionStatementKind mylite_ast_classify_transaction_statement(
    const char *symbol_name);
static void mylite_ast_set_transaction_begin_details(
    MyliteAstTransactionStatement *transaction_statement,
    const MyliteAstNode *node);
static void mylite_ast_set_transaction_completion_details(
    MyliteAstTransactionStatement *transaction_statement,
    const MyliteAstNode *node);
static int mylite_ast_set_transaction_savepoint_name(
    MyliteAst *ast, MyliteAstTransactionStatement *transaction_statement,
    const MyliteAstNode *node);
static int mylite_ast_set_set_statement_view(MyliteAst *ast,
                                             MyliteAstStatement *statement,
                                             const MyliteAstNode *payload);
static int mylite_ast_set_set_assignments(MyliteAst *ast,
                                          MyliteAstSetStatement *set_statement,
                                          const MyliteAstNode *payload);
static int mylite_ast_set_expression_summary(MyliteAst *ast,
                                             MyliteAstExpression *expression,
                                             const MyliteAstNode *node);
static const MyliteAstNode *mylite_ast_expression_payload(
    const MyliteAstNode *node);
static MyliteExpressionKind mylite_ast_classify_expression(
    const MyliteAstNode *node);
static MyliteExpressionLiteralKind mylite_ast_expression_literal_kind(
    const MyliteAstNode *node);
static const MyliteAstNode *mylite_ast_expression_value_node(
    const MyliteAstNode *node, MyliteExpressionKind kind,
    MyliteExpressionLiteralKind literal_kind);
static int mylite_ast_set_expression_value(MyliteAst *ast,
                                           MyliteAstExpression *expression,
                                           const MyliteAstNode *value_node);
static size_t mylite_ast_count_set_assignments(const MyliteAstNode *node);
static void mylite_ast_fill_set_assignments(
    MyliteAst *ast, MyliteAstSetStatement *set_statement,
    const MyliteAstNode *node, size_t *index, int *ok);
static int mylite_ast_fill_set_assignment(MyliteAst *ast,
                                          MyliteAstSetStatement *set_statement,
                                          MyliteAstSetAssignment *assignment,
                                          const MyliteAstNode *node);
static int mylite_ast_fill_set_variable_assignment(
    MyliteAst *ast, MyliteAstSetAssignment *assignment,
    const MyliteAstNode *node);
static int mylite_ast_fill_transaction_characteristic_assignment(
    MyliteAst *ast, MyliteAstSetStatement *set_statement,
    MyliteAstSetAssignment *assignment, const MyliteAstNode *node);
static int mylite_ast_fill_config_assignment(MyliteAst *ast,
                                             MyliteAstSetAssignment *assignment,
                                             const MyliteAstNode *node);
static MyliteSetStatementForm mylite_ast_classify_set_statement_form(
    const char *symbol_name, const MyliteAstNode *node);
static MyliteSetVariableScope mylite_ast_set_statement_scope(
    const MyliteAstNode *node);
static MyliteSetVariableScope mylite_ast_set_assignment_prefix_scope(
    const MyliteAstNode *node);
static MyliteSetAssignmentKind mylite_ast_classify_set_variable_assignment(
    const MyliteAstNode *node);
static MyliteSetAssignmentOperator mylite_ast_set_assignment_operator(
    const MyliteAstNode *node);
static const MyliteAstNode *mylite_ast_set_assignment_name_node(
    const MyliteAstNode *node);
static const MyliteAstNode *mylite_ast_set_assignment_value_node(
    const MyliteAstNode *node);
static const MyliteAstNode *mylite_ast_set_names_extend_value_node(
    const MyliteAstNode *node);
static int mylite_ast_set_assignment_name_value(
    MyliteAst *ast, MyliteAstSetAssignment *assignment,
    const MyliteAstNode *node);
static int mylite_ast_set_assignment_constant_name(
    MyliteAstSetAssignment *assignment, const char *name);
static int mylite_ast_set_variable_name_value(MyliteAst *ast,
                                              MyliteAstSetAssignment *assignment,
                                              const MyliteAstNode *node);
static int mylite_ast_set_at_variable_name_value(
    MyliteAst *ast, MyliteAstSetAssignment *assignment,
    const MyliteAstNode *node);
static int mylite_ast_set_rename_table_view(MyliteAst *ast,
                                            MyliteAstStatement *statement,
                                            const MyliteAstNode *payload);
static int mylite_ast_set_truncate_table_view(MyliteAst *ast,
                                              MyliteAstStatement *statement,
                                              const MyliteAstNode *payload);
static int mylite_ast_set_use_database_view(MyliteAst *ast,
                                            MyliteAstStatement *statement,
                                            const MyliteAstNode *payload);
static MyliteStatementTargetKind mylite_ast_target_kind_for_statement(
    MyliteStatementKind kind, const char *symbol_name);
static int mylite_ast_collect_statement_targets(MyliteAst *ast,
                                                MyliteAstStatement *statement,
                                                const MyliteAstNode *payload);
static int mylite_ast_collect_set_targets(MyliteAst *ast,
                                          MyliteAstStatement *statement,
                                          const MyliteAstNode *node);
static const MyliteAstNode *mylite_ast_set_target_node(
    const MyliteAstNode *node);
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
static int mylite_ast_fill_statement_target(MyliteAst *ast,
                                            MyliteAstStatementTarget *target,
                                            MyliteStatementTargetKind kind,
                                            MyliteStatementTargetRole role,
                                            const MyliteAstNode *node);
static int mylite_ast_set_statement_target_values(MyliteAst *ast,
                                                  MyliteAstStatementTarget *target);
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
static int mylite_ast_set_create_table_column_metadata(
    MyliteAst *ast, MyliteAstCreateTableColumn *column);
static void mylite_ast_set_create_table_column_nullability(
    MyliteAstCreateTableColumn *column);
static void mylite_ast_set_create_table_column_generated_storage(
    MyliteAstCreateTableColumn *column);
static int mylite_ast_set_create_table_column_type_value_metadata(
    MyliteAst *ast, MyliteAstCreateTableColumn *column);
static int mylite_ast_set_create_table_column_default_value_metadata(
    MyliteAst *ast, MyliteAstCreateTableColumn *column);
static int mylite_ast_set_create_table_column_on_update_value_metadata(
    MyliteAst *ast, MyliteAstCreateTableColumn *column);
static int mylite_ast_set_create_table_column_comment_value_metadata(
    MyliteAst *ast, MyliteAstCreateTableColumn *column);
static int mylite_ast_set_create_table_column_value_metadata(
    MyliteAst *ast, MyliteCreateTableColumnValueKind *kind,
    const char **value, size_t *value_length,
    unsigned long long *unsigned_integer_value,
    int *has_unsigned_integer_value, const MyliteAstNode *node, size_t start,
    size_t end);
static const MyliteAstNode *mylite_ast_find_value_token_in_span(
    const MyliteAstNode *node, size_t start, size_t end);
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
    MyliteAstCreateTableColumnTypeElement *elements, const MyliteAstNode *node,
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
static int mylite_ast_copy_source_span(MyliteAst *ast, size_t start,
                                       size_t end, const char **value,
                                       size_t *value_length);
static int mylite_ast_ascii_case_equal_span(const char *value, size_t length,
                                            const char *expected);
static int mylite_ast_ascii_case_has_prefix(const char *value, size_t length,
                                            const char *prefix);
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
static void mylite_ast_set_create_table_key_summary(
    MyliteAstCreateTableKey *key);
static MyliteCreateTableKeyKind mylite_ast_classify_create_table_key(
    const MyliteAstNode *constraint_elem);
static void mylite_ast_set_create_table_key_names(MyliteAstCreateTableKey *key,
                                                  const MyliteAstNode *constraint);
static int mylite_ast_set_create_table_key_name_values(
    MyliteAst *ast, MyliteAstCreateTableKey *key);
static void mylite_ast_set_create_table_key_index_type(
    MyliteAstCreateTableKey *key, const MyliteAstNode *constraint_elem);
static MyliteCreateTableIndexType mylite_ast_classify_index_type(
    const MyliteAstNode *node);
static int mylite_ast_set_create_table_key_reference(
    MyliteAst *ast, MyliteAstCreateTableKey *key,
    const MyliteAstNode *constraint_elem);
static int mylite_ast_set_create_table_key_referenced_table_values(
    MyliteAst *ast, MyliteAstCreateTableKey *key);
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
static int mylite_ast_set_create_table_key_part_name_values(
    MyliteAst *ast, MyliteAstCreateTableKeyPart *parts, size_t count);
static void mylite_ast_fill_key_part(MyliteAstCreateTableKeyPart *part,
                                     const MyliteAstNode *node);
static size_t mylite_ast_count_index_options(const MyliteAstNode *node);
static void mylite_ast_fill_index_options(MyliteAstCreateTableKeyOption *options,
                                          size_t count,
                                          const MyliteAstNode *node,
                                          size_t *index, int *ok, MyliteAst *ast);
static int mylite_ast_fill_key_option(MyliteAst *ast,
                                      MyliteAstCreateTableKeyOption *option,
                                      const MyliteAstNode *node);
static int mylite_ast_set_key_option_value_metadata(
    MyliteAst *ast, MyliteAstCreateTableKeyOption *option,
    const MyliteAstNode *node);
static const MyliteAstNode *mylite_ast_find_key_option_value_token(
    const MyliteAstNode *node, size_t min_start);
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
static int mylite_ast_set_database_options(MyliteAst *ast,
                                           MyliteAstCreateDatabase *create_database,
                                           const MyliteAstNode *payload);
static size_t mylite_ast_count_database_options(const MyliteAstNode *node);
static void mylite_ast_fill_database_options(
    MyliteAst *ast, MyliteAstDatabaseOption *options, size_t option_count,
    const MyliteAstNode *node, size_t *index, int *ok);
static int mylite_ast_fill_database_option(
    MyliteAst *ast, MyliteAstDatabaseOption *option,
    const MyliteAstNode *node);
static MyliteDatabaseOptionKind mylite_ast_classify_database_option(
    const MyliteAstNode *node);
static void mylite_ast_set_database_option_name(
    MyliteAstDatabaseOption *option, const MyliteAstNode *node);
static void mylite_ast_set_database_option_value(
    MyliteAstDatabaseOption *option, const MyliteAstNode *node);
static int mylite_ast_set_database_option_value_metadata(
    MyliteAst *ast, MyliteAstDatabaseOption *option,
    const MyliteAstNode *node);
static const MyliteAstNode *mylite_ast_find_database_option_value_token(
    const MyliteAstNode *node, size_t min_start);
static const MyliteAstNode *mylite_ast_find_database_option_name(
    const MyliteAstNode *node, MyliteDatabaseOptionKind kind);
static size_t mylite_ast_count_create_table_options(const MyliteAstNode *node);
static void mylite_ast_fill_create_table_options(
    MyliteAst *ast, MyliteAstStatement *statement, const MyliteAstNode *node,
    size_t *index, int *ok);
static int mylite_ast_fill_create_table_option(
    MyliteAst *ast, MyliteAstCreateTableOption *option,
    const MyliteAstNode *node);
static MyliteCreateTableOptionKind mylite_ast_classify_create_table_option(
    const MyliteAstNode *node);
static void mylite_ast_set_create_table_option_name(
    MyliteAstCreateTableOption *option, const MyliteAstNode *node);
static void mylite_ast_set_create_table_option_value(
    MyliteAstCreateTableOption *option, const MyliteAstNode *node);
static int mylite_ast_set_create_table_option_value_metadata(
    MyliteAst *ast, MyliteAstCreateTableOption *option,
    const MyliteAstNode *node);
static const MyliteAstNode *mylite_ast_find_create_table_option_value_token(
    const MyliteAstNode *node, size_t min_start);
static int mylite_ast_is_unsigned_integer_table_option(
    MyliteCreateTableOptionKind kind);
static int mylite_ast_parse_unsigned_integer_value(const char *source,
                                                   size_t start, size_t end,
                                                   unsigned long long *value);
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
static const MyliteAstCreateTableColumnTypeElement *
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
    case MYLITE_STATEMENT_USE:
      return "use";
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

const char *mylite_expression_kind_name(MyliteExpressionKind kind) {
  switch (kind) {
    case MYLITE_EXPRESSION_UNKNOWN:
      return "unknown";
    case MYLITE_EXPRESSION_RAW:
      return "raw";
    case MYLITE_EXPRESSION_LITERAL:
      return "literal";
    case MYLITE_EXPRESSION_IDENTIFIER:
      return "identifier";
    case MYLITE_EXPRESSION_VARIABLE:
      return "variable";
    case MYLITE_EXPRESSION_FUNCTION_CALL:
      return "function_call";
    case MYLITE_EXPRESSION_DEFAULT:
      return "default";
  }
  return "unknown";
}

const char *mylite_expression_literal_kind_name(
    MyliteExpressionLiteralKind kind) {
  switch (kind) {
    case MYLITE_EXPRESSION_LITERAL_NONE:
      return "none";
    case MYLITE_EXPRESSION_LITERAL_STRING:
      return "string";
    case MYLITE_EXPRESSION_LITERAL_UNSIGNED_INTEGER:
      return "unsigned_integer";
    case MYLITE_EXPRESSION_LITERAL_FLOAT:
      return "float";
    case MYLITE_EXPRESSION_LITERAL_HEX:
      return "hex";
    case MYLITE_EXPRESSION_LITERAL_BIT:
      return "bit";
    case MYLITE_EXPRESSION_LITERAL_NULL:
      return "null";
    case MYLITE_EXPRESSION_LITERAL_TRUE:
      return "true";
    case MYLITE_EXPRESSION_LITERAL_FALSE:
      return "false";
  }
  return "none";
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

const char *mylite_create_table_column_nullability_name(
    MyliteCreateTableColumnNullability nullability) {
  switch (nullability) {
    case MYLITE_CREATE_TABLE_COLUMN_NULLABILITY_UNSPECIFIED:
      return "unspecified";
    case MYLITE_CREATE_TABLE_COLUMN_NULLABILITY_NULL:
      return "null";
    case MYLITE_CREATE_TABLE_COLUMN_NULLABILITY_NOT_NULL:
      return "not_null";
  }
  return "unspecified";
}

const char *mylite_create_table_column_generated_storage_name(
    MyliteCreateTableColumnGeneratedStorage storage) {
  switch (storage) {
    case MYLITE_CREATE_TABLE_COLUMN_GENERATED_STORAGE_UNSPECIFIED:
      return "unspecified";
    case MYLITE_CREATE_TABLE_COLUMN_GENERATED_STORAGE_VIRTUAL:
      return "virtual";
    case MYLITE_CREATE_TABLE_COLUMN_GENERATED_STORAGE_STORED:
      return "stored";
  }
  return "unspecified";
}

const char *mylite_create_table_column_value_kind_name(
    MyliteCreateTableColumnValueKind kind) {
  switch (kind) {
    case MYLITE_CREATE_TABLE_COLUMN_VALUE_UNKNOWN:
      return "unknown";
    case MYLITE_CREATE_TABLE_COLUMN_VALUE_RAW:
      return "raw";
    case MYLITE_CREATE_TABLE_COLUMN_VALUE_STRING:
      return "string";
    case MYLITE_CREATE_TABLE_COLUMN_VALUE_UNSIGNED_INTEGER:
      return "unsigned_integer";
    case MYLITE_CREATE_TABLE_COLUMN_VALUE_NULL:
      return "null";
    case MYLITE_CREATE_TABLE_COLUMN_VALUE_CURRENT_TIMESTAMP:
      return "current_timestamp";
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

const char *mylite_create_table_index_type_name(
    MyliteCreateTableIndexType type) {
  switch (type) {
    case MYLITE_CREATE_TABLE_INDEX_TYPE_UNSPECIFIED:
      return "unspecified";
    case MYLITE_CREATE_TABLE_INDEX_TYPE_BTREE:
      return "btree";
    case MYLITE_CREATE_TABLE_INDEX_TYPE_HASH:
      return "hash";
    case MYLITE_CREATE_TABLE_INDEX_TYPE_RTREE:
      return "rtree";
  }
  return "unspecified";
}

const char *mylite_create_table_key_visibility_name(
    MyliteCreateTableKeyVisibility visibility) {
  switch (visibility) {
    case MYLITE_CREATE_TABLE_KEY_VISIBILITY_UNSPECIFIED:
      return "unspecified";
    case MYLITE_CREATE_TABLE_KEY_VISIBILITY_VISIBLE:
      return "visible";
    case MYLITE_CREATE_TABLE_KEY_VISIBILITY_INVISIBLE:
      return "invisible";
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

const char *mylite_create_table_key_option_value_kind_name(
    MyliteCreateTableKeyOptionValueKind kind) {
  switch (kind) {
    case MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_UNKNOWN:
      return "unknown";
    case MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_RAW:
      return "raw";
    case MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_IDENTIFIER:
      return "identifier";
    case MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_STRING:
      return "string";
    case MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_UNSIGNED_INTEGER:
      return "unsigned_integer";
    case MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_INDEX_TYPE:
      return "index_type";
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

const char *mylite_create_table_option_value_kind_name(
    MyliteCreateTableOptionValueKind kind) {
  switch (kind) {
    case MYLITE_CREATE_TABLE_OPTION_VALUE_UNKNOWN:
      return "unknown";
    case MYLITE_CREATE_TABLE_OPTION_VALUE_RAW:
      return "raw";
    case MYLITE_CREATE_TABLE_OPTION_VALUE_IDENTIFIER:
      return "identifier";
    case MYLITE_CREATE_TABLE_OPTION_VALUE_STRING:
      return "string";
    case MYLITE_CREATE_TABLE_OPTION_VALUE_UNSIGNED_INTEGER:
      return "unsigned_integer";
    case MYLITE_CREATE_TABLE_OPTION_VALUE_LIST:
      return "list";
  }
  return "unknown";
}

const char *mylite_database_option_kind_name(MyliteDatabaseOptionKind kind) {
  switch (kind) {
    case MYLITE_DATABASE_OPTION_UNKNOWN:
      return "unknown";
    case MYLITE_DATABASE_OPTION_CHARSET:
      return "charset";
    case MYLITE_DATABASE_OPTION_COLLATE:
      return "collate";
    case MYLITE_DATABASE_OPTION_ENCRYPTION:
      return "encryption";
    case MYLITE_DATABASE_OPTION_PLACEMENT_POLICY:
      return "placement_policy";
    case MYLITE_DATABASE_OPTION_TI_FLASH_REPLICA:
      return "ti_flash_replica";
    case MYLITE_DATABASE_OPTION_READ_ONLY:
      return "read_only";
  }
  return "unknown";
}

const char *mylite_database_option_value_kind_name(
    MyliteDatabaseOptionValueKind kind) {
  switch (kind) {
    case MYLITE_DATABASE_OPTION_VALUE_UNKNOWN:
      return "unknown";
    case MYLITE_DATABASE_OPTION_VALUE_RAW:
      return "raw";
    case MYLITE_DATABASE_OPTION_VALUE_IDENTIFIER:
      return "identifier";
    case MYLITE_DATABASE_OPTION_VALUE_STRING:
      return "string";
    case MYLITE_DATABASE_OPTION_VALUE_DEFAULT:
      return "default";
  }
  return "unknown";
}

const char *mylite_create_view_algorithm_name(
    MyliteCreateViewAlgorithm algorithm) {
  switch (algorithm) {
    case MYLITE_CREATE_VIEW_ALGORITHM_UNSPECIFIED:
      return "unspecified";
    case MYLITE_CREATE_VIEW_ALGORITHM_UNDEFINED:
      return "undefined";
    case MYLITE_CREATE_VIEW_ALGORITHM_MERGE:
      return "merge";
    case MYLITE_CREATE_VIEW_ALGORITHM_TEMPTABLE:
      return "temptable";
  }
  return "unspecified";
}

const char *mylite_view_sql_security_name(MyliteViewSqlSecurity security) {
  switch (security) {
    case MYLITE_VIEW_SQL_SECURITY_UNSPECIFIED:
      return "unspecified";
    case MYLITE_VIEW_SQL_SECURITY_DEFINER:
      return "definer";
    case MYLITE_VIEW_SQL_SECURITY_INVOKER:
      return "invoker";
  }
  return "unspecified";
}

const char *mylite_view_check_option_name(MyliteViewCheckOption check_option) {
  switch (check_option) {
    case MYLITE_VIEW_CHECK_OPTION_NONE:
      return "none";
    case MYLITE_VIEW_CHECK_OPTION_CASCADED:
      return "cascaded";
    case MYLITE_VIEW_CHECK_OPTION_LOCAL:
      return "local";
  }
  return "none";
}

const char *mylite_drop_view_mode_name(MyliteDropViewMode mode) {
  switch (mode) {
    case MYLITE_DROP_VIEW_MODE_UNSPECIFIED:
      return "unspecified";
    case MYLITE_DROP_VIEW_MODE_RESTRICT:
      return "restrict";
    case MYLITE_DROP_VIEW_MODE_CASCADE:
      return "cascade";
  }
  return "unspecified";
}

const char *mylite_set_statement_form_name(MyliteSetStatementForm form) {
  switch (form) {
    case MYLITE_SET_STATEMENT_UNKNOWN:
      return "unknown";
    case MYLITE_SET_STATEMENT_ASSIGNMENTS:
      return "assignments";
    case MYLITE_SET_STATEMENT_PASSWORD:
      return "password";
    case MYLITE_SET_STATEMENT_TRANSACTION:
      return "transaction";
    case MYLITE_SET_STATEMENT_CONFIG:
      return "config";
    case MYLITE_SET_STATEMENT_SESSION_STATES:
      return "session_states";
    case MYLITE_SET_STATEMENT_RESOURCE_GROUP:
      return "resource_group";
    case MYLITE_SET_STATEMENT_ROLE:
      return "role";
    case MYLITE_SET_STATEMENT_DEFAULT_ROLE:
      return "default_role";
  }
  return "unknown";
}

const char *mylite_set_assignment_kind_name(MyliteSetAssignmentKind kind) {
  switch (kind) {
    case MYLITE_SET_ASSIGNMENT_UNKNOWN:
      return "unknown";
    case MYLITE_SET_ASSIGNMENT_SYSTEM_VARIABLE:
      return "system_variable";
    case MYLITE_SET_ASSIGNMENT_USER_VARIABLE:
      return "user_variable";
    case MYLITE_SET_ASSIGNMENT_NAMES:
      return "names";
    case MYLITE_SET_ASSIGNMENT_CHARACTER_SET:
      return "character_set";
    case MYLITE_SET_ASSIGNMENT_TRANSACTION_CHARACTERISTIC:
      return "transaction_characteristic";
    case MYLITE_SET_ASSIGNMENT_CONFIG:
      return "config";
  }
  return "unknown";
}

const char *mylite_set_variable_scope_name(MyliteSetVariableScope scope) {
  switch (scope) {
    case MYLITE_SET_VARIABLE_SCOPE_UNSPECIFIED:
      return "unspecified";
    case MYLITE_SET_VARIABLE_SCOPE_GLOBAL:
      return "global";
    case MYLITE_SET_VARIABLE_SCOPE_SESSION:
      return "session";
    case MYLITE_SET_VARIABLE_SCOPE_LOCAL:
      return "local";
    case MYLITE_SET_VARIABLE_SCOPE_INSTANCE:
      return "instance";
  }
  return "unspecified";
}

const char *mylite_set_assignment_operator_name(
    MyliteSetAssignmentOperator operator_kind) {
  switch (operator_kind) {
    case MYLITE_SET_ASSIGNMENT_OPERATOR_NONE:
      return "none";
    case MYLITE_SET_ASSIGNMENT_OPERATOR_EQ:
      return "eq";
    case MYLITE_SET_ASSIGNMENT_OPERATOR_ASSIGNMENT_EQ:
      return "assignment_eq";
  }
  return "none";
}

const char *mylite_prepare_statement_source_kind_name(
    MylitePrepareStatementSourceKind kind) {
  switch (kind) {
    case MYLITE_PREPARE_STATEMENT_SOURCE_UNKNOWN:
      return "unknown";
    case MYLITE_PREPARE_STATEMENT_SOURCE_STRING:
      return "string";
    case MYLITE_PREPARE_STATEMENT_SOURCE_USER_VARIABLE:
      return "user_variable";
  }
  return "unknown";
}

const char *mylite_deallocate_statement_mode_name(
    MyliteDeallocateStatementMode mode) {
  switch (mode) {
    case MYLITE_DEALLOCATE_STATEMENT_MODE_UNKNOWN:
      return "unknown";
    case MYLITE_DEALLOCATE_STATEMENT_MODE_DEALLOCATE:
      return "deallocate";
    case MYLITE_DEALLOCATE_STATEMENT_MODE_DROP:
      return "drop";
  }
  return "unknown";
}

const char *mylite_transaction_statement_kind_name(
    MyliteTransactionStatementKind kind) {
  switch (kind) {
    case MYLITE_TRANSACTION_STATEMENT_UNKNOWN:
      return "unknown";
    case MYLITE_TRANSACTION_STATEMENT_BEGIN:
      return "begin";
    case MYLITE_TRANSACTION_STATEMENT_COMMIT:
      return "commit";
    case MYLITE_TRANSACTION_STATEMENT_ROLLBACK:
      return "rollback";
    case MYLITE_TRANSACTION_STATEMENT_SAVEPOINT:
      return "savepoint";
    case MYLITE_TRANSACTION_STATEMENT_RELEASE_SAVEPOINT:
      return "release_savepoint";
  }
  return "unknown";
}

const char *mylite_transaction_begin_form_name(
    MyliteTransactionBeginForm form) {
  switch (form) {
    case MYLITE_TRANSACTION_BEGIN_FORM_UNKNOWN:
      return "unknown";
    case MYLITE_TRANSACTION_BEGIN_FORM_BEGIN:
      return "begin";
    case MYLITE_TRANSACTION_BEGIN_FORM_START_TRANSACTION:
      return "start_transaction";
  }
  return "unknown";
}

const char *mylite_transaction_begin_mode_name(
    MyliteTransactionBeginMode mode) {
  switch (mode) {
    case MYLITE_TRANSACTION_BEGIN_MODE_UNSPECIFIED:
      return "unspecified";
    case MYLITE_TRANSACTION_BEGIN_MODE_PESSIMISTIC:
      return "pessimistic";
    case MYLITE_TRANSACTION_BEGIN_MODE_OPTIMISTIC:
      return "optimistic";
  }
  return "unspecified";
}

const char *mylite_transaction_access_mode_name(
    MyliteTransactionAccessMode mode) {
  switch (mode) {
    case MYLITE_TRANSACTION_ACCESS_UNSPECIFIED:
      return "unspecified";
    case MYLITE_TRANSACTION_ACCESS_READ_WRITE:
      return "read_write";
    case MYLITE_TRANSACTION_ACCESS_READ_ONLY:
      return "read_only";
  }
  return "unspecified";
}

const char *mylite_alter_table_spec_kind_name(MyliteAlterTableSpecKind kind) {
  switch (kind) {
    case MYLITE_ALTER_TABLE_SPEC_UNKNOWN:
      return "unknown";
    case MYLITE_ALTER_TABLE_SPEC_TABLE_OPTIONS:
      return "table_options";
    case MYLITE_ALTER_TABLE_SPEC_CONVERT_CHARACTER_SET:
      return "convert_character_set";
    case MYLITE_ALTER_TABLE_SPEC_ADD_COLUMN:
      return "add_column";
    case MYLITE_ALTER_TABLE_SPEC_ADD_TABLE_ELEMENTS:
      return "add_table_elements";
    case MYLITE_ALTER_TABLE_SPEC_ADD_CONSTRAINT:
      return "add_constraint";
    case MYLITE_ALTER_TABLE_SPEC_ADD_PARTITION:
      return "add_partition";
    case MYLITE_ALTER_TABLE_SPEC_DROP_COLUMN:
      return "drop_column";
    case MYLITE_ALTER_TABLE_SPEC_DROP_PRIMARY_KEY:
      return "drop_primary_key";
    case MYLITE_ALTER_TABLE_SPEC_DROP_INDEX:
      return "drop_index";
    case MYLITE_ALTER_TABLE_SPEC_DROP_FOREIGN_KEY:
      return "drop_foreign_key";
    case MYLITE_ALTER_TABLE_SPEC_DROP_CHECK:
      return "drop_check";
    case MYLITE_ALTER_TABLE_SPEC_DROP_PARTITION:
      return "drop_partition";
    case MYLITE_ALTER_TABLE_SPEC_MODIFY_COLUMN:
      return "modify_column";
    case MYLITE_ALTER_TABLE_SPEC_CHANGE_COLUMN:
      return "change_column";
    case MYLITE_ALTER_TABLE_SPEC_ALTER_COLUMN_SET_DEFAULT:
      return "alter_column_set_default";
    case MYLITE_ALTER_TABLE_SPEC_ALTER_COLUMN_DROP_DEFAULT:
      return "alter_column_drop_default";
    case MYLITE_ALTER_TABLE_SPEC_ALTER_COLUMN_VISIBILITY:
      return "alter_column_visibility";
    case MYLITE_ALTER_TABLE_SPEC_RENAME_COLUMN:
      return "rename_column";
    case MYLITE_ALTER_TABLE_SPEC_RENAME_TABLE:
      return "rename_table";
    case MYLITE_ALTER_TABLE_SPEC_RENAME_INDEX:
      return "rename_index";
    case MYLITE_ALTER_TABLE_SPEC_ORDER_BY:
      return "order_by";
    case MYLITE_ALTER_TABLE_SPEC_DISABLE_KEYS:
      return "disable_keys";
    case MYLITE_ALTER_TABLE_SPEC_ENABLE_KEYS:
      return "enable_keys";
    case MYLITE_ALTER_TABLE_SPEC_LOCK:
      return "lock";
    case MYLITE_ALTER_TABLE_SPEC_ALGORITHM:
      return "algorithm";
    case MYLITE_ALTER_TABLE_SPEC_FORCE:
      return "force";
    case MYLITE_ALTER_TABLE_SPEC_VALIDATION:
      return "validation";
    case MYLITE_ALTER_TABLE_SPEC_ALTER_CHECK:
      return "alter_check";
    case MYLITE_ALTER_TABLE_SPEC_ALTER_INDEX_VISIBILITY:
      return "alter_index_visibility";
    case MYLITE_ALTER_TABLE_SPEC_TABLESPACE:
      return "tablespace";
    case MYLITE_ALTER_TABLE_SPEC_PARTITION:
      return "partition";
    case MYLITE_ALTER_TABLE_SPEC_SECONDARY_LOAD:
      return "secondary_load";
    case MYLITE_ALTER_TABLE_SPEC_SECONDARY_UNLOAD:
      return "secondary_unload";
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

const char *mylite_ast_statement_target_schema_value(const MyliteAst *ast,
                                                     size_t index) {
  const MyliteAstStatementTarget *target =
      mylite_ast_statement_target_at(ast, index, 0);
  return target == NULL ? NULL : target->schema_value;
}

size_t mylite_ast_statement_target_schema_value_length(const MyliteAst *ast,
                                                       size_t index) {
  const MyliteAstStatementTarget *target =
      mylite_ast_statement_target_at(ast, index, 0);
  return target == NULL ? 0 : target->schema_value_length;
}

size_t mylite_ast_statement_target_name_start(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? 0 : statement->target_name_start;
}

size_t mylite_ast_statement_target_name_end(const MyliteAst *ast, size_t index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, index);
  return statement == NULL ? 0 : statement->target_name_end;
}

const char *mylite_ast_statement_target_name_value(const MyliteAst *ast,
                                                   size_t index) {
  const MyliteAstStatementTarget *target =
      mylite_ast_statement_target_at(ast, index, 0);
  return target == NULL ? NULL : target->name_value;
}

size_t mylite_ast_statement_target_name_value_length(const MyliteAst *ast,
                                                     size_t index) {
  const MyliteAstStatementTarget *target =
      mylite_ast_statement_target_at(ast, index, 0);
  return target == NULL ? 0 : target->name_value_length;
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

const char *mylite_ast_statement_target_schema_value_at(
    const MyliteAst *ast, size_t statement_index, size_t target_index) {
  const MyliteAstStatementTarget *target =
      mylite_ast_statement_target_at(ast, statement_index, target_index);
  return target == NULL ? NULL : target->schema_value;
}

size_t mylite_ast_statement_target_schema_value_length_at(
    const MyliteAst *ast, size_t statement_index, size_t target_index) {
  const MyliteAstStatementTarget *target =
      mylite_ast_statement_target_at(ast, statement_index, target_index);
  return target == NULL ? 0 : target->schema_value_length;
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

const char *mylite_ast_statement_target_name_value_at(
    const MyliteAst *ast, size_t statement_index, size_t target_index) {
  const MyliteAstStatementTarget *target =
      mylite_ast_statement_target_at(ast, statement_index, target_index);
  return target == NULL ? NULL : target->name_value;
}

size_t mylite_ast_statement_target_name_value_length_at(
    const MyliteAst *ast, size_t statement_index, size_t target_index) {
  const MyliteAstStatementTarget *target =
      mylite_ast_statement_target_at(ast, statement_index, target_index);
  return target == NULL ? 0 : target->name_value_length;
}

const MyliteAstAlterTable *mylite_ast_alter_table_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->alter_table;
}

const MyliteAstCreateDatabase *mylite_ast_create_database_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->create_database;
}

const MyliteAstCreateTable *mylite_ast_create_table_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement = mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->create_table;
}

const MyliteAstCreateIndex *mylite_ast_create_index_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->create_index;
}

const MyliteAstCreateView *mylite_ast_create_view_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->create_view;
}

const MyliteAstDropDatabase *mylite_ast_drop_database_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->drop_database;
}

const MyliteAstDropIndex *mylite_ast_drop_index_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->drop_index;
}

const MyliteAstDropTable *mylite_ast_drop_table_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->drop_table;
}

const MyliteAstDropView *mylite_ast_drop_view_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->drop_view;
}

const MyliteAstPrepareStatement *mylite_ast_prepare_statement_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->prepare_statement;
}

const MyliteAstExecuteStatement *mylite_ast_execute_statement_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->execute_statement;
}

const MyliteAstDeallocateStatement *mylite_ast_deallocate_statement_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->deallocate_statement;
}

const MyliteAstRenameTable *mylite_ast_rename_table_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->rename_table;
}

const MyliteAstSetStatement *mylite_ast_set_statement_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->set_statement;
}

const MyliteAstTruncateTable *mylite_ast_truncate_table_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->truncate_table;
}

const MyliteAstTransactionStatement *mylite_ast_transaction_statement_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->transaction_statement;
}

const MyliteAstUseDatabase *mylite_ast_use_database_view(
    const MyliteAst *ast, size_t statement_index) {
  const MyliteAstStatement *statement =
      mylite_ast_statement_at(ast, statement_index);
  return statement == NULL ? NULL : statement->use_database;
}

const MyliteAstNode *mylite_ast_alter_table_view_node(
    const MyliteAstAlterTable *alter_table) {
  return alter_table == NULL ? NULL : alter_table->node;
}

size_t mylite_ast_alter_table_view_start(
    const MyliteAstAlterTable *alter_table) {
  return alter_table == NULL ? 0 : alter_table->start;
}

size_t mylite_ast_alter_table_view_end(
    const MyliteAstAlterTable *alter_table) {
  return alter_table == NULL ? 0 : alter_table->end;
}

size_t mylite_ast_alter_table_view_target_start(
    const MyliteAstAlterTable *alter_table) {
  return alter_table == NULL || alter_table->target == NULL
             ? 0
             : alter_table->target->start;
}

size_t mylite_ast_alter_table_view_target_end(
    const MyliteAstAlterTable *alter_table) {
  return alter_table == NULL || alter_table->target == NULL
             ? 0
             : alter_table->target->end;
}

const char *mylite_ast_alter_table_view_schema_value(
    const MyliteAstAlterTable *alter_table) {
  return alter_table == NULL || alter_table->target == NULL
             ? NULL
             : alter_table->target->schema_value;
}

size_t mylite_ast_alter_table_view_schema_value_length(
    const MyliteAstAlterTable *alter_table) {
  return alter_table == NULL || alter_table->target == NULL
             ? 0
             : alter_table->target->schema_value_length;
}

const char *mylite_ast_alter_table_view_name_value(
    const MyliteAstAlterTable *alter_table) {
  return alter_table == NULL || alter_table->target == NULL
             ? NULL
             : alter_table->target->name_value;
}

size_t mylite_ast_alter_table_view_name_value_length(
    const MyliteAstAlterTable *alter_table) {
  return alter_table == NULL || alter_table->target == NULL
             ? 0
             : alter_table->target->name_value_length;
}

size_t mylite_ast_alter_table_view_spec_count(
    const MyliteAstAlterTable *alter_table) {
  return alter_table == NULL ? 0 : alter_table->spec_count;
}

const MyliteAstAlterTableSpec *mylite_ast_alter_table_view_spec_at(
    const MyliteAstAlterTable *alter_table, size_t spec_index) {
  if (alter_table == NULL || spec_index >= alter_table->spec_count) {
    return NULL;
  }
  return &alter_table->specs[spec_index];
}

size_t mylite_ast_alter_table_view_option_count(
    const MyliteAstAlterTable *alter_table) {
  return alter_table == NULL ? 0 : alter_table->option_count;
}

const MyliteAstCreateTableOption *mylite_ast_alter_table_view_option_at(
    const MyliteAstAlterTable *alter_table, size_t option_index) {
  if (alter_table == NULL || option_index >= alter_table->option_count) {
    return NULL;
  }
  return &alter_table->options[option_index];
}

const MyliteAstNode *mylite_ast_alter_table_spec_view_node(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? NULL : spec->node;
}

const MyliteAstCreateTableColumn *mylite_ast_alter_table_spec_view_column(
    const MyliteAstAlterTableSpec *spec) {
  return mylite_ast_alter_table_spec_view_column_at(spec, 0);
}

const MyliteAstCreateTableKey *mylite_ast_alter_table_spec_view_key(
    const MyliteAstAlterTableSpec *spec) {
  return mylite_ast_alter_table_spec_view_key_at(spec, 0);
}

size_t mylite_ast_alter_table_spec_view_column_count(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? 0 : spec->column_count;
}

const MyliteAstCreateTableColumn *mylite_ast_alter_table_spec_view_column_at(
    const MyliteAstAlterTableSpec *spec, size_t column_index) {
  if (spec == NULL || column_index >= spec->column_count) {
    return NULL;
  }
  return &spec->columns[column_index];
}

size_t mylite_ast_alter_table_spec_view_key_count(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? 0 : spec->key_count;
}

const MyliteAstCreateTableKey *mylite_ast_alter_table_spec_view_key_at(
    const MyliteAstAlterTableSpec *spec, size_t key_index) {
  if (spec == NULL || key_index >= spec->key_count) {
    return NULL;
  }
  return &spec->keys[key_index];
}

MyliteAlterTableSpecKind mylite_ast_alter_table_spec_view_kind(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? MYLITE_ALTER_TABLE_SPEC_UNKNOWN : spec->kind;
}

size_t mylite_ast_alter_table_spec_view_start(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? 0 : spec->start;
}

size_t mylite_ast_alter_table_spec_view_end(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? 0 : spec->end;
}

int mylite_ast_alter_table_spec_view_has_if_exists(
    const MyliteAstAlterTableSpec *spec) {
  return spec != NULL && spec->has_if_exists;
}

int mylite_ast_alter_table_spec_view_has_if_not_exists(
    const MyliteAstAlterTableSpec *spec) {
  return spec != NULL && spec->has_if_not_exists;
}

size_t mylite_ast_alter_table_spec_view_name_start(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? 0 : spec->name_start;
}

size_t mylite_ast_alter_table_spec_view_name_end(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? 0 : spec->name_end;
}

const char *mylite_ast_alter_table_spec_view_name_value(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? NULL : spec->name_value;
}

size_t mylite_ast_alter_table_spec_view_name_value_length(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? 0 : spec->name_value_length;
}

size_t mylite_ast_alter_table_spec_view_secondary_name_start(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? 0 : spec->secondary_name_start;
}

size_t mylite_ast_alter_table_spec_view_secondary_name_end(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? 0 : spec->secondary_name_end;
}

const char *mylite_ast_alter_table_spec_view_secondary_name_value(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? NULL : spec->secondary_name_value;
}

size_t mylite_ast_alter_table_spec_view_secondary_name_value_length(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? 0 : spec->secondary_name_value_length;
}

size_t mylite_ast_alter_table_spec_view_table_start(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? 0 : spec->table_start;
}

size_t mylite_ast_alter_table_spec_view_table_end(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? 0 : spec->table_end;
}

const char *mylite_ast_alter_table_spec_view_table_schema_value(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? NULL : spec->table_schema_value;
}

size_t mylite_ast_alter_table_spec_view_table_schema_value_length(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? 0 : spec->table_schema_value_length;
}

const char *mylite_ast_alter_table_spec_view_table_name_value(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? NULL : spec->table_name_value;
}

size_t mylite_ast_alter_table_spec_view_table_name_value_length(
    const MyliteAstAlterTableSpec *spec) {
  return spec == NULL ? 0 : spec->table_name_value_length;
}

const MyliteAstNode *mylite_ast_create_table_view_node(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? NULL : create_table->node;
}

size_t mylite_ast_create_table_view_start(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? 0 : create_table->start;
}

size_t mylite_ast_create_table_view_end(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? 0 : create_table->end;
}

size_t mylite_ast_create_table_view_target_start(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? 0 : create_table->target_start;
}

size_t mylite_ast_create_table_view_target_end(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? 0 : create_table->target_end;
}

size_t mylite_ast_create_table_view_schema_start(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? 0 : create_table->schema_start;
}

size_t mylite_ast_create_table_view_schema_end(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? 0 : create_table->schema_end;
}

const char *mylite_ast_create_table_view_schema_value(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? NULL : create_table->schema_value;
}

size_t mylite_ast_create_table_view_schema_value_length(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? 0 : create_table->schema_value_length;
}

size_t mylite_ast_create_table_view_name_start(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? 0 : create_table->name_start;
}

size_t mylite_ast_create_table_view_name_end(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? 0 : create_table->name_end;
}

const char *mylite_ast_create_table_view_name_value(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? NULL : create_table->name_value;
}

size_t mylite_ast_create_table_view_name_value_length(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? 0 : create_table->name_value_length;
}

size_t mylite_ast_create_table_view_column_count(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? 0 : create_table->column_count;
}

size_t mylite_ast_create_table_view_key_count(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? 0 : create_table->key_count;
}

size_t mylite_ast_create_table_view_option_count(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? 0 : create_table->option_count;
}

const MyliteAstCreateTableOption *mylite_ast_create_table_view_engine_option(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? NULL : create_table->engine_option;
}

const char *mylite_ast_create_table_view_engine_value(
    const MyliteAstCreateTable *create_table) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_view_engine_option(create_table);
  return option == NULL ? NULL : option->value;
}

size_t mylite_ast_create_table_view_engine_value_length(
    const MyliteAstCreateTable *create_table) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_view_engine_option(create_table);
  return option == NULL ? 0 : option->value_length;
}

const MyliteAstCreateTableOption *mylite_ast_create_table_view_charset_option(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? NULL : create_table->charset_option;
}

const char *mylite_ast_create_table_view_charset_value(
    const MyliteAstCreateTable *create_table) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_view_charset_option(create_table);
  return option == NULL ? NULL : option->value;
}

size_t mylite_ast_create_table_view_charset_value_length(
    const MyliteAstCreateTable *create_table) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_view_charset_option(create_table);
  return option == NULL ? 0 : option->value_length;
}

const MyliteAstCreateTableOption *mylite_ast_create_table_view_collation_option(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? NULL : create_table->collation_option;
}

const char *mylite_ast_create_table_view_collation_value(
    const MyliteAstCreateTable *create_table) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_view_collation_option(create_table);
  return option == NULL ? NULL : option->value;
}

size_t mylite_ast_create_table_view_collation_value_length(
    const MyliteAstCreateTable *create_table) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_view_collation_option(create_table);
  return option == NULL ? 0 : option->value_length;
}

const MyliteAstCreateTableOption *mylite_ast_create_table_view_comment_option(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? NULL : create_table->comment_option;
}

const char *mylite_ast_create_table_view_comment_value(
    const MyliteAstCreateTable *create_table) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_view_comment_option(create_table);
  return option == NULL ? NULL : option->value;
}

size_t mylite_ast_create_table_view_comment_value_length(
    const MyliteAstCreateTable *create_table) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_view_comment_option(create_table);
  return option == NULL ? 0 : option->value_length;
}

const MyliteAstCreateTableOption *
mylite_ast_create_table_view_auto_increment_option(
    const MyliteAstCreateTable *create_table) {
  return create_table == NULL ? NULL : create_table->auto_increment_option;
}

int mylite_ast_create_table_view_has_auto_increment_value(
    const MyliteAstCreateTable *create_table) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_view_auto_increment_option(create_table);
  return option != NULL && option->has_unsigned_integer_value;
}

unsigned long long mylite_ast_create_table_view_auto_increment_value(
    const MyliteAstCreateTable *create_table) {
  const MyliteAstCreateTableOption *option =
      mylite_ast_create_table_view_auto_increment_option(create_table);
  return option == NULL ? 0 : option->unsigned_integer_value;
}

const MyliteAstCreateTableColumn *mylite_ast_create_table_view_column_at(
    const MyliteAstCreateTable *create_table, size_t column_index) {
  if (create_table == NULL || column_index >= create_table->column_count) {
    return NULL;
  }
  return &create_table->columns[column_index];
}

const MyliteAstCreateTableKey *mylite_ast_create_table_view_key_at(
    const MyliteAstCreateTable *create_table, size_t key_index) {
  if (create_table == NULL || key_index >= create_table->key_count) {
    return NULL;
  }
  return &create_table->keys[key_index];
}

const MyliteAstCreateTableOption *mylite_ast_create_table_view_option_at(
    const MyliteAstCreateTable *create_table, size_t option_index) {
  if (create_table == NULL || option_index >= create_table->option_count) {
    return NULL;
  }
  return &create_table->options[option_index];
}

const MyliteAstNode *mylite_ast_create_index_view_node(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL ? NULL : create_index->node;
}

size_t mylite_ast_create_index_view_start(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL ? 0 : create_index->start;
}

size_t mylite_ast_create_index_view_end(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL ? 0 : create_index->end;
}

MyliteCreateTableKeyKind mylite_ast_create_index_view_key_kind(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL ? MYLITE_CREATE_TABLE_KEY_UNKNOWN
                              : create_index->key.kind;
}

MyliteCreateTableIndexType mylite_ast_create_index_view_index_type_kind(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL ? MYLITE_CREATE_TABLE_INDEX_TYPE_UNSPECIFIED
                              : create_index->key.index_type_kind;
}

MyliteCreateTableKeyVisibility mylite_ast_create_index_view_visibility(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL ? MYLITE_CREATE_TABLE_KEY_VISIBILITY_UNSPECIFIED
                              : create_index->key.visibility;
}

size_t mylite_ast_create_index_view_name_start(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL ? 0 : create_index->key.name_start;
}

size_t mylite_ast_create_index_view_name_end(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL ? 0 : create_index->key.name_end;
}

const char *mylite_ast_create_index_view_name_value(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL ? NULL : create_index->key.name_value;
}

size_t mylite_ast_create_index_view_name_value_length(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL ? 0 : create_index->key.name_value_length;
}

size_t mylite_ast_create_index_view_table_start(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL || create_index->target == NULL
             ? 0
             : create_index->target->start;
}

size_t mylite_ast_create_index_view_table_end(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL || create_index->target == NULL
             ? 0
             : create_index->target->end;
}

const char *mylite_ast_create_index_view_table_schema_value(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL || create_index->target == NULL
             ? NULL
             : create_index->target->schema_value;
}

size_t mylite_ast_create_index_view_table_schema_value_length(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL || create_index->target == NULL
             ? 0
             : create_index->target->schema_value_length;
}

const char *mylite_ast_create_index_view_table_name_value(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL || create_index->target == NULL
             ? NULL
             : create_index->target->name_value;
}

size_t mylite_ast_create_index_view_table_name_value_length(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL || create_index->target == NULL
             ? 0
             : create_index->target->name_value_length;
}

size_t mylite_ast_create_index_view_column_count(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL ? 0 : create_index->key.column_count;
}

const MyliteAstCreateTableKeyPart *mylite_ast_create_index_view_column_at(
    const MyliteAstCreateIndex *create_index, size_t column_index) {
  if (create_index == NULL || column_index >= create_index->key.column_count) {
    return NULL;
  }
  return &create_index->key.columns[column_index];
}

size_t mylite_ast_create_index_view_option_count(
    const MyliteAstCreateIndex *create_index) {
  return create_index == NULL ? 0 : create_index->key.option_count;
}

const MyliteAstCreateTableKeyOption *mylite_ast_create_index_view_option_at(
    const MyliteAstCreateIndex *create_index, size_t option_index) {
  if (create_index == NULL || option_index >= create_index->key.option_count) {
    return NULL;
  }
  return &create_index->key.options[option_index];
}

const char *mylite_ast_create_index_view_comment_value(
    const MyliteAstCreateIndex *create_index) {
  return mylite_ast_create_table_key_view_comment_value(
      create_index == NULL ? NULL : &create_index->key);
}

size_t mylite_ast_create_index_view_comment_value_length(
    const MyliteAstCreateIndex *create_index) {
  return mylite_ast_create_table_key_view_comment_value_length(
      create_index == NULL ? NULL : &create_index->key);
}

const char *mylite_ast_create_index_view_parser_value(
    const MyliteAstCreateIndex *create_index) {
  return mylite_ast_create_table_key_view_parser_value(
      create_index == NULL ? NULL : &create_index->key);
}

size_t mylite_ast_create_index_view_parser_value_length(
    const MyliteAstCreateIndex *create_index) {
  return mylite_ast_create_table_key_view_parser_value_length(
      create_index == NULL ? NULL : &create_index->key);
}

int mylite_ast_create_index_view_has_key_block_size_value(
    const MyliteAstCreateIndex *create_index) {
  return mylite_ast_create_table_key_view_has_key_block_size_value(
      create_index == NULL ? NULL : &create_index->key);
}

unsigned long long mylite_ast_create_index_view_key_block_size_value(
    const MyliteAstCreateIndex *create_index) {
  return mylite_ast_create_table_key_view_key_block_size_value(
      create_index == NULL ? NULL : &create_index->key);
}

const MyliteAstNode *mylite_ast_create_database_view_node(
    const MyliteAstCreateDatabase *create_database) {
  return create_database == NULL ? NULL : create_database->node;
}

size_t mylite_ast_create_database_view_start(
    const MyliteAstCreateDatabase *create_database) {
  return create_database == NULL ? 0 : create_database->start;
}

size_t mylite_ast_create_database_view_end(
    const MyliteAstCreateDatabase *create_database) {
  return create_database == NULL ? 0 : create_database->end;
}

int mylite_ast_create_database_view_has_if_not_exists(
    const MyliteAstCreateDatabase *create_database) {
  return create_database != NULL && create_database->has_if_not_exists;
}

int mylite_ast_create_database_view_uses_schema_keyword(
    const MyliteAstCreateDatabase *create_database) {
  return create_database != NULL && create_database->uses_schema_keyword;
}

size_t mylite_ast_create_database_view_name_start(
    const MyliteAstCreateDatabase *create_database) {
  return create_database == NULL || create_database->target == NULL
             ? 0
             : create_database->target->name_start;
}

size_t mylite_ast_create_database_view_name_end(
    const MyliteAstCreateDatabase *create_database) {
  return create_database == NULL || create_database->target == NULL
             ? 0
             : create_database->target->name_end;
}

const char *mylite_ast_create_database_view_name_value(
    const MyliteAstCreateDatabase *create_database) {
  return create_database == NULL || create_database->target == NULL
             ? NULL
             : create_database->target->name_value;
}

size_t mylite_ast_create_database_view_name_value_length(
    const MyliteAstCreateDatabase *create_database) {
  return create_database == NULL || create_database->target == NULL
             ? 0
             : create_database->target->name_value_length;
}

size_t mylite_ast_create_database_view_option_count(
    const MyliteAstCreateDatabase *create_database) {
  return create_database == NULL ? 0 : create_database->option_count;
}

const MyliteAstDatabaseOption *mylite_ast_create_database_view_option_at(
    const MyliteAstCreateDatabase *create_database, size_t option_index) {
  if (create_database == NULL || option_index >= create_database->option_count) {
    return NULL;
  }
  return &create_database->options[option_index];
}

const char *mylite_ast_create_database_view_charset_value(
    const MyliteAstCreateDatabase *create_database) {
  return create_database == NULL || create_database->charset_option == NULL
             ? NULL
             : create_database->charset_option->value;
}

size_t mylite_ast_create_database_view_charset_value_length(
    const MyliteAstCreateDatabase *create_database) {
  return create_database == NULL || create_database->charset_option == NULL
             ? 0
             : create_database->charset_option->value_length;
}

const char *mylite_ast_create_database_view_collation_value(
    const MyliteAstCreateDatabase *create_database) {
  return create_database == NULL || create_database->collation_option == NULL
             ? NULL
             : create_database->collation_option->value;
}

size_t mylite_ast_create_database_view_collation_value_length(
    const MyliteAstCreateDatabase *create_database) {
  return create_database == NULL || create_database->collation_option == NULL
             ? 0
             : create_database->collation_option->value_length;
}

const char *mylite_ast_create_database_view_encryption_value(
    const MyliteAstCreateDatabase *create_database) {
  return create_database == NULL || create_database->encryption_option == NULL
             ? NULL
             : create_database->encryption_option->value;
}

size_t mylite_ast_create_database_view_encryption_value_length(
    const MyliteAstCreateDatabase *create_database) {
  return create_database == NULL || create_database->encryption_option == NULL
             ? 0
             : create_database->encryption_option->value_length;
}

const MyliteAstNode *mylite_ast_drop_database_view_node(
    const MyliteAstDropDatabase *drop_database) {
  return drop_database == NULL ? NULL : drop_database->node;
}

size_t mylite_ast_drop_database_view_start(
    const MyliteAstDropDatabase *drop_database) {
  return drop_database == NULL ? 0 : drop_database->start;
}

size_t mylite_ast_drop_database_view_end(
    const MyliteAstDropDatabase *drop_database) {
  return drop_database == NULL ? 0 : drop_database->end;
}

int mylite_ast_drop_database_view_has_if_exists(
    const MyliteAstDropDatabase *drop_database) {
  return drop_database != NULL && drop_database->has_if_exists;
}

int mylite_ast_drop_database_view_uses_schema_keyword(
    const MyliteAstDropDatabase *drop_database) {
  return drop_database != NULL && drop_database->uses_schema_keyword;
}

size_t mylite_ast_drop_database_view_name_start(
    const MyliteAstDropDatabase *drop_database) {
  return drop_database == NULL || drop_database->target == NULL
             ? 0
             : drop_database->target->name_start;
}

size_t mylite_ast_drop_database_view_name_end(
    const MyliteAstDropDatabase *drop_database) {
  return drop_database == NULL || drop_database->target == NULL
             ? 0
             : drop_database->target->name_end;
}

const char *mylite_ast_drop_database_view_name_value(
    const MyliteAstDropDatabase *drop_database) {
  return drop_database == NULL || drop_database->target == NULL
             ? NULL
             : drop_database->target->name_value;
}

size_t mylite_ast_drop_database_view_name_value_length(
    const MyliteAstDropDatabase *drop_database) {
  return drop_database == NULL || drop_database->target == NULL
             ? 0
             : drop_database->target->name_value_length;
}

const MyliteAstNode *mylite_ast_database_option_view_node(
    const MyliteAstDatabaseOption *option) {
  return option == NULL ? NULL : option->node;
}

MyliteDatabaseOptionKind mylite_ast_database_option_view_kind(
    const MyliteAstDatabaseOption *option) {
  return option == NULL ? MYLITE_DATABASE_OPTION_UNKNOWN : option->kind;
}

MyliteDatabaseOptionValueKind mylite_ast_database_option_view_value_kind(
    const MyliteAstDatabaseOption *option) {
  return option == NULL ? MYLITE_DATABASE_OPTION_VALUE_UNKNOWN
                        : option->value_kind;
}

size_t mylite_ast_database_option_view_start(
    const MyliteAstDatabaseOption *option) {
  return option == NULL ? 0 : option->start;
}

size_t mylite_ast_database_option_view_end(
    const MyliteAstDatabaseOption *option) {
  return option == NULL ? 0 : option->end;
}

size_t mylite_ast_database_option_view_name_start(
    const MyliteAstDatabaseOption *option) {
  return option == NULL ? 0 : option->name_start;
}

size_t mylite_ast_database_option_view_name_end(
    const MyliteAstDatabaseOption *option) {
  return option == NULL ? 0 : option->name_end;
}

size_t mylite_ast_database_option_view_value_start(
    const MyliteAstDatabaseOption *option) {
  return option == NULL ? 0 : option->value_start;
}

size_t mylite_ast_database_option_view_value_end(
    const MyliteAstDatabaseOption *option) {
  return option == NULL ? 0 : option->value_end;
}

const char *mylite_ast_database_option_view_value(
    const MyliteAstDatabaseOption *option) {
  return option == NULL ? NULL : option->value;
}

size_t mylite_ast_database_option_view_value_length(
    const MyliteAstDatabaseOption *option) {
  return option == NULL ? 0 : option->value_length;
}

const MyliteAstNode *mylite_ast_create_view_view_node(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL ? NULL : create_view->node;
}

size_t mylite_ast_create_view_view_start(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL ? 0 : create_view->start;
}

size_t mylite_ast_create_view_view_end(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL ? 0 : create_view->end;
}

int mylite_ast_create_view_view_has_or_replace(
    const MyliteAstCreateView *create_view) {
  return create_view != NULL && create_view->has_or_replace;
}

MyliteCreateViewAlgorithm mylite_ast_create_view_view_algorithm(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL ? MYLITE_CREATE_VIEW_ALGORITHM_UNSPECIFIED
                             : create_view->algorithm;
}

MyliteViewSqlSecurity mylite_ast_create_view_view_sql_security(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL ? MYLITE_VIEW_SQL_SECURITY_UNSPECIFIED
                             : create_view->sql_security;
}

MyliteViewCheckOption mylite_ast_create_view_view_check_option(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL ? MYLITE_VIEW_CHECK_OPTION_NONE
                             : create_view->check_option;
}

size_t mylite_ast_create_view_view_name_start(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL || create_view->target == NULL
             ? 0
             : create_view->target->name_start;
}

size_t mylite_ast_create_view_view_name_end(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL || create_view->target == NULL
             ? 0
             : create_view->target->name_end;
}

const char *mylite_ast_create_view_view_schema_value(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL || create_view->target == NULL
             ? NULL
             : create_view->target->schema_value;
}

size_t mylite_ast_create_view_view_schema_value_length(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL || create_view->target == NULL
             ? 0
             : create_view->target->schema_value_length;
}

const char *mylite_ast_create_view_view_name_value(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL || create_view->target == NULL
             ? NULL
             : create_view->target->name_value;
}

size_t mylite_ast_create_view_view_name_value_length(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL || create_view->target == NULL
             ? 0
             : create_view->target->name_value_length;
}

size_t mylite_ast_create_view_view_definer_start(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL ? 0 : create_view->definer_start;
}

size_t mylite_ast_create_view_view_definer_end(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL ? 0 : create_view->definer_end;
}

const MyliteAstNode *mylite_ast_create_view_view_select_node(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL ? NULL : create_view->select_node;
}

size_t mylite_ast_create_view_view_select_start(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL ? 0 : create_view->select_start;
}

size_t mylite_ast_create_view_view_select_end(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL ? 0 : create_view->select_end;
}

size_t mylite_ast_create_view_view_column_count(
    const MyliteAstCreateView *create_view) {
  return create_view == NULL ? 0 : create_view->column_count;
}

const MyliteAstViewColumn *mylite_ast_create_view_view_column_at(
    const MyliteAstCreateView *create_view, size_t column_index) {
  if (create_view == NULL || column_index >= create_view->column_count) {
    return NULL;
  }
  return &create_view->columns[column_index];
}

size_t mylite_ast_view_column_view_start(const MyliteAstViewColumn *column) {
  return column == NULL ? 0 : column->start;
}

size_t mylite_ast_view_column_view_end(const MyliteAstViewColumn *column) {
  return column == NULL ? 0 : column->end;
}

const char *mylite_ast_view_column_view_name_value(
    const MyliteAstViewColumn *column) {
  return column == NULL ? NULL : column->name_value;
}

size_t mylite_ast_view_column_view_name_value_length(
    const MyliteAstViewColumn *column) {
  return column == NULL ? 0 : column->name_value_length;
}

const MyliteAstNode *mylite_ast_drop_view_view_node(
    const MyliteAstDropView *drop_view) {
  return drop_view == NULL ? NULL : drop_view->node;
}

size_t mylite_ast_drop_view_view_start(const MyliteAstDropView *drop_view) {
  return drop_view == NULL ? 0 : drop_view->start;
}

size_t mylite_ast_drop_view_view_end(const MyliteAstDropView *drop_view) {
  return drop_view == NULL ? 0 : drop_view->end;
}

int mylite_ast_drop_view_view_has_if_exists(
    const MyliteAstDropView *drop_view) {
  return drop_view != NULL && drop_view->has_if_exists;
}

MyliteDropViewMode mylite_ast_drop_view_view_mode(
    const MyliteAstDropView *drop_view) {
  return drop_view == NULL ? MYLITE_DROP_VIEW_MODE_UNSPECIFIED
                           : drop_view->mode;
}

size_t mylite_ast_drop_view_view_view_count(
    const MyliteAstDropView *drop_view) {
  return drop_view == NULL ? 0 : drop_view->target_count;
}

const char *mylite_ast_drop_view_view_schema_value_at(
    const MyliteAstDropView *drop_view, size_t view_index) {
  return drop_view == NULL || view_index >= drop_view->target_count
             ? NULL
             : drop_view->targets[view_index].schema_value;
}

size_t mylite_ast_drop_view_view_schema_value_length_at(
    const MyliteAstDropView *drop_view, size_t view_index) {
  return drop_view == NULL || view_index >= drop_view->target_count
             ? 0
             : drop_view->targets[view_index].schema_value_length;
}

const char *mylite_ast_drop_view_view_name_value_at(
    const MyliteAstDropView *drop_view, size_t view_index) {
  return drop_view == NULL || view_index >= drop_view->target_count
             ? NULL
             : drop_view->targets[view_index].name_value;
}

size_t mylite_ast_drop_view_view_name_value_length_at(
    const MyliteAstDropView *drop_view, size_t view_index) {
  return drop_view == NULL || view_index >= drop_view->target_count
             ? 0
             : drop_view->targets[view_index].name_value_length;
}

const MyliteAstNode *mylite_ast_set_statement_view_node(
    const MyliteAstSetStatement *set_statement) {
  return set_statement == NULL ? NULL : set_statement->node;
}

size_t mylite_ast_set_statement_view_start(
    const MyliteAstSetStatement *set_statement) {
  return set_statement == NULL ? 0 : set_statement->start;
}

size_t mylite_ast_set_statement_view_end(
    const MyliteAstSetStatement *set_statement) {
  return set_statement == NULL ? 0 : set_statement->end;
}

MyliteSetStatementForm mylite_ast_set_statement_view_form(
    const MyliteAstSetStatement *set_statement) {
  return set_statement == NULL ? MYLITE_SET_STATEMENT_UNKNOWN
                               : set_statement->form;
}

size_t mylite_ast_set_statement_view_assignment_count(
    const MyliteAstSetStatement *set_statement) {
  return set_statement == NULL ? 0 : set_statement->assignment_count;
}

const MyliteAstSetAssignment *mylite_ast_set_statement_view_assignment_at(
    const MyliteAstSetStatement *set_statement, size_t assignment_index) {
  if (set_statement == NULL ||
      assignment_index >= set_statement->assignment_count) {
    return NULL;
  }
  return &set_statement->assignments[assignment_index];
}

const MyliteAstNode *mylite_ast_set_assignment_view_node(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL ? NULL : assignment->node;
}

MyliteSetAssignmentKind mylite_ast_set_assignment_view_kind(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL ? MYLITE_SET_ASSIGNMENT_UNKNOWN : assignment->kind;
}

MyliteSetVariableScope mylite_ast_set_assignment_view_scope(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL ? MYLITE_SET_VARIABLE_SCOPE_UNSPECIFIED
                            : assignment->scope;
}

MyliteSetAssignmentOperator mylite_ast_set_assignment_view_operator(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL ? MYLITE_SET_ASSIGNMENT_OPERATOR_NONE
                            : assignment->operator_kind;
}

size_t mylite_ast_set_assignment_view_start(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL ? 0 : assignment->start;
}

size_t mylite_ast_set_assignment_view_end(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL ? 0 : assignment->end;
}

size_t mylite_ast_set_assignment_view_name_start(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL ? 0 : assignment->name_start;
}

size_t mylite_ast_set_assignment_view_name_end(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL ? 0 : assignment->name_end;
}

const char *mylite_ast_set_assignment_view_name_value(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL ? NULL : assignment->name_value;
}

size_t mylite_ast_set_assignment_view_name_value_length(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL ? 0 : assignment->name_value_length;
}

const MyliteAstNode *mylite_ast_set_assignment_view_value_node(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL ? NULL : assignment->value_node;
}

const MyliteAstExpression *mylite_ast_set_assignment_view_value_expression(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL || assignment->value_expression.node == NULL
             ? NULL
             : &assignment->value_expression;
}

size_t mylite_ast_set_assignment_view_value_start(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL ? 0 : assignment->value_start;
}

size_t mylite_ast_set_assignment_view_value_end(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL ? 0 : assignment->value_end;
}

const MyliteAstNode *mylite_ast_set_assignment_view_extend_value_node(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL ? NULL : assignment->extend_value_node;
}

size_t mylite_ast_set_assignment_view_extend_value_start(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL ? 0 : assignment->extend_value_start;
}

size_t mylite_ast_set_assignment_view_extend_value_end(
    const MyliteAstSetAssignment *assignment) {
  return assignment == NULL ? 0 : assignment->extend_value_end;
}

const MyliteAstNode *mylite_ast_prepare_statement_view_node(
    const MyliteAstPrepareStatement *prepare_statement) {
  return prepare_statement == NULL ? NULL : prepare_statement->node;
}

size_t mylite_ast_prepare_statement_view_start(
    const MyliteAstPrepareStatement *prepare_statement) {
  return prepare_statement == NULL ? 0 : prepare_statement->start;
}

size_t mylite_ast_prepare_statement_view_end(
    const MyliteAstPrepareStatement *prepare_statement) {
  return prepare_statement == NULL ? 0 : prepare_statement->end;
}

size_t mylite_ast_prepare_statement_view_name_start(
    const MyliteAstPrepareStatement *prepare_statement) {
  return prepare_statement == NULL ? 0 : prepare_statement->name_start;
}

size_t mylite_ast_prepare_statement_view_name_end(
    const MyliteAstPrepareStatement *prepare_statement) {
  return prepare_statement == NULL ? 0 : prepare_statement->name_end;
}

const char *mylite_ast_prepare_statement_view_name_value(
    const MyliteAstPrepareStatement *prepare_statement) {
  return prepare_statement == NULL ? NULL : prepare_statement->name_value;
}

size_t mylite_ast_prepare_statement_view_name_value_length(
    const MyliteAstPrepareStatement *prepare_statement) {
  return prepare_statement == NULL ? 0 : prepare_statement->name_value_length;
}

MylitePrepareStatementSourceKind
mylite_ast_prepare_statement_view_source_kind(
    const MyliteAstPrepareStatement *prepare_statement) {
  return prepare_statement == NULL ? MYLITE_PREPARE_STATEMENT_SOURCE_UNKNOWN
                                   : prepare_statement->source_kind;
}

const MyliteAstNode *mylite_ast_prepare_statement_view_source_node(
    const MyliteAstPrepareStatement *prepare_statement) {
  return prepare_statement == NULL ? NULL : prepare_statement->source_node;
}

size_t mylite_ast_prepare_statement_view_source_start(
    const MyliteAstPrepareStatement *prepare_statement) {
  return prepare_statement == NULL ? 0 : prepare_statement->source_start;
}

size_t mylite_ast_prepare_statement_view_source_end(
    const MyliteAstPrepareStatement *prepare_statement) {
  return prepare_statement == NULL ? 0 : prepare_statement->source_end;
}

const char *mylite_ast_prepare_statement_view_source_value(
    const MyliteAstPrepareStatement *prepare_statement) {
  return prepare_statement == NULL ? NULL : prepare_statement->source_value;
}

size_t mylite_ast_prepare_statement_view_source_value_length(
    const MyliteAstPrepareStatement *prepare_statement) {
  return prepare_statement == NULL ? 0 : prepare_statement->source_value_length;
}

const MyliteAstNode *mylite_ast_execute_statement_view_node(
    const MyliteAstExecuteStatement *execute_statement) {
  return execute_statement == NULL ? NULL : execute_statement->node;
}

size_t mylite_ast_execute_statement_view_start(
    const MyliteAstExecuteStatement *execute_statement) {
  return execute_statement == NULL ? 0 : execute_statement->start;
}

size_t mylite_ast_execute_statement_view_end(
    const MyliteAstExecuteStatement *execute_statement) {
  return execute_statement == NULL ? 0 : execute_statement->end;
}

size_t mylite_ast_execute_statement_view_name_start(
    const MyliteAstExecuteStatement *execute_statement) {
  return execute_statement == NULL ? 0 : execute_statement->name_start;
}

size_t mylite_ast_execute_statement_view_name_end(
    const MyliteAstExecuteStatement *execute_statement) {
  return execute_statement == NULL ? 0 : execute_statement->name_end;
}

const char *mylite_ast_execute_statement_view_name_value(
    const MyliteAstExecuteStatement *execute_statement) {
  return execute_statement == NULL ? NULL : execute_statement->name_value;
}

size_t mylite_ast_execute_statement_view_name_value_length(
    const MyliteAstExecuteStatement *execute_statement) {
  return execute_statement == NULL ? 0 : execute_statement->name_value_length;
}

size_t mylite_ast_execute_statement_view_using_count(
    const MyliteAstExecuteStatement *execute_statement) {
  return execute_statement == NULL ? 0 : execute_statement->using_count;
}

const MyliteAstPreparedStatementVariable *
mylite_ast_execute_statement_view_using_variable_at(
    const MyliteAstExecuteStatement *execute_statement, size_t variable_index) {
  if (execute_statement == NULL ||
      variable_index >= execute_statement->using_count) {
    return NULL;
  }
  return &execute_statement->using_variables[variable_index];
}

const MyliteAstNode *mylite_ast_prepared_statement_variable_view_node(
    const MyliteAstPreparedStatementVariable *variable) {
  return variable == NULL ? NULL : variable->node;
}

size_t mylite_ast_prepared_statement_variable_view_start(
    const MyliteAstPreparedStatementVariable *variable) {
  return variable == NULL ? 0 : variable->start;
}

size_t mylite_ast_prepared_statement_variable_view_end(
    const MyliteAstPreparedStatementVariable *variable) {
  return variable == NULL ? 0 : variable->end;
}

size_t mylite_ast_prepared_statement_variable_view_name_start(
    const MyliteAstPreparedStatementVariable *variable) {
  return variable == NULL ? 0 : variable->name_start;
}

size_t mylite_ast_prepared_statement_variable_view_name_end(
    const MyliteAstPreparedStatementVariable *variable) {
  return variable == NULL ? 0 : variable->name_end;
}

const char *mylite_ast_prepared_statement_variable_view_name_value(
    const MyliteAstPreparedStatementVariable *variable) {
  return variable == NULL ? NULL : variable->name_value;
}

size_t mylite_ast_prepared_statement_variable_view_name_value_length(
    const MyliteAstPreparedStatementVariable *variable) {
  return variable == NULL ? 0 : variable->name_value_length;
}

const MyliteAstNode *mylite_ast_deallocate_statement_view_node(
    const MyliteAstDeallocateStatement *deallocate_statement) {
  return deallocate_statement == NULL ? NULL : deallocate_statement->node;
}

size_t mylite_ast_deallocate_statement_view_start(
    const MyliteAstDeallocateStatement *deallocate_statement) {
  return deallocate_statement == NULL ? 0 : deallocate_statement->start;
}

size_t mylite_ast_deallocate_statement_view_end(
    const MyliteAstDeallocateStatement *deallocate_statement) {
  return deallocate_statement == NULL ? 0 : deallocate_statement->end;
}

MyliteDeallocateStatementMode mylite_ast_deallocate_statement_view_mode(
    const MyliteAstDeallocateStatement *deallocate_statement) {
  return deallocate_statement == NULL ? MYLITE_DEALLOCATE_STATEMENT_MODE_UNKNOWN
                                      : deallocate_statement->mode;
}

size_t mylite_ast_deallocate_statement_view_name_start(
    const MyliteAstDeallocateStatement *deallocate_statement) {
  return deallocate_statement == NULL ? 0 : deallocate_statement->name_start;
}

size_t mylite_ast_deallocate_statement_view_name_end(
    const MyliteAstDeallocateStatement *deallocate_statement) {
  return deallocate_statement == NULL ? 0 : deallocate_statement->name_end;
}

const char *mylite_ast_deallocate_statement_view_name_value(
    const MyliteAstDeallocateStatement *deallocate_statement) {
  return deallocate_statement == NULL ? NULL : deallocate_statement->name_value;
}

size_t mylite_ast_deallocate_statement_view_name_value_length(
    const MyliteAstDeallocateStatement *deallocate_statement) {
  return deallocate_statement == NULL ? 0
                                      : deallocate_statement->name_value_length;
}

const MyliteAstNode *mylite_ast_transaction_statement_view_node(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement == NULL ? NULL : transaction_statement->node;
}

size_t mylite_ast_transaction_statement_view_start(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement == NULL ? 0 : transaction_statement->start;
}

size_t mylite_ast_transaction_statement_view_end(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement == NULL ? 0 : transaction_statement->end;
}

MyliteTransactionStatementKind mylite_ast_transaction_statement_view_kind(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement == NULL ? MYLITE_TRANSACTION_STATEMENT_UNKNOWN
                                       : transaction_statement->kind;
}

MyliteTransactionBeginForm mylite_ast_transaction_statement_view_begin_form(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement == NULL ? MYLITE_TRANSACTION_BEGIN_FORM_UNKNOWN
                                       : transaction_statement->begin_form;
}

MyliteTransactionBeginMode mylite_ast_transaction_statement_view_begin_mode(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement == NULL
             ? MYLITE_TRANSACTION_BEGIN_MODE_UNSPECIFIED
             : transaction_statement->begin_mode;
}

MyliteTransactionAccessMode mylite_ast_transaction_statement_view_access_mode(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement == NULL ? MYLITE_TRANSACTION_ACCESS_UNSPECIFIED
                                       : transaction_statement->access_mode;
}

int mylite_ast_transaction_statement_view_has_consistent_snapshot(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement != NULL &&
         transaction_statement->has_consistent_snapshot;
}

int mylite_ast_transaction_statement_view_has_causal_consistency(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement != NULL &&
         transaction_statement->has_causal_consistency;
}

int mylite_ast_transaction_statement_view_has_work_keyword(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement != NULL && transaction_statement->has_work_keyword;
}

int mylite_ast_transaction_statement_view_has_chain(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement != NULL && transaction_statement->has_chain;
}

int mylite_ast_transaction_statement_view_has_no_chain(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement != NULL && transaction_statement->has_no_chain;
}

int mylite_ast_transaction_statement_view_has_release(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement != NULL && transaction_statement->has_release;
}

int mylite_ast_transaction_statement_view_has_no_release(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement != NULL && transaction_statement->has_no_release;
}

int mylite_ast_transaction_statement_view_has_savepoint_keyword(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement != NULL &&
         transaction_statement->has_savepoint_keyword;
}

size_t mylite_ast_transaction_statement_view_savepoint_name_start(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement == NULL ? 0
                                       : transaction_statement->savepoint_name_start;
}

size_t mylite_ast_transaction_statement_view_savepoint_name_end(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement == NULL ? 0
                                       : transaction_statement->savepoint_name_end;
}

const char *mylite_ast_transaction_statement_view_savepoint_name_value(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement == NULL
             ? NULL
             : transaction_statement->savepoint_name_value;
}

size_t mylite_ast_transaction_statement_view_savepoint_name_value_length(
    const MyliteAstTransactionStatement *transaction_statement) {
  return transaction_statement == NULL
             ? 0
             : transaction_statement->savepoint_name_value_length;
}

const MyliteAstNode *mylite_ast_expression_view_node(
    const MyliteAstExpression *expression) {
  return expression == NULL ? NULL : expression->node;
}

MyliteExpressionKind mylite_ast_expression_view_kind(
    const MyliteAstExpression *expression) {
  return expression == NULL ? MYLITE_EXPRESSION_UNKNOWN : expression->kind;
}

MyliteExpressionLiteralKind mylite_ast_expression_view_literal_kind(
    const MyliteAstExpression *expression) {
  return expression == NULL ? MYLITE_EXPRESSION_LITERAL_NONE
                            : expression->literal_kind;
}

size_t mylite_ast_expression_view_start(const MyliteAstExpression *expression) {
  return expression == NULL ? 0 : expression->start;
}

size_t mylite_ast_expression_view_end(const MyliteAstExpression *expression) {
  return expression == NULL ? 0 : expression->end;
}

size_t mylite_ast_expression_view_value_start(
    const MyliteAstExpression *expression) {
  return expression == NULL ? 0 : expression->value_start;
}

size_t mylite_ast_expression_view_value_end(
    const MyliteAstExpression *expression) {
  return expression == NULL ? 0 : expression->value_end;
}

const char *mylite_ast_expression_view_value(
    const MyliteAstExpression *expression) {
  return expression == NULL ? NULL : expression->value;
}

size_t mylite_ast_expression_view_value_length(
    const MyliteAstExpression *expression) {
  return expression == NULL ? 0 : expression->value_length;
}

int mylite_ast_expression_view_has_unsigned_integer(
    const MyliteAstExpression *expression) {
  return expression != NULL && expression->has_unsigned_integer_value;
}

unsigned long long mylite_ast_expression_view_unsigned_integer_value(
    const MyliteAstExpression *expression) {
  return expression == NULL ? 0 : expression->unsigned_integer_value;
}

const MyliteAstNode *mylite_ast_drop_index_view_node(
    const MyliteAstDropIndex *drop_index) {
  return drop_index == NULL ? NULL : drop_index->node;
}

size_t mylite_ast_drop_index_view_start(
    const MyliteAstDropIndex *drop_index) {
  return drop_index == NULL ? 0 : drop_index->start;
}

size_t mylite_ast_drop_index_view_end(const MyliteAstDropIndex *drop_index) {
  return drop_index == NULL ? 0 : drop_index->end;
}

int mylite_ast_drop_index_view_has_if_exists(
    const MyliteAstDropIndex *drop_index) {
  return drop_index != NULL && drop_index->has_if_exists;
}

int mylite_ast_drop_index_view_is_hypothetical(
    const MyliteAstDropIndex *drop_index) {
  return drop_index != NULL && drop_index->is_hypothetical;
}

size_t mylite_ast_drop_index_view_name_start(
    const MyliteAstDropIndex *drop_index) {
  return drop_index == NULL ? 0 : drop_index->name_start;
}

size_t mylite_ast_drop_index_view_name_end(
    const MyliteAstDropIndex *drop_index) {
  return drop_index == NULL ? 0 : drop_index->name_end;
}

const char *mylite_ast_drop_index_view_name_value(
    const MyliteAstDropIndex *drop_index) {
  return drop_index == NULL ? NULL : drop_index->name_value;
}

size_t mylite_ast_drop_index_view_name_value_length(
    const MyliteAstDropIndex *drop_index) {
  return drop_index == NULL ? 0 : drop_index->name_value_length;
}

const char *mylite_ast_drop_index_view_table_schema_value(
    const MyliteAstDropIndex *drop_index) {
  return drop_index == NULL || drop_index->target == NULL
             ? NULL
             : drop_index->target->schema_value;
}

size_t mylite_ast_drop_index_view_table_schema_value_length(
    const MyliteAstDropIndex *drop_index) {
  return drop_index == NULL || drop_index->target == NULL
             ? 0
             : drop_index->target->schema_value_length;
}

const char *mylite_ast_drop_index_view_table_name_value(
    const MyliteAstDropIndex *drop_index) {
  return drop_index == NULL || drop_index->target == NULL
             ? NULL
             : drop_index->target->name_value;
}

size_t mylite_ast_drop_index_view_table_name_value_length(
    const MyliteAstDropIndex *drop_index) {
  return drop_index == NULL || drop_index->target == NULL
             ? 0
             : drop_index->target->name_value_length;
}

const MyliteAstNode *mylite_ast_drop_table_view_node(
    const MyliteAstDropTable *drop_table) {
  return drop_table == NULL ? NULL : drop_table->node;
}

size_t mylite_ast_drop_table_view_start(const MyliteAstDropTable *drop_table) {
  return drop_table == NULL ? 0 : drop_table->start;
}

size_t mylite_ast_drop_table_view_end(const MyliteAstDropTable *drop_table) {
  return drop_table == NULL ? 0 : drop_table->end;
}

int mylite_ast_drop_table_view_is_temporary(
    const MyliteAstDropTable *drop_table) {
  return drop_table != NULL && drop_table->is_temporary;
}

int mylite_ast_drop_table_view_has_if_exists(
    const MyliteAstDropTable *drop_table) {
  return drop_table != NULL && drop_table->has_if_exists;
}

size_t mylite_ast_drop_table_view_table_count(
    const MyliteAstDropTable *drop_table) {
  return drop_table == NULL ? 0 : drop_table->target_count;
}

const char *mylite_ast_drop_table_view_table_schema_value_at(
    const MyliteAstDropTable *drop_table, size_t table_index) {
  return drop_table == NULL || table_index >= drop_table->target_count
             ? NULL
             : drop_table->targets[table_index].schema_value;
}

size_t mylite_ast_drop_table_view_table_schema_value_length_at(
    const MyliteAstDropTable *drop_table, size_t table_index) {
  return drop_table == NULL || table_index >= drop_table->target_count
             ? 0
             : drop_table->targets[table_index].schema_value_length;
}

const char *mylite_ast_drop_table_view_table_name_value_at(
    const MyliteAstDropTable *drop_table, size_t table_index) {
  return drop_table == NULL || table_index >= drop_table->target_count
             ? NULL
             : drop_table->targets[table_index].name_value;
}

size_t mylite_ast_drop_table_view_table_name_value_length_at(
    const MyliteAstDropTable *drop_table, size_t table_index) {
  return drop_table == NULL || table_index >= drop_table->target_count
             ? 0
             : drop_table->targets[table_index].name_value_length;
}

const MyliteAstNode *mylite_ast_rename_table_view_node(
    const MyliteAstRenameTable *rename_table) {
  return rename_table == NULL ? NULL : rename_table->node;
}

size_t mylite_ast_rename_table_view_start(
    const MyliteAstRenameTable *rename_table) {
  return rename_table == NULL ? 0 : rename_table->start;
}

size_t mylite_ast_rename_table_view_end(
    const MyliteAstRenameTable *rename_table) {
  return rename_table == NULL ? 0 : rename_table->end;
}

size_t mylite_ast_rename_table_view_pair_count(
    const MyliteAstRenameTable *rename_table) {
  return rename_table == NULL ? 0 : rename_table->target_count / 2;
}

const char *mylite_ast_rename_table_view_source_schema_value_at(
    const MyliteAstRenameTable *rename_table, size_t pair_index) {
  size_t target_index = pair_index * 2;
  return rename_table == NULL || target_index >= rename_table->target_count
             ? NULL
             : rename_table->targets[target_index].schema_value;
}

size_t mylite_ast_rename_table_view_source_schema_value_length_at(
    const MyliteAstRenameTable *rename_table, size_t pair_index) {
  size_t target_index = pair_index * 2;
  return rename_table == NULL || target_index >= rename_table->target_count
             ? 0
             : rename_table->targets[target_index].schema_value_length;
}

const char *mylite_ast_rename_table_view_source_name_value_at(
    const MyliteAstRenameTable *rename_table, size_t pair_index) {
  size_t target_index = pair_index * 2;
  return rename_table == NULL || target_index >= rename_table->target_count
             ? NULL
             : rename_table->targets[target_index].name_value;
}

size_t mylite_ast_rename_table_view_source_name_value_length_at(
    const MyliteAstRenameTable *rename_table, size_t pair_index) {
  size_t target_index = pair_index * 2;
  return rename_table == NULL || target_index >= rename_table->target_count
             ? 0
             : rename_table->targets[target_index].name_value_length;
}

const char *mylite_ast_rename_table_view_destination_schema_value_at(
    const MyliteAstRenameTable *rename_table, size_t pair_index) {
  size_t target_index = pair_index * 2 + 1;
  return rename_table == NULL || target_index >= rename_table->target_count
             ? NULL
             : rename_table->targets[target_index].schema_value;
}

size_t mylite_ast_rename_table_view_destination_schema_value_length_at(
    const MyliteAstRenameTable *rename_table, size_t pair_index) {
  size_t target_index = pair_index * 2 + 1;
  return rename_table == NULL || target_index >= rename_table->target_count
             ? 0
             : rename_table->targets[target_index].schema_value_length;
}

const char *mylite_ast_rename_table_view_destination_name_value_at(
    const MyliteAstRenameTable *rename_table, size_t pair_index) {
  size_t target_index = pair_index * 2 + 1;
  return rename_table == NULL || target_index >= rename_table->target_count
             ? NULL
             : rename_table->targets[target_index].name_value;
}

size_t mylite_ast_rename_table_view_destination_name_value_length_at(
    const MyliteAstRenameTable *rename_table, size_t pair_index) {
  size_t target_index = pair_index * 2 + 1;
  return rename_table == NULL || target_index >= rename_table->target_count
             ? 0
             : rename_table->targets[target_index].name_value_length;
}

const MyliteAstNode *mylite_ast_truncate_table_view_node(
    const MyliteAstTruncateTable *truncate_table) {
  return truncate_table == NULL ? NULL : truncate_table->node;
}

size_t mylite_ast_truncate_table_view_start(
    const MyliteAstTruncateTable *truncate_table) {
  return truncate_table == NULL ? 0 : truncate_table->start;
}

size_t mylite_ast_truncate_table_view_end(
    const MyliteAstTruncateTable *truncate_table) {
  return truncate_table == NULL ? 0 : truncate_table->end;
}

int mylite_ast_truncate_table_view_has_table_keyword(
    const MyliteAstTruncateTable *truncate_table) {
  return truncate_table != NULL && truncate_table->has_table_keyword;
}

const char *mylite_ast_truncate_table_view_schema_value(
    const MyliteAstTruncateTable *truncate_table) {
  return truncate_table == NULL || truncate_table->target == NULL
             ? NULL
             : truncate_table->target->schema_value;
}

size_t mylite_ast_truncate_table_view_schema_value_length(
    const MyliteAstTruncateTable *truncate_table) {
  return truncate_table == NULL || truncate_table->target == NULL
             ? 0
             : truncate_table->target->schema_value_length;
}

const char *mylite_ast_truncate_table_view_name_value(
    const MyliteAstTruncateTable *truncate_table) {
  return truncate_table == NULL || truncate_table->target == NULL
             ? NULL
             : truncate_table->target->name_value;
}

size_t mylite_ast_truncate_table_view_name_value_length(
    const MyliteAstTruncateTable *truncate_table) {
  return truncate_table == NULL || truncate_table->target == NULL
             ? 0
             : truncate_table->target->name_value_length;
}

const MyliteAstNode *mylite_ast_use_database_view_node(
    const MyliteAstUseDatabase *use_database) {
  return use_database == NULL ? NULL : use_database->node;
}

size_t mylite_ast_use_database_view_start(
    const MyliteAstUseDatabase *use_database) {
  return use_database == NULL ? 0 : use_database->start;
}

size_t mylite_ast_use_database_view_end(
    const MyliteAstUseDatabase *use_database) {
  return use_database == NULL ? 0 : use_database->end;
}

size_t mylite_ast_use_database_view_name_start(
    const MyliteAstUseDatabase *use_database) {
  return use_database == NULL || use_database->target == NULL
             ? 0
             : use_database->target->name_start;
}

size_t mylite_ast_use_database_view_name_end(
    const MyliteAstUseDatabase *use_database) {
  return use_database == NULL || use_database->target == NULL
             ? 0
             : use_database->target->name_end;
}

const char *mylite_ast_use_database_view_name_value(
    const MyliteAstUseDatabase *use_database) {
  return use_database == NULL || use_database->target == NULL
             ? NULL
             : use_database->target->name_value;
}

size_t mylite_ast_use_database_view_name_value_length(
    const MyliteAstUseDatabase *use_database) {
  return use_database == NULL || use_database->target == NULL
             ? 0
             : use_database->target->name_value_length;
}

size_t mylite_ast_create_table_column_view_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->start;
}

size_t mylite_ast_create_table_column_view_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->end;
}

const char *mylite_ast_create_table_column_view_name_value(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->name_value;
}

size_t mylite_ast_create_table_column_view_name_value_length(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->name_value_length;
}

MyliteCreateTableColumnTypeFamily mylite_ast_create_table_column_view_type_family(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? MYLITE_CREATE_TABLE_COLUMN_TYPE_UNKNOWN
                        : column->type_family;
}

MyliteCreateTableColumnTypeKind mylite_ast_create_table_column_view_type_kind(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_UNKNOWN
                        : column->type_kind;
}

MyliteCreateTableColumnStorageClass
mylite_ast_create_table_column_view_storage_class(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? MYLITE_CREATE_TABLE_COLUMN_STORAGE_UNKNOWN
                        : column->storage_class;
}

unsigned int mylite_ast_create_table_column_view_flags(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->flags;
}

MyliteCreateTableColumnNullability
mylite_ast_create_table_column_view_nullability(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? MYLITE_CREATE_TABLE_COLUMN_NULLABILITY_UNSPECIFIED
                        : column->nullability;
}

MyliteCreateTableColumnGeneratedStorage
mylite_ast_create_table_column_view_generated_storage_kind(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL
             ? MYLITE_CREATE_TABLE_COLUMN_GENERATED_STORAGE_UNSPECIFIED
             : column->generated_storage_kind;
}

size_t mylite_ast_create_table_column_view_name_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->name_start;
}

size_t mylite_ast_create_table_column_view_name_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->name_end;
}

size_t mylite_ast_create_table_column_view_type_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_start;
}

size_t mylite_ast_create_table_column_view_type_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_end;
}

const MyliteAstNode *mylite_ast_create_table_column_view_type_node(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->type_node;
}

size_t mylite_ast_create_table_column_view_type_name_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_name_start;
}

size_t mylite_ast_create_table_column_view_type_name_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_name_end;
}

size_t mylite_ast_create_table_column_view_type_parameters_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_parameters_start;
}

size_t mylite_ast_create_table_column_view_type_parameters_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_parameters_end;
}

size_t mylite_ast_create_table_column_view_type_numeric_parameter_count(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_numeric_parameter_count;
}

unsigned long long
mylite_ast_create_table_column_view_type_numeric_parameter_at(
    const MyliteAstCreateTableColumn *column, size_t parameter_index) {
  if (column == NULL ||
      parameter_index >= column->type_numeric_parameter_count ||
      parameter_index >= sizeof(column->type_numeric_parameters) /
                             sizeof(column->type_numeric_parameters[0])) {
    return 0;
  }
  return column->type_numeric_parameters[parameter_index];
}

size_t mylite_ast_create_table_column_view_type_element_count(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_element_count;
}

const MyliteAstCreateTableColumnTypeElement *
mylite_ast_create_table_column_view_type_element_at(
    const MyliteAstCreateTableColumn *column, size_t element_index) {
  if (column == NULL || element_index >= column->type_element_count) {
    return NULL;
  }
  return &column->type_elements[element_index];
}

size_t mylite_ast_create_table_column_type_element_view_start(
    const MyliteAstCreateTableColumnTypeElement *element) {
  return element == NULL ? 0 : element->start;
}

size_t mylite_ast_create_table_column_type_element_view_end(
    const MyliteAstCreateTableColumnTypeElement *element) {
  return element == NULL ? 0 : element->end;
}

const char *mylite_ast_create_table_column_type_element_view_value(
    const MyliteAstCreateTableColumnTypeElement *element) {
  return element == NULL ? NULL : element->value;
}

size_t mylite_ast_create_table_column_type_element_view_value_length(
    const MyliteAstCreateTableColumnTypeElement *element) {
  return element == NULL ? 0 : element->value_length;
}

int mylite_ast_create_table_column_view_type_has_length(
    const MyliteAstCreateTableColumn *column) {
  return column != NULL &&
         (column->type_shape_flags &
          MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_LENGTH) != 0;
}

unsigned long long mylite_ast_create_table_column_view_type_length(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_length;
}

int mylite_ast_create_table_column_view_type_has_precision(
    const MyliteAstCreateTableColumn *column) {
  return column != NULL &&
         (column->type_shape_flags &
          MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_PRECISION) != 0;
}

unsigned long long mylite_ast_create_table_column_view_type_precision(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_precision;
}

int mylite_ast_create_table_column_view_type_has_scale(
    const MyliteAstCreateTableColumn *column) {
  return column != NULL &&
         (column->type_shape_flags &
          MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_SCALE) != 0;
}

unsigned long long mylite_ast_create_table_column_view_type_scale(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_scale;
}

int mylite_ast_create_table_column_view_type_has_fractional_seconds_precision(
    const MyliteAstCreateTableColumn *column) {
  return column != NULL &&
         (column->type_shape_flags &
          MYLITE_AST_CREATE_TABLE_COLUMN_TYPE_SHAPE_FSP) != 0;
}

unsigned long long
mylite_ast_create_table_column_view_type_fractional_seconds_precision(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_fractional_seconds_precision;
}

size_t mylite_ast_create_table_column_view_type_attributes_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_attributes_start;
}

size_t mylite_ast_create_table_column_view_type_attributes_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_attributes_end;
}

size_t mylite_ast_create_table_column_view_type_unsigned_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_unsigned_start;
}

size_t mylite_ast_create_table_column_view_type_unsigned_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_unsigned_end;
}

size_t mylite_ast_create_table_column_view_type_zerofill_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_zerofill_start;
}

size_t mylite_ast_create_table_column_view_type_zerofill_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_zerofill_end;
}

size_t mylite_ast_create_table_column_view_type_binary_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_binary_start;
}

size_t mylite_ast_create_table_column_view_type_binary_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_binary_end;
}

size_t mylite_ast_create_table_column_view_type_charset_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_charset_start;
}

size_t mylite_ast_create_table_column_view_type_charset_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_charset_end;
}

size_t mylite_ast_create_table_column_view_type_charset_value_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_charset_value_start;
}

size_t mylite_ast_create_table_column_view_type_charset_value_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_charset_value_end;
}

const char *mylite_ast_create_table_column_view_type_charset_value(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->type_charset_value;
}

size_t mylite_ast_create_table_column_view_type_charset_value_length(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_charset_value_length;
}

size_t mylite_ast_create_table_column_view_type_collation_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_collation_start;
}

size_t mylite_ast_create_table_column_view_type_collation_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_collation_end;
}

size_t mylite_ast_create_table_column_view_type_collation_value_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_collation_value_start;
}

size_t mylite_ast_create_table_column_view_type_collation_value_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_collation_value_end;
}

const char *mylite_ast_create_table_column_view_type_collation_value(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->type_collation_value;
}

size_t mylite_ast_create_table_column_view_type_collation_value_length(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->type_collation_value_length;
}

size_t mylite_ast_create_table_column_view_options_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->options_start;
}

size_t mylite_ast_create_table_column_view_options_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->options_end;
}

const MyliteAstNode *mylite_ast_create_table_column_view_options_node(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->options_node;
}

size_t mylite_ast_create_table_column_view_default_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->default_start;
}

size_t mylite_ast_create_table_column_view_default_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->default_end;
}

const MyliteAstNode *mylite_ast_create_table_column_view_default_node(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->default_node;
}

size_t mylite_ast_create_table_column_view_default_value_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->default_value_start;
}

size_t mylite_ast_create_table_column_view_default_value_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->default_value_end;
}

const MyliteAstNode *mylite_ast_create_table_column_view_default_value_node(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->default_value_node;
}

MyliteCreateTableColumnValueKind
mylite_ast_create_table_column_view_default_value_kind(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? MYLITE_CREATE_TABLE_COLUMN_VALUE_UNKNOWN
                        : column->default_value_kind;
}

const char *mylite_ast_create_table_column_view_default_value(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->default_value;
}

size_t mylite_ast_create_table_column_view_default_value_length(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->default_value_length;
}

int mylite_ast_create_table_column_view_has_default_unsigned_integer(
    const MyliteAstCreateTableColumn *column) {
  return column != NULL && column->has_default_unsigned_integer_value;
}

unsigned long long
mylite_ast_create_table_column_view_default_unsigned_integer_value(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->default_unsigned_integer_value;
}

size_t mylite_ast_create_table_column_view_on_update_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->on_update_start;
}

size_t mylite_ast_create_table_column_view_on_update_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->on_update_end;
}

const MyliteAstNode *mylite_ast_create_table_column_view_on_update_node(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->on_update_node;
}

size_t mylite_ast_create_table_column_view_on_update_value_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->on_update_value_start;
}

size_t mylite_ast_create_table_column_view_on_update_value_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->on_update_value_end;
}

const MyliteAstNode *
mylite_ast_create_table_column_view_on_update_value_node(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->on_update_value_node;
}

MyliteCreateTableColumnValueKind
mylite_ast_create_table_column_view_on_update_value_kind(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? MYLITE_CREATE_TABLE_COLUMN_VALUE_UNKNOWN
                        : column->on_update_value_kind;
}

const char *mylite_ast_create_table_column_view_on_update_value(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->on_update_value;
}

size_t mylite_ast_create_table_column_view_on_update_value_length(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->on_update_value_length;
}

size_t mylite_ast_create_table_column_view_generated_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->generated_start;
}

size_t mylite_ast_create_table_column_view_generated_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->generated_end;
}

const MyliteAstNode *mylite_ast_create_table_column_view_generated_node(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->generated_node;
}

size_t mylite_ast_create_table_column_view_generated_expression_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->generated_expression_start;
}

size_t mylite_ast_create_table_column_view_generated_expression_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->generated_expression_end;
}

const MyliteAstNode *
mylite_ast_create_table_column_view_generated_expression_node(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->generated_expression_node;
}

size_t mylite_ast_create_table_column_view_generated_storage_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->generated_storage_start;
}

size_t mylite_ast_create_table_column_view_generated_storage_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->generated_storage_end;
}

const MyliteAstNode *
mylite_ast_create_table_column_view_generated_storage_node(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->generated_storage_node;
}

size_t mylite_ast_create_table_column_view_comment_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->comment_start;
}

size_t mylite_ast_create_table_column_view_comment_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->comment_end;
}

const MyliteAstNode *mylite_ast_create_table_column_view_comment_node(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->comment_node;
}

size_t mylite_ast_create_table_column_view_comment_value_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->comment_value_start;
}

size_t mylite_ast_create_table_column_view_comment_value_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->comment_value_end;
}

const char *mylite_ast_create_table_column_view_comment_value(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->comment_value;
}

size_t mylite_ast_create_table_column_view_comment_value_length(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->comment_value_length;
}

size_t mylite_ast_create_table_column_view_check_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->check_start;
}

size_t mylite_ast_create_table_column_view_check_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->check_end;
}

const MyliteAstNode *mylite_ast_create_table_column_view_check_node(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->check_node;
}

size_t mylite_ast_create_table_column_view_check_expression_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->check_expression_start;
}

size_t mylite_ast_create_table_column_view_check_expression_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->check_expression_end;
}

const MyliteAstNode *
mylite_ast_create_table_column_view_check_expression_node(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->check_expression_node;
}

MyliteCreateTableCheckEnforcement
mylite_ast_create_table_column_view_check_enforcement(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_UNSPECIFIED
                        : column->check_enforcement;
}

size_t mylite_ast_create_table_column_view_check_enforcement_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->check_enforcement_start;
}

size_t mylite_ast_create_table_column_view_check_enforcement_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->check_enforcement_end;
}

const MyliteAstNode *
mylite_ast_create_table_column_view_check_enforcement_node(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->check_enforcement_node;
}

size_t mylite_ast_create_table_column_view_reference_start(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->reference_start;
}

size_t mylite_ast_create_table_column_view_reference_end(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? 0 : column->reference_end;
}

const MyliteAstNode *mylite_ast_create_table_column_view_reference_node(
    const MyliteAstCreateTableColumn *column) {
  return column == NULL ? NULL : column->reference_node;
}

MyliteCreateTableKeyKind mylite_ast_create_table_key_view_kind(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? MYLITE_CREATE_TABLE_KEY_UNKNOWN : key->kind;
}

size_t mylite_ast_create_table_key_view_start(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->start;
}

size_t mylite_ast_create_table_key_view_end(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->end;
}

size_t mylite_ast_create_table_key_view_constraint_name_start(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->constraint_name_start;
}

size_t mylite_ast_create_table_key_view_constraint_name_end(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->constraint_name_end;
}

const char *mylite_ast_create_table_key_view_constraint_name_value(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? NULL : key->constraint_name_value;
}

size_t mylite_ast_create_table_key_view_constraint_name_value_length(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->constraint_name_value_length;
}

size_t mylite_ast_create_table_key_view_name_start(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->name_start;
}

size_t mylite_ast_create_table_key_view_name_end(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->name_end;
}

const char *mylite_ast_create_table_key_view_name_value(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? NULL : key->name_value;
}

size_t mylite_ast_create_table_key_view_name_value_length(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->name_value_length;
}

size_t mylite_ast_create_table_key_view_index_type_start(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->index_type_start;
}

size_t mylite_ast_create_table_key_view_index_type_end(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->index_type_end;
}

MyliteCreateTableIndexType mylite_ast_create_table_key_view_index_type_kind(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? MYLITE_CREATE_TABLE_INDEX_TYPE_UNSPECIFIED
                     : key->index_type_kind;
}

MyliteCreateTableKeyVisibility mylite_ast_create_table_key_view_visibility(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? MYLITE_CREATE_TABLE_KEY_VISIBILITY_UNSPECIFIED
                     : key->visibility;
}

size_t mylite_ast_create_table_key_view_column_count(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->column_count;
}

size_t mylite_ast_create_table_key_view_referenced_column_count(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->referenced_column_count;
}

size_t mylite_ast_create_table_key_view_option_count(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->option_count;
}

const MyliteAstCreateTableKeyPart *mylite_ast_create_table_key_view_column_at(
    const MyliteAstCreateTableKey *key, size_t column_index) {
  if (key == NULL || column_index >= key->column_count) {
    return NULL;
  }
  return &key->columns[column_index];
}

size_t mylite_ast_create_table_key_view_referenced_table_start(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->referenced_table_start;
}

size_t mylite_ast_create_table_key_view_referenced_table_end(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->referenced_table_end;
}

size_t mylite_ast_create_table_key_view_referenced_table_schema_start(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->referenced_table_schema_start;
}

size_t mylite_ast_create_table_key_view_referenced_table_schema_end(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->referenced_table_schema_end;
}

const char *mylite_ast_create_table_key_view_referenced_table_schema_value(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? NULL : key->referenced_table_schema_value;
}

size_t mylite_ast_create_table_key_view_referenced_table_schema_value_length(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->referenced_table_schema_value_length;
}

size_t mylite_ast_create_table_key_view_referenced_table_name_start(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->referenced_table_name_start;
}

size_t mylite_ast_create_table_key_view_referenced_table_name_end(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->referenced_table_name_end;
}

const char *mylite_ast_create_table_key_view_referenced_table_name_value(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? NULL : key->referenced_table_name_value;
}

size_t mylite_ast_create_table_key_view_referenced_table_name_value_length(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->referenced_table_name_value_length;
}

const MyliteAstCreateTableKeyPart *
mylite_ast_create_table_key_view_referenced_column_at(
    const MyliteAstCreateTableKey *key, size_t column_index) {
  if (key == NULL || column_index >= key->referenced_column_count) {
    return NULL;
  }
  return &key->referenced_columns[column_index];
}

MyliteCreateTableForeignMatchKind
mylite_ast_create_table_key_view_foreign_match_kind(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? MYLITE_CREATE_TABLE_FOREIGN_MATCH_UNSPECIFIED
                     : key->foreign_match_kind;
}

size_t mylite_ast_create_table_key_view_foreign_match_start(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->foreign_match_start;
}

size_t mylite_ast_create_table_key_view_foreign_match_end(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->foreign_match_end;
}

MyliteCreateTableForeignAction
mylite_ast_create_table_key_view_foreign_on_delete_action(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? MYLITE_CREATE_TABLE_FOREIGN_ACTION_UNSPECIFIED
                     : key->foreign_on_delete_action;
}

size_t mylite_ast_create_table_key_view_foreign_on_delete_start(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->foreign_on_delete_start;
}

size_t mylite_ast_create_table_key_view_foreign_on_delete_end(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->foreign_on_delete_end;
}

MyliteCreateTableForeignAction
mylite_ast_create_table_key_view_foreign_on_update_action(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? MYLITE_CREATE_TABLE_FOREIGN_ACTION_UNSPECIFIED
                     : key->foreign_on_update_action;
}

size_t mylite_ast_create_table_key_view_foreign_on_update_start(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->foreign_on_update_start;
}

size_t mylite_ast_create_table_key_view_foreign_on_update_end(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->foreign_on_update_end;
}

size_t mylite_ast_create_table_key_view_check_expression_start(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->check_expression_start;
}

size_t mylite_ast_create_table_key_view_check_expression_end(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->check_expression_end;
}

MyliteCreateTableCheckEnforcement
mylite_ast_create_table_key_view_check_enforcement(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_UNSPECIFIED
                     : key->check_enforcement;
}

size_t mylite_ast_create_table_key_view_check_enforcement_start(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->check_enforcement_start;
}

size_t mylite_ast_create_table_key_view_check_enforcement_end(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? 0 : key->check_enforcement_end;
}

const MyliteAstCreateTableKeyOption *
mylite_ast_create_table_key_view_option_at(
    const MyliteAstCreateTableKey *key, size_t option_index) {
  if (key == NULL || option_index >= key->option_count) {
    return NULL;
  }
  return &key->options[option_index];
}

const MyliteAstCreateTableKeyOption *
mylite_ast_create_table_key_view_comment_option(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? NULL : key->comment_option;
}

const char *mylite_ast_create_table_key_view_comment_value(
    const MyliteAstCreateTableKey *key) {
  const MyliteAstCreateTableKeyOption *option =
      mylite_ast_create_table_key_view_comment_option(key);
  return option == NULL ? NULL : option->value;
}

size_t mylite_ast_create_table_key_view_comment_value_length(
    const MyliteAstCreateTableKey *key) {
  const MyliteAstCreateTableKeyOption *option =
      mylite_ast_create_table_key_view_comment_option(key);
  return option == NULL ? 0 : option->value_length;
}

const MyliteAstCreateTableKeyOption *
mylite_ast_create_table_key_view_parser_option(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? NULL : key->parser_option;
}

const char *mylite_ast_create_table_key_view_parser_value(
    const MyliteAstCreateTableKey *key) {
  const MyliteAstCreateTableKeyOption *option =
      mylite_ast_create_table_key_view_parser_option(key);
  return option == NULL ? NULL : option->value;
}

size_t mylite_ast_create_table_key_view_parser_value_length(
    const MyliteAstCreateTableKey *key) {
  const MyliteAstCreateTableKeyOption *option =
      mylite_ast_create_table_key_view_parser_option(key);
  return option == NULL ? 0 : option->value_length;
}

const MyliteAstCreateTableKeyOption *
mylite_ast_create_table_key_view_key_block_size_option(
    const MyliteAstCreateTableKey *key) {
  return key == NULL ? NULL : key->key_block_size_option;
}

int mylite_ast_create_table_key_view_has_key_block_size_value(
    const MyliteAstCreateTableKey *key) {
  const MyliteAstCreateTableKeyOption *option =
      mylite_ast_create_table_key_view_key_block_size_option(key);
  return option != NULL && option->has_unsigned_integer_value;
}

unsigned long long mylite_ast_create_table_key_view_key_block_size_value(
    const MyliteAstCreateTableKey *key) {
  const MyliteAstCreateTableKeyOption *option =
      mylite_ast_create_table_key_view_key_block_size_option(key);
  return option == NULL ? 0 : option->unsigned_integer_value;
}

MyliteCreateTableKeyPartKind mylite_ast_create_table_key_part_view_kind(
    const MyliteAstCreateTableKeyPart *part) {
  return part == NULL ? MYLITE_CREATE_TABLE_KEY_PART_UNKNOWN : part->kind;
}

size_t mylite_ast_create_table_key_part_view_start(
    const MyliteAstCreateTableKeyPart *part) {
  return part == NULL ? 0 : part->start;
}

size_t mylite_ast_create_table_key_part_view_end(
    const MyliteAstCreateTableKeyPart *part) {
  return part == NULL ? 0 : part->end;
}

size_t mylite_ast_create_table_key_part_view_name_start(
    const MyliteAstCreateTableKeyPart *part) {
  return part == NULL ? 0 : part->name_start;
}

size_t mylite_ast_create_table_key_part_view_name_end(
    const MyliteAstCreateTableKeyPart *part) {
  return part == NULL ? 0 : part->name_end;
}

const char *mylite_ast_create_table_key_part_view_name_value(
    const MyliteAstCreateTableKeyPart *part) {
  return part == NULL ? NULL : part->name_value;
}

size_t mylite_ast_create_table_key_part_view_name_value_length(
    const MyliteAstCreateTableKeyPart *part) {
  return part == NULL ? 0 : part->name_value_length;
}

size_t mylite_ast_create_table_key_part_view_expression_start(
    const MyliteAstCreateTableKeyPart *part) {
  return part == NULL ? 0 : part->expression_start;
}

size_t mylite_ast_create_table_key_part_view_expression_end(
    const MyliteAstCreateTableKeyPart *part) {
  return part == NULL ? 0 : part->expression_end;
}

size_t mylite_ast_create_table_key_part_view_prefix_start(
    const MyliteAstCreateTableKeyPart *part) {
  return part == NULL ? 0 : part->prefix_start;
}

size_t mylite_ast_create_table_key_part_view_prefix_end(
    const MyliteAstCreateTableKeyPart *part) {
  return part == NULL ? 0 : part->prefix_end;
}

size_t mylite_ast_create_table_key_part_view_prefix_value_start(
    const MyliteAstCreateTableKeyPart *part) {
  return part == NULL ? 0 : part->prefix_value_start;
}

size_t mylite_ast_create_table_key_part_view_prefix_value_end(
    const MyliteAstCreateTableKeyPart *part) {
  return part == NULL ? 0 : part->prefix_value_end;
}

MyliteCreateTableKeyPartOrder mylite_ast_create_table_key_part_view_order(
    const MyliteAstCreateTableKeyPart *part) {
  return part == NULL ? MYLITE_CREATE_TABLE_KEY_PART_ORDER_UNSPECIFIED
                      : part->order;
}

size_t mylite_ast_create_table_key_part_view_order_start(
    const MyliteAstCreateTableKeyPart *part) {
  return part == NULL ? 0 : part->order_start;
}

size_t mylite_ast_create_table_key_part_view_order_end(
    const MyliteAstCreateTableKeyPart *part) {
  return part == NULL ? 0 : part->order_end;
}

MyliteCreateTableKeyOptionKind mylite_ast_create_table_key_option_view_kind(
    const MyliteAstCreateTableKeyOption *option) {
  return option == NULL ? MYLITE_CREATE_TABLE_KEY_OPTION_UNKNOWN
                        : option->kind;
}

size_t mylite_ast_create_table_key_option_view_start(
    const MyliteAstCreateTableKeyOption *option) {
  return option == NULL ? 0 : option->start;
}

size_t mylite_ast_create_table_key_option_view_end(
    const MyliteAstCreateTableKeyOption *option) {
  return option == NULL ? 0 : option->end;
}

size_t mylite_ast_create_table_key_option_view_name_start(
    const MyliteAstCreateTableKeyOption *option) {
  return option == NULL ? 0 : option->name_start;
}

size_t mylite_ast_create_table_key_option_view_name_end(
    const MyliteAstCreateTableKeyOption *option) {
  return option == NULL ? 0 : option->name_end;
}

size_t mylite_ast_create_table_key_option_view_value_start(
    const MyliteAstCreateTableKeyOption *option) {
  return option == NULL ? 0 : option->value_start;
}

size_t mylite_ast_create_table_key_option_view_value_end(
    const MyliteAstCreateTableKeyOption *option) {
  return option == NULL ? 0 : option->value_end;
}

MyliteCreateTableKeyOptionValueKind
mylite_ast_create_table_key_option_view_value_kind(
    const MyliteAstCreateTableKeyOption *option) {
  return option == NULL ? MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_UNKNOWN
                        : option->value_kind;
}

const char *mylite_ast_create_table_key_option_view_value(
    const MyliteAstCreateTableKeyOption *option) {
  return option == NULL ? NULL : option->value;
}

size_t mylite_ast_create_table_key_option_view_value_length(
    const MyliteAstCreateTableKeyOption *option) {
  return option == NULL ? 0 : option->value_length;
}

int mylite_ast_create_table_key_option_view_has_unsigned_integer(
    const MyliteAstCreateTableKeyOption *option) {
  return option != NULL && option->has_unsigned_integer_value;
}

unsigned long long
mylite_ast_create_table_key_option_view_unsigned_integer_value(
    const MyliteAstCreateTableKeyOption *option) {
  return option == NULL ? 0 : option->unsigned_integer_value;
}

MyliteCreateTableIndexType
mylite_ast_create_table_key_option_view_index_type_kind(
    const MyliteAstCreateTableKeyOption *option) {
  return option == NULL ? MYLITE_CREATE_TABLE_INDEX_TYPE_UNSPECIFIED
                        : option->index_type_kind;
}

MyliteCreateTableOptionKind mylite_ast_create_table_option_view_kind(
    const MyliteAstCreateTableOption *option) {
  return option == NULL ? MYLITE_CREATE_TABLE_OPTION_UNKNOWN : option->kind;
}

size_t mylite_ast_create_table_option_view_start(
    const MyliteAstCreateTableOption *option) {
  return option == NULL ? 0 : option->start;
}

size_t mylite_ast_create_table_option_view_end(
    const MyliteAstCreateTableOption *option) {
  return option == NULL ? 0 : option->end;
}

size_t mylite_ast_create_table_option_view_name_start(
    const MyliteAstCreateTableOption *option) {
  return option == NULL ? 0 : option->name_start;
}

size_t mylite_ast_create_table_option_view_name_end(
    const MyliteAstCreateTableOption *option) {
  return option == NULL ? 0 : option->name_end;
}

size_t mylite_ast_create_table_option_view_value_start(
    const MyliteAstCreateTableOption *option) {
  return option == NULL ? 0 : option->value_start;
}

size_t mylite_ast_create_table_option_view_value_end(
    const MyliteAstCreateTableOption *option) {
  return option == NULL ? 0 : option->value_end;
}

MyliteCreateTableOptionValueKind
mylite_ast_create_table_option_view_value_kind(
    const MyliteAstCreateTableOption *option) {
  return option == NULL ? MYLITE_CREATE_TABLE_OPTION_VALUE_UNKNOWN
                        : option->value_kind;
}

const char *mylite_ast_create_table_option_view_value(
    const MyliteAstCreateTableOption *option) {
  return option == NULL ? NULL : option->value;
}

size_t mylite_ast_create_table_option_view_value_length(
    const MyliteAstCreateTableOption *option) {
  return option == NULL ? 0 : option->value_length;
}

int mylite_ast_create_table_option_view_has_unsigned_integer(
    const MyliteAstCreateTableOption *option) {
  return option != NULL && option->has_unsigned_integer_value;
}

unsigned long long
mylite_ast_create_table_option_view_unsigned_integer_value(
    const MyliteAstCreateTableOption *option) {
  return option == NULL ? 0 : option->unsigned_integer_value;
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
  const MyliteAstCreateTableColumnTypeElement *element =
      mylite_ast_create_table_column_type_element_at(
          ast, statement_index, column_index, element_index);
  return element == NULL ? 0 : element->start;
}

size_t mylite_ast_create_table_column_type_element_end(
    const MyliteAst *ast, size_t statement_index, size_t column_index,
    size_t element_index) {
  const MyliteAstCreateTableColumnTypeElement *element =
      mylite_ast_create_table_column_type_element_at(
          ast, statement_index, column_index, element_index);
  return element == NULL ? 0 : element->end;
}

const char *mylite_ast_create_table_column_type_element_value(
    const MyliteAst *ast, size_t statement_index, size_t column_index,
    size_t element_index) {
  const MyliteAstCreateTableColumnTypeElement *element =
      mylite_ast_create_table_column_type_element_at(
          ast, statement_index, column_index, element_index);
  return element == NULL ? NULL : element->value;
}

size_t mylite_ast_create_table_column_type_element_value_length(
    const MyliteAst *ast, size_t statement_index, size_t column_index,
    size_t element_index) {
  const MyliteAstCreateTableColumnTypeElement *element =
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

const char *mylite_ast_create_table_key_constraint_name_value(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? NULL : key->constraint_name_value;
}

size_t mylite_ast_create_table_key_constraint_name_value_length(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->constraint_name_value_length;
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

const char *mylite_ast_create_table_key_name_value(const MyliteAst *ast,
                                                   size_t statement_index,
                                                   size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? NULL : key->name_value;
}

size_t mylite_ast_create_table_key_name_value_length(const MyliteAst *ast,
                                                     size_t statement_index,
                                                     size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->name_value_length;
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

const char *mylite_ast_create_table_key_column_name_value(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 0);
  return part == NULL ? NULL : part->name_value;
}

size_t mylite_ast_create_table_key_column_name_value_length(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 0);
  return part == NULL ? 0 : part->name_value_length;
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

const char *mylite_ast_create_table_key_referenced_table_schema_value(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? NULL : key->referenced_table_schema_value;
}

size_t mylite_ast_create_table_key_referenced_table_schema_value_length(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->referenced_table_schema_value_length;
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

const char *mylite_ast_create_table_key_referenced_table_name_value(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? NULL : key->referenced_table_name_value;
}

size_t mylite_ast_create_table_key_referenced_table_name_value_length(
    const MyliteAst *ast, size_t statement_index, size_t key_index) {
  const MyliteAstCreateTableKey *key =
      mylite_ast_create_table_key_at(ast, statement_index, key_index);
  return key == NULL ? 0 : key->referenced_table_name_value_length;
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

const char *mylite_ast_create_table_key_referenced_column_name_value(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 1);
  return part == NULL ? NULL : part->name_value;
}

size_t mylite_ast_create_table_key_referenced_column_name_value_length(
    const MyliteAst *ast, size_t statement_index, size_t key_index,
    size_t column_index) {
  const MyliteAstCreateTableKeyPart *part =
      mylite_ast_create_table_key_part_at(ast, statement_index, key_index,
                                          column_index, 1);
  return part == NULL ? 0 : part->name_value_length;
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
  if (strcmp(symbol_name, "nt_prepare_stmt") == 0 ||
      strcmp(symbol_name, "nt_prepared_stmt") == 0) {
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
  if (strcmp(symbol_name, "nt_use_stmt") == 0) {
    return MYLITE_STATEMENT_USE;
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
           mylite_ast_collect_create_table_options(ast, statement, payload) &&
           mylite_ast_set_create_table_view(ast, statement, payload);
  }
  if (strcmp(statement->symbol_name, "nt_create_database_stmt") == 0) {
    return mylite_ast_set_create_database_view(ast, statement, payload);
  }
  if (strcmp(statement->symbol_name, "nt_alter_table_stmt") == 0) {
    return mylite_ast_set_alter_table_view(ast, statement, payload);
  }
  if (strcmp(statement->symbol_name, "nt_create_index_stmt") == 0) {
    return mylite_ast_set_create_index_view(ast, statement, payload);
  }
  if (strcmp(statement->symbol_name, "nt_create_view_stmt") == 0) {
    return mylite_ast_set_create_view_view(ast, statement, payload);
  }
  if (strcmp(statement->symbol_name, "nt_drop_index_stmt") == 0) {
    return mylite_ast_set_drop_index_view(ast, statement, payload);
  }
  if (strcmp(statement->symbol_name, "nt_drop_database_stmt") == 0) {
    return mylite_ast_set_drop_database_view(ast, statement, payload);
  }
  if (strcmp(statement->symbol_name, "nt_drop_table_stmt") == 0) {
    return mylite_ast_set_drop_table_view(ast, statement, payload);
  }
  if (strcmp(statement->symbol_name, "nt_drop_view_stmt") == 0) {
    return mylite_ast_set_drop_view_view(ast, statement, payload);
  }
  if (strcmp(statement->symbol_name, "nt_prepared_stmt") == 0 ||
      strcmp(statement->symbol_name, "nt_prepare_stmt") == 0) {
    return mylite_ast_set_prepare_statement_view(ast, statement, payload);
  }
  if (strcmp(statement->symbol_name, "nt_execute_stmt") == 0) {
    return mylite_ast_set_execute_statement_view(ast, statement, payload);
  }
  if (strcmp(statement->symbol_name, "nt_deallocate_stmt") == 0) {
    return mylite_ast_set_deallocate_statement_view(ast, statement, payload);
  }
  if (strcmp(statement->symbol_name, "nt_set_stmt") == 0 ||
      strcmp(statement->symbol_name, "nt_set_role_stmt") == 0 ||
      strcmp(statement->symbol_name, "nt_set_default_role_stmt") == 0) {
    return mylite_ast_set_set_statement_view(ast, statement, payload);
  }
  if (strcmp(statement->symbol_name, "nt_rename_table_stmt") == 0) {
    return mylite_ast_set_rename_table_view(ast, statement, payload);
  }
  if (strcmp(statement->symbol_name, "nt_truncate_table_stmt") == 0) {
    return mylite_ast_set_truncate_table_view(ast, statement, payload);
  }
  if (statement->kind == MYLITE_STATEMENT_TRANSACTION) {
    return mylite_ast_set_transaction_statement_view(ast, statement, payload);
  }
  if (strcmp(statement->symbol_name, "nt_use_stmt") == 0) {
    return mylite_ast_set_use_database_view(ast, statement, payload);
  }
  return 1;
}

static int mylite_ast_set_create_database_view(MyliteAst *ast,
                                               MyliteAstStatement *statement,
                                               const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL) {
    return 1;
  }
  MyliteAstCreateDatabase *create_database =
      mylite_ast_alloc(ast, sizeof(*create_database));
  if (create_database == NULL) {
    return 0;
  }
  create_database->node = payload == NULL ? statement->node : payload;
  create_database->start = mylite_ast_node_start(create_database->node);
  create_database->end = mylite_ast_node_end(create_database->node);
  if (statement->target_count > 0) {
    create_database->target = &statement->targets[0];
  }
  const MyliteAstNode *if_not_exists =
      mylite_ast_find_first_symbol(create_database->node, "nt_if_not_exists");
  create_database->has_if_not_exists =
      if_not_exists != NULL && if_not_exists->has_span;
  const MyliteAstNode *database_sym =
      mylite_ast_find_first_symbol(create_database->node, "nt_database_sym");
  create_database->uses_schema_keyword =
      mylite_ast_find_first_token(database_sym, MYLITE_TOK_SCHEMA) !=
      NULL;
  if (!mylite_ast_set_database_options(ast, create_database,
                                       create_database->node)) {
    return 0;
  }
  mylite_ast_set_database_option_summary(create_database);
  statement->create_database = create_database;
  return 1;
}

static void mylite_ast_set_database_option_summary(
    MyliteAstCreateDatabase *create_database) {
  if (create_database == NULL) {
    return;
  }
  for (size_t i = 0; i < create_database->option_count; i++) {
    const MyliteAstDatabaseOption *option = &create_database->options[i];
    switch (option->kind) {
      case MYLITE_DATABASE_OPTION_CHARSET:
        create_database->charset_option = option;
        break;
      case MYLITE_DATABASE_OPTION_COLLATE:
        create_database->collation_option = option;
        break;
      case MYLITE_DATABASE_OPTION_ENCRYPTION:
        create_database->encryption_option = option;
        break;
      case MYLITE_DATABASE_OPTION_UNKNOWN:
      case MYLITE_DATABASE_OPTION_PLACEMENT_POLICY:
      case MYLITE_DATABASE_OPTION_TI_FLASH_REPLICA:
      case MYLITE_DATABASE_OPTION_READ_ONLY:
        break;
    }
  }
}

static int mylite_ast_set_create_table_view(MyliteAst *ast,
                                            MyliteAstStatement *statement,
                                            const MyliteAstNode *payload) {
  if (statement == NULL) {
    return 1;
  }

  MyliteAstCreateTable *create_table =
      mylite_ast_alloc(ast, sizeof(*create_table));
  if (create_table == NULL) {
    return 0;
  }

  const MyliteAstStatementTarget *target =
      mylite_ast_create_table_target(statement);
  create_table->node = payload == NULL ? statement->node : payload;
  create_table->start = mylite_ast_node_start(create_table->node);
  create_table->end = mylite_ast_node_end(create_table->node);
  if (target != NULL) {
    create_table->target_start = target->start;
    create_table->target_end = target->end;
    create_table->schema_start = target->schema_start;
    create_table->schema_end = target->schema_end;
    create_table->schema_value = target->schema_value;
    create_table->schema_value_length = target->schema_value_length;
    create_table->name_start = target->name_start;
    create_table->name_end = target->name_end;
    create_table->name_value = target->name_value;
    create_table->name_value_length = target->name_value_length;
  }
  create_table->columns = statement->create_table_columns;
  create_table->column_count = statement->create_table_column_count;
  create_table->keys = statement->create_table_keys;
  create_table->key_count = statement->create_table_key_count;
  create_table->options = statement->create_table_options;
  create_table->option_count = statement->create_table_option_count;
  mylite_ast_set_create_table_option_summary(create_table);
  statement->create_table = create_table;
  return 1;
}

static void mylite_ast_set_create_table_option_summary(
    MyliteAstCreateTable *create_table) {
  if (create_table == NULL) {
    return;
  }
  for (size_t i = 0; i < create_table->option_count; i++) {
    const MyliteAstCreateTableOption *option = &create_table->options[i];
    switch (option->kind) {
      case MYLITE_CREATE_TABLE_OPTION_ENGINE:
        create_table->engine_option = option;
        break;
      case MYLITE_CREATE_TABLE_OPTION_CHARSET:
        create_table->charset_option = option;
        break;
      case MYLITE_CREATE_TABLE_OPTION_COLLATE:
        create_table->collation_option = option;
        break;
      case MYLITE_CREATE_TABLE_OPTION_COMMENT:
        create_table->comment_option = option;
        break;
      case MYLITE_CREATE_TABLE_OPTION_AUTO_INCREMENT:
        create_table->auto_increment_option = option;
        break;
      case MYLITE_CREATE_TABLE_OPTION_UNKNOWN:
      case MYLITE_CREATE_TABLE_OPTION_SECONDARY_ENGINE:
      case MYLITE_CREATE_TABLE_OPTION_ROW_FORMAT:
      case MYLITE_CREATE_TABLE_OPTION_KEY_BLOCK_SIZE:
      case MYLITE_CREATE_TABLE_OPTION_AUTOEXTEND_SIZE:
      case MYLITE_CREATE_TABLE_OPTION_AVG_ROW_LENGTH:
      case MYLITE_CREATE_TABLE_OPTION_MAX_ROWS:
      case MYLITE_CREATE_TABLE_OPTION_MIN_ROWS:
      case MYLITE_CREATE_TABLE_OPTION_DELAY_KEY_WRITE:
      case MYLITE_CREATE_TABLE_OPTION_ENCRYPTION:
      case MYLITE_CREATE_TABLE_OPTION_STATS_PERSISTENT:
      case MYLITE_CREATE_TABLE_OPTION_PACK_KEYS:
      case MYLITE_CREATE_TABLE_OPTION_TABLESPACE:
      case MYLITE_CREATE_TABLE_OPTION_STORAGE:
      case MYLITE_CREATE_TABLE_OPTION_COMPRESSION:
      case MYLITE_CREATE_TABLE_OPTION_CONNECTION:
      case MYLITE_CREATE_TABLE_OPTION_PASSWORD:
      case MYLITE_CREATE_TABLE_OPTION_INSERT_METHOD:
      case MYLITE_CREATE_TABLE_OPTION_DATA_DIRECTORY:
      case MYLITE_CREATE_TABLE_OPTION_INDEX_DIRECTORY:
      case MYLITE_CREATE_TABLE_OPTION_UNION:
      case MYLITE_CREATE_TABLE_OPTION_ENGINE_ATTRIBUTE:
      case MYLITE_CREATE_TABLE_OPTION_SECONDARY_ENGINE_ATTRIBUTE:
        break;
    }
  }
}

static const MyliteAstStatementTarget *mylite_ast_create_table_target(
    const MyliteAstStatement *statement) {
  if (statement == NULL) {
    return NULL;
  }
  for (size_t i = 0; i < statement->target_count; i++) {
    const MyliteAstStatementTarget *target = &statement->targets[i];
    if (target->kind == MYLITE_STATEMENT_TARGET_TABLE &&
        target->role == MYLITE_STATEMENT_TARGET_ROLE_PRIMARY) {
      return target;
    }
  }
  return NULL;
}

static int mylite_ast_set_alter_table_view(MyliteAst *ast,
                                           MyliteAstStatement *statement,
                                           const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL) {
    return 1;
  }

  MyliteAstAlterTable *alter_table =
      mylite_ast_alloc(ast, sizeof(*alter_table));
  if (alter_table == NULL) {
    return 0;
  }
  alter_table->node = payload == NULL ? statement->node : payload;
  alter_table->start = mylite_ast_node_start(alter_table->node);
  alter_table->end = mylite_ast_node_end(alter_table->node);
  alter_table->target = mylite_ast_alter_table_target(statement);

  alter_table->spec_count = mylite_ast_count_alter_table_specs(payload);
  if (alter_table->spec_count > 0) {
    alter_table->specs =
        mylite_ast_alloc(ast, alter_table->spec_count * sizeof(*alter_table->specs));
    if (alter_table->specs == NULL) {
      return 0;
    }
    size_t index = 0;
    int ok = 1;
    mylite_ast_fill_alter_table_specs(ast, alter_table, payload, &index, &ok);
    if (!ok || index != alter_table->spec_count) {
      return 0;
    }
  }

  if (!mylite_ast_set_alter_table_options(ast, alter_table, payload)) {
    return 0;
  }
  statement->alter_table = alter_table;
  return 1;
}

static const MyliteAstStatementTarget *mylite_ast_alter_table_target(
    const MyliteAstStatement *statement) {
  if (statement == NULL) {
    return NULL;
  }
  for (size_t i = 0; i < statement->target_count; i++) {
    const MyliteAstStatementTarget *target = &statement->targets[i];
    if (target->kind == MYLITE_STATEMENT_TARGET_TABLE &&
        target->role == MYLITE_STATEMENT_TARGET_ROLE_PRIMARY) {
      return target;
    }
  }
  return NULL;
}

static size_t mylite_ast_count_alter_table_specs(const MyliteAstNode *node) {
  if (node == NULL) {
    return 0;
  }
  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_alter_table_spec") == 0) {
    return 1;
  }
  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_alter_table_spec_single_opt") == 0) {
    return node->has_span && node->end > node->start ? 1 : 0;
  }

  size_t count = 0;
  for (size_t i = 0; i < node->child_count; i++) {
    count += mylite_ast_count_alter_table_specs(node->children[i]);
  }
  return count;
}

static void mylite_ast_fill_alter_table_specs(MyliteAst *ast,
                                              MyliteAstAlterTable *alter_table,
                                              const MyliteAstNode *node,
                                              size_t *index, int *ok) {
  if (alter_table == NULL || node == NULL || index == NULL || ok == NULL ||
      !*ok || *index >= alter_table->spec_count) {
    return;
  }
  if (node->symbol_name != NULL &&
      (strcmp(node->symbol_name, "nt_alter_table_spec") == 0 ||
       (strcmp(node->symbol_name, "nt_alter_table_spec_single_opt") == 0 &&
        node->has_span && node->end > node->start))) {
    *ok = mylite_ast_fill_alter_table_spec(ast, &alter_table->specs[*index],
                                           node);
    if (*ok) {
      (*index)++;
    }
    return;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    mylite_ast_fill_alter_table_specs(ast, alter_table, node->children[i],
                                      index, ok);
    if (!*ok) {
      return;
    }
  }
}

static int mylite_ast_fill_alter_table_spec(MyliteAst *ast,
                                            MyliteAstAlterTableSpec *spec,
                                            const MyliteAstNode *node) {
  if (spec == NULL || node == NULL) {
    return 1;
  }
  spec->node = node;
  spec->kind = mylite_ast_classify_alter_table_spec(node);
  spec->start = mylite_ast_node_start(node);
  spec->end = mylite_ast_node_end(node);
  const MyliteAstNode *if_exists =
      mylite_ast_find_first_symbol(node, "nt_if_exists");
  const MyliteAstNode *if_not_exists =
      mylite_ast_find_first_symbol(node, "nt_if_not_exists");
  spec->has_if_exists = if_exists != NULL && if_exists->has_span;
  spec->has_if_not_exists = if_not_exists != NULL && if_not_exists->has_span;
  mylite_ast_set_alter_table_spec_spans(spec, node);
  return mylite_ast_set_alter_table_spec_values(ast, spec) &&
         mylite_ast_set_alter_table_spec_payload(ast, spec, node);
}

static int mylite_ast_set_alter_table_spec_payload(
    MyliteAst *ast, MyliteAstAlterTableSpec *spec, const MyliteAstNode *node) {
  if (spec == NULL || node == NULL) {
    return 1;
  }
  switch (spec->kind) {
    case MYLITE_ALTER_TABLE_SPEC_ADD_COLUMN:
    case MYLITE_ALTER_TABLE_SPEC_MODIFY_COLUMN:
    case MYLITE_ALTER_TABLE_SPEC_CHANGE_COLUMN:
      return mylite_ast_set_alter_table_spec_column(ast, spec, node);
    case MYLITE_ALTER_TABLE_SPEC_ADD_CONSTRAINT:
      return mylite_ast_set_alter_table_spec_key(ast, spec, node);
    case MYLITE_ALTER_TABLE_SPEC_ADD_TABLE_ELEMENTS:
      return mylite_ast_set_alter_table_spec_table_elements(ast, spec, node);
    case MYLITE_ALTER_TABLE_SPEC_UNKNOWN:
    case MYLITE_ALTER_TABLE_SPEC_TABLE_OPTIONS:
    case MYLITE_ALTER_TABLE_SPEC_CONVERT_CHARACTER_SET:
    case MYLITE_ALTER_TABLE_SPEC_ADD_PARTITION:
    case MYLITE_ALTER_TABLE_SPEC_DROP_COLUMN:
    case MYLITE_ALTER_TABLE_SPEC_DROP_PRIMARY_KEY:
    case MYLITE_ALTER_TABLE_SPEC_DROP_INDEX:
    case MYLITE_ALTER_TABLE_SPEC_DROP_FOREIGN_KEY:
    case MYLITE_ALTER_TABLE_SPEC_DROP_CHECK:
    case MYLITE_ALTER_TABLE_SPEC_DROP_PARTITION:
    case MYLITE_ALTER_TABLE_SPEC_ALTER_COLUMN_SET_DEFAULT:
    case MYLITE_ALTER_TABLE_SPEC_ALTER_COLUMN_DROP_DEFAULT:
    case MYLITE_ALTER_TABLE_SPEC_ALTER_COLUMN_VISIBILITY:
    case MYLITE_ALTER_TABLE_SPEC_RENAME_COLUMN:
    case MYLITE_ALTER_TABLE_SPEC_RENAME_TABLE:
    case MYLITE_ALTER_TABLE_SPEC_RENAME_INDEX:
    case MYLITE_ALTER_TABLE_SPEC_ORDER_BY:
    case MYLITE_ALTER_TABLE_SPEC_DISABLE_KEYS:
    case MYLITE_ALTER_TABLE_SPEC_ENABLE_KEYS:
    case MYLITE_ALTER_TABLE_SPEC_LOCK:
    case MYLITE_ALTER_TABLE_SPEC_ALGORITHM:
    case MYLITE_ALTER_TABLE_SPEC_FORCE:
    case MYLITE_ALTER_TABLE_SPEC_VALIDATION:
    case MYLITE_ALTER_TABLE_SPEC_ALTER_CHECK:
    case MYLITE_ALTER_TABLE_SPEC_ALTER_INDEX_VISIBILITY:
    case MYLITE_ALTER_TABLE_SPEC_TABLESPACE:
    case MYLITE_ALTER_TABLE_SPEC_PARTITION:
    case MYLITE_ALTER_TABLE_SPEC_SECONDARY_LOAD:
    case MYLITE_ALTER_TABLE_SPEC_SECONDARY_UNLOAD:
      return 1;
  }
  return 1;
}

static int mylite_ast_set_alter_table_spec_column(
    MyliteAst *ast, MyliteAstAlterTableSpec *spec, const MyliteAstNode *node) {
  const MyliteAstNode *column_def =
      mylite_ast_find_first_symbol(node, "nt_column_def");
  if (column_def == NULL) {
    return 1;
  }
  if (!mylite_ast_fill_create_table_column(ast, &spec->column, column_def)) {
    return 0;
  }
  spec->columns = &spec->column;
  spec->column_count = 1;
  spec->has_column = 1;
  return 1;
}

static int mylite_ast_set_alter_table_spec_key(
    MyliteAst *ast, MyliteAstAlterTableSpec *spec, const MyliteAstNode *node) {
  const MyliteAstNode *constraint =
      mylite_ast_find_first_symbol(node, "nt_constraint_with_columnar_index");
  if (constraint == NULL) {
    constraint = mylite_ast_find_first_symbol(node, "nt_constraint");
  }
  if (constraint == NULL) {
    return 1;
  }
  if (!mylite_ast_fill_create_table_key(ast, &spec->key, constraint)) {
    return 0;
  }
  spec->keys = &spec->key;
  spec->key_count = 1;
  spec->has_key = 1;
  return 1;
}

static int mylite_ast_set_alter_table_spec_table_elements(
    MyliteAst *ast, MyliteAstAlterTableSpec *spec, const MyliteAstNode *node) {
  const MyliteAstNode *list =
      mylite_ast_find_first_symbol(node, "nt_table_element_list");
  if (ast == NULL || spec == NULL || list == NULL) {
    return 1;
  }

  spec->column_count = mylite_ast_count_create_table_columns(list);
  if (spec->column_count > 0) {
    spec->columns =
        mylite_ast_alloc(ast, spec->column_count * sizeof(*spec->columns));
    if (spec->columns == NULL) {
      return 0;
    }
    MyliteAstStatement scratch = {0};
    scratch.create_table_columns = spec->columns;
    scratch.create_table_column_count = spec->column_count;
    size_t index = 0;
    if (!mylite_ast_fill_create_table_columns(ast, &scratch, list, &index) ||
        index != spec->column_count) {
      return 0;
    }
    spec->has_column = 1;
  }

  spec->key_count = mylite_ast_count_create_table_keys(list);
  if (spec->key_count > 0) {
    spec->keys = mylite_ast_alloc(ast, spec->key_count * sizeof(*spec->keys));
    if (spec->keys == NULL) {
      return 0;
    }
    MyliteAstStatement scratch = {0};
    scratch.create_table_keys = spec->keys;
    scratch.create_table_key_count = spec->key_count;
    size_t index = 0;
    int ok = 1;
    mylite_ast_fill_create_table_keys(ast, &scratch, list, &index, &ok);
    if (!ok || index != spec->key_count) {
      return 0;
    }
    spec->has_key = 1;
  }
  return 1;
}

static MyliteAlterTableSpecKind mylite_ast_classify_alter_table_spec(
    const MyliteAstNode *node) {
  if (node == NULL) {
    return MYLITE_ALTER_TABLE_SPEC_UNKNOWN;
  }
  int first_token = mylite_ast_first_token(node);
  switch (first_token) {
    case MYLITE_TOK_ADD:
      if (mylite_ast_find_first_token(node, MYLITE_TOK_PARTITION) != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_ADD_PARTITION;
      }
      if (mylite_ast_find_first_symbol(node, "nt_table_element_list") != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_ADD_TABLE_ELEMENTS;
      }
      if (mylite_ast_find_first_symbol(node, "nt_column_def") != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_ADD_COLUMN;
      }
      if (mylite_ast_find_first_symbol(node, "nt_constraint_with_columnar_index") !=
              NULL ||
          mylite_ast_find_first_symbol(node, "nt_constraint") != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_ADD_CONSTRAINT;
      }
      break;
    case MYLITE_TOK_CONVERT:
      return MYLITE_ALTER_TABLE_SPEC_CONVERT_CHARACTER_SET;
    case MYLITE_TOK_DROP:
      if (mylite_ast_find_first_token(node, MYLITE_TOK_PRIMARY) != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_DROP_PRIMARY_KEY;
      }
      if (mylite_ast_find_first_token(node, MYLITE_TOK_FOREIGN) != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_DROP_FOREIGN_KEY;
      }
      if (mylite_ast_find_first_token(node, MYLITE_TOK_CHECK) != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_DROP_CHECK;
      }
      if (mylite_ast_find_first_token(node, MYLITE_TOK_PARTITION) != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_DROP_PARTITION;
      }
      if (mylite_ast_find_first_token(node, MYLITE_TOK_KEY) != NULL ||
          mylite_ast_find_first_token(node, MYLITE_TOK_INDEX) != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_DROP_INDEX;
      }
      if (mylite_ast_find_first_symbol(node, "nt_column_name") != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_DROP_COLUMN;
      }
      break;
    case MYLITE_TOK_MODIFY:
      return MYLITE_ALTER_TABLE_SPEC_MODIFY_COLUMN;
    case MYLITE_TOK_CHANGE:
      return MYLITE_ALTER_TABLE_SPEC_CHANGE_COLUMN;
    case MYLITE_TOK_ALTER:
      if (mylite_ast_find_first_token(node, MYLITE_TOK_INDEX) != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_ALTER_INDEX_VISIBILITY;
      }
      if (mylite_ast_find_first_token(node, MYLITE_TOK_CHECK) != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_ALTER_CHECK;
      }
      if (mylite_ast_find_first_token(node, MYLITE_TOK_DEFAULT_KWD) != NULL) {
        return mylite_ast_find_first_token(node, MYLITE_TOK_DROP) != NULL
                   ? MYLITE_ALTER_TABLE_SPEC_ALTER_COLUMN_DROP_DEFAULT
                   : MYLITE_ALTER_TABLE_SPEC_ALTER_COLUMN_SET_DEFAULT;
      }
      if (mylite_ast_find_first_token(node, MYLITE_TOK_VISIBLE) != NULL ||
          mylite_ast_find_first_token(node, MYLITE_TOK_INVISIBLE) != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_ALTER_COLUMN_VISIBILITY;
      }
      break;
    case MYLITE_TOK_RENAME:
      if (mylite_ast_find_first_token(node, MYLITE_TOK_COLUMN) != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_RENAME_COLUMN;
      }
      if (mylite_ast_find_first_token(node, MYLITE_TOK_KEY) != NULL ||
          mylite_ast_find_first_token(node, MYLITE_TOK_INDEX) != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_RENAME_INDEX;
      }
      if (mylite_ast_find_first_symbol(node, "nt_table_name") != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_RENAME_TABLE;
      }
      break;
    case MYLITE_TOK_ORDER:
      return MYLITE_ALTER_TABLE_SPEC_ORDER_BY;
    case MYLITE_TOK_DISABLE:
      if (mylite_ast_find_first_token(node, MYLITE_TOK_KEYS) != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_DISABLE_KEYS;
      }
      break;
    case MYLITE_TOK_ENABLE:
      if (mylite_ast_find_first_token(node, MYLITE_TOK_KEYS) != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_ENABLE_KEYS;
      }
      break;
    case MYLITE_TOK_LOCK:
      return MYLITE_ALTER_TABLE_SPEC_LOCK;
    case MYLITE_TOK_ALGORITHM:
      return MYLITE_ALTER_TABLE_SPEC_ALGORITHM;
    case MYLITE_TOK_FORCE:
      return MYLITE_ALTER_TABLE_SPEC_FORCE;
    case MYLITE_TOK_WITH:
    case MYLITE_TOK_WITHOUT:
      if (mylite_ast_find_first_token(node, MYLITE_TOK_VALIDATION) != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_VALIDATION;
      }
      break;
    case MYLITE_TOK_IMPORT_KWD:
    case MYLITE_TOK_DISCARD:
      if (mylite_ast_find_first_token(node, MYLITE_TOK_TABLESPACE) != NULL) {
        return MYLITE_ALTER_TABLE_SPEC_TABLESPACE;
      }
      break;
    case MYLITE_TOK_PARTITION:
    case MYLITE_TOK_REMOVE:
    case MYLITE_TOK_REORGANIZE:
    case MYLITE_TOK_SPLIT:
    case MYLITE_TOK_MERGE:
    case MYLITE_TOK_FIRST:
    case MYLITE_TOK_LAST:
    case MYLITE_TOK_COALESCE:
    case MYLITE_TOK_EXCHANGE:
    case MYLITE_TOK_TRUNCATE:
    case MYLITE_TOK_OPTIMIZE:
    case MYLITE_TOK_REPAIR:
    case MYLITE_TOK_REBUILD:
      return MYLITE_ALTER_TABLE_SPEC_PARTITION;
    case MYLITE_TOK_SECONDARY_LOAD:
      return MYLITE_ALTER_TABLE_SPEC_SECONDARY_LOAD;
    case MYLITE_TOK_SECONDARY_UNLOAD:
      return MYLITE_ALTER_TABLE_SPEC_SECONDARY_UNLOAD;
  }
  if (mylite_ast_find_first_symbol(node, "nt_table_option") != NULL) {
    return MYLITE_ALTER_TABLE_SPEC_TABLE_OPTIONS;
  }
  return MYLITE_ALTER_TABLE_SPEC_UNKNOWN;
}

static void mylite_ast_set_alter_table_spec_spans(
    MyliteAstAlterTableSpec *spec, const MyliteAstNode *node) {
  if (spec == NULL || node == NULL) {
    return;
  }

  switch (spec->kind) {
    case MYLITE_ALTER_TABLE_SPEC_ADD_COLUMN:
    case MYLITE_ALTER_TABLE_SPEC_MODIFY_COLUMN: {
      const MyliteAstNode *column_def =
          mylite_ast_find_first_symbol(node, "nt_column_def");
      mylite_ast_set_alter_table_spec_name(
          spec, mylite_ast_find_first_symbol(column_def, "nt_column_name"));
      break;
    }
    case MYLITE_ALTER_TABLE_SPEC_CHANGE_COLUMN: {
      mylite_ast_set_alter_table_spec_name(
          spec, mylite_ast_find_first_symbol(node, "nt_column_name"));
      const MyliteAstNode *column_def =
          mylite_ast_find_first_symbol(node, "nt_column_def");
      mylite_ast_set_alter_table_spec_secondary_name(
          spec, mylite_ast_find_first_symbol(column_def, "nt_column_name"));
      break;
    }
    case MYLITE_ALTER_TABLE_SPEC_DROP_COLUMN:
    case MYLITE_ALTER_TABLE_SPEC_ALTER_COLUMN_SET_DEFAULT:
    case MYLITE_ALTER_TABLE_SPEC_ALTER_COLUMN_DROP_DEFAULT:
    case MYLITE_ALTER_TABLE_SPEC_ALTER_COLUMN_VISIBILITY:
      mylite_ast_set_alter_table_spec_name(
          spec, mylite_ast_find_first_symbol(node, "nt_column_name"));
      break;
    case MYLITE_ALTER_TABLE_SPEC_RENAME_COLUMN:
    case MYLITE_ALTER_TABLE_SPEC_RENAME_INDEX: {
      size_t remaining = 0;
      mylite_ast_set_alter_table_spec_name(
          spec, mylite_ast_find_nth_symbol(node, "nt_identifier", &remaining));
      remaining = 1;
      mylite_ast_set_alter_table_spec_secondary_name(
          spec, mylite_ast_find_nth_symbol(node, "nt_identifier", &remaining));
      break;
    }
    case MYLITE_ALTER_TABLE_SPEC_DROP_INDEX:
    case MYLITE_ALTER_TABLE_SPEC_ALTER_INDEX_VISIBILITY:
    case MYLITE_ALTER_TABLE_SPEC_ALTER_CHECK:
    case MYLITE_ALTER_TABLE_SPEC_DROP_CHECK:
      mylite_ast_set_alter_table_spec_name(
          spec, mylite_ast_find_first_symbol(node, "nt_identifier"));
      break;
    case MYLITE_ALTER_TABLE_SPEC_DROP_FOREIGN_KEY:
      mylite_ast_set_alter_table_spec_name(
          spec, mylite_ast_find_first_symbol(node, "nt_symbol"));
      break;
    case MYLITE_ALTER_TABLE_SPEC_RENAME_TABLE:
    case MYLITE_ALTER_TABLE_SPEC_PARTITION:
      mylite_ast_set_alter_table_spec_table(
          spec, mylite_ast_find_first_symbol(node, "nt_table_name"));
      break;
    case MYLITE_ALTER_TABLE_SPEC_UNKNOWN:
    case MYLITE_ALTER_TABLE_SPEC_TABLE_OPTIONS:
    case MYLITE_ALTER_TABLE_SPEC_CONVERT_CHARACTER_SET:
    case MYLITE_ALTER_TABLE_SPEC_ADD_TABLE_ELEMENTS:
    case MYLITE_ALTER_TABLE_SPEC_ADD_CONSTRAINT:
    case MYLITE_ALTER_TABLE_SPEC_ADD_PARTITION:
    case MYLITE_ALTER_TABLE_SPEC_DROP_PRIMARY_KEY:
    case MYLITE_ALTER_TABLE_SPEC_DROP_PARTITION:
    case MYLITE_ALTER_TABLE_SPEC_ORDER_BY:
    case MYLITE_ALTER_TABLE_SPEC_DISABLE_KEYS:
    case MYLITE_ALTER_TABLE_SPEC_ENABLE_KEYS:
    case MYLITE_ALTER_TABLE_SPEC_LOCK:
    case MYLITE_ALTER_TABLE_SPEC_ALGORITHM:
    case MYLITE_ALTER_TABLE_SPEC_FORCE:
    case MYLITE_ALTER_TABLE_SPEC_VALIDATION:
    case MYLITE_ALTER_TABLE_SPEC_TABLESPACE:
    case MYLITE_ALTER_TABLE_SPEC_SECONDARY_LOAD:
    case MYLITE_ALTER_TABLE_SPEC_SECONDARY_UNLOAD:
      break;
  }
}

static int mylite_ast_set_alter_table_spec_values(
    MyliteAst *ast, MyliteAstAlterTableSpec *spec) {
  if (spec == NULL) {
    return 1;
  }
  if (!mylite_ast_decode_identifier(ast, spec->name_start, spec->name_end,
                                    &spec->name_value,
                                    &spec->name_value_length)) {
    return 0;
  }
  if (!mylite_ast_decode_identifier(ast, spec->secondary_name_start,
                                    spec->secondary_name_end,
                                    &spec->secondary_name_value,
                                    &spec->secondary_name_value_length)) {
    return 0;
  }
  if (!mylite_ast_decode_identifier(ast, spec->table_schema_start,
                                    spec->table_schema_end,
                                    &spec->table_schema_value,
                                    &spec->table_schema_value_length)) {
    return 0;
  }
  return mylite_ast_decode_identifier(ast, spec->table_name_start,
                                      spec->table_name_end,
                                      &spec->table_name_value,
                                      &spec->table_name_value_length);
}

static void mylite_ast_set_alter_table_spec_name(
    MyliteAstAlterTableSpec *spec, const MyliteAstNode *node) {
  const MyliteAstNode *identifier = mylite_ast_find_identifier_for_name(node);
  if (spec == NULL || identifier == NULL || !identifier->has_span) {
    return;
  }
  spec->name_start = mylite_ast_node_start(identifier);
  spec->name_end = mylite_ast_node_end(identifier);
}

static void mylite_ast_set_alter_table_spec_secondary_name(
    MyliteAstAlterTableSpec *spec, const MyliteAstNode *node) {
  const MyliteAstNode *identifier = mylite_ast_find_identifier_for_name(node);
  if (spec == NULL || identifier == NULL || !identifier->has_span) {
    return;
  }
  spec->secondary_name_start = mylite_ast_node_start(identifier);
  spec->secondary_name_end = mylite_ast_node_end(identifier);
}

static void mylite_ast_set_alter_table_spec_table(
    MyliteAstAlterTableSpec *spec, const MyliteAstNode *node) {
  if (spec == NULL || node == NULL || !node->has_span) {
    return;
  }
  spec->table_start = mylite_ast_node_start(node);
  spec->table_end = mylite_ast_node_end(node);
  mylite_ast_set_table_name_span_parts(node, &spec->table_schema_start,
                                       &spec->table_schema_end,
                                       &spec->table_name_start,
                                       &spec->table_name_end);
}

static const MyliteAstNode *mylite_ast_find_identifier_for_name(
    const MyliteAstNode *node) {
  if (node == NULL) {
    return NULL;
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, "nt_identifier") == 0) {
    return node;
  }
  const MyliteAstNode *identifier =
      mylite_ast_find_first_symbol(node, "nt_identifier");
  return identifier != NULL ? identifier : node;
}

static int mylite_ast_set_alter_table_options(MyliteAst *ast,
                                              MyliteAstAlterTable *alter_table,
                                              const MyliteAstNode *payload) {
  if (ast == NULL || alter_table == NULL || payload == NULL) {
    return 1;
  }
  alter_table->option_count = mylite_ast_count_create_table_options(payload);
  if (alter_table->option_count == 0) {
    return 1;
  }
  alter_table->options =
      mylite_ast_alloc(ast, alter_table->option_count * sizeof(*alter_table->options));
  if (alter_table->options == NULL) {
    return 0;
  }

  size_t index = 0;
  int ok = 1;
  mylite_ast_fill_alter_table_options(ast, alter_table, payload, &index, &ok);
  return ok && index == alter_table->option_count;
}

static void mylite_ast_fill_alter_table_options(
    MyliteAst *ast, MyliteAstAlterTable *alter_table,
    const MyliteAstNode *node, size_t *index, int *ok) {
  if (alter_table == NULL || node == NULL || index == NULL || ok == NULL ||
      !*ok || *index >= alter_table->option_count) {
    return;
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, "nt_table_option") == 0) {
    *ok = mylite_ast_fill_create_table_option(ast,
                                              &alter_table->options[*index],
                                              node);
    if (*ok) {
      (*index)++;
    }
    return;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    mylite_ast_fill_alter_table_options(ast, alter_table, node->children[i],
                                        index, ok);
    if (!*ok) {
      return;
    }
  }
}

static int mylite_ast_set_create_index_view(MyliteAst *ast,
                                            MyliteAstStatement *statement,
                                            const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL) {
    return 1;
  }

  MyliteAstCreateIndex *create_index =
      mylite_ast_alloc(ast, sizeof(*create_index));
  if (create_index == NULL) {
    return 0;
  }
  create_index->node = payload == NULL ? statement->node : payload;
  create_index->start = mylite_ast_node_start(create_index->node);
  create_index->end = mylite_ast_node_end(create_index->node);
  if (statement->target_count > 0) {
    create_index->target = &statement->targets[0];
  }
  if (!mylite_ast_set_create_index_key(ast, &create_index->key,
                                       create_index->node)) {
    return 0;
  }
  statement->create_index = create_index;
  return 1;
}

static int mylite_ast_set_create_index_key(MyliteAst *ast,
                                           MyliteAstCreateTableKey *key,
                                           const MyliteAstNode *payload) {
  if (key == NULL || payload == NULL) {
    return 1;
  }

  key->kind = mylite_ast_classify_create_table_key(payload);
  key->start = mylite_ast_node_start(payload);
  key->end = mylite_ast_node_end(payload);

  const MyliteAstNode *name = mylite_ast_find_first_symbol(payload,
                                                           "nt_identifier");
  if (name != NULL && name->has_span) {
    key->name_start = mylite_ast_node_start(name);
    key->name_end = mylite_ast_node_end(name);
    if (!mylite_ast_set_create_table_key_name_values(ast, key)) {
      return 0;
    }
  }

  mylite_ast_set_create_table_key_index_type(key, payload);

  size_t remaining = 0;
  const MyliteAstNode *parts =
      mylite_ast_find_nth_symbol(payload, "nt_index_part_specification_list",
                                 &remaining);
  if (!mylite_ast_set_create_table_key_parts(ast, key, parts, 0)) {
    return 0;
  }

  const MyliteAstNode *options =
      mylite_ast_find_first_symbol(payload, "nt_index_option_list");
  if (!mylite_ast_set_create_table_key_options(ast, key, options)) {
    return 0;
  }
  mylite_ast_set_create_table_key_summary(key);
  return 1;
}

static int mylite_ast_set_create_view_view(MyliteAst *ast,
                                           MyliteAstStatement *statement,
                                           const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL) {
    return 1;
  }

  MyliteAstCreateView *create_view =
      mylite_ast_alloc(ast, sizeof(*create_view));
  if (create_view == NULL) {
    return 0;
  }
  create_view->node = payload == NULL ? statement->node : payload;
  create_view->start = mylite_ast_node_start(create_view->node);
  create_view->end = mylite_ast_node_end(create_view->node);
  if (statement->target_count > 0) {
    create_view->target = &statement->targets[0];
  }

  const MyliteAstNode *or_replace =
      mylite_ast_find_first_symbol(create_view->node, "nt_or_replace");
  create_view->has_or_replace = or_replace != NULL && or_replace->has_span;
  const MyliteAstNode *algorithm =
      mylite_ast_find_first_symbol(create_view->node, "nt_view_algorithm");
  create_view->algorithm = mylite_ast_classify_create_view_algorithm(algorithm);
  const MyliteAstNode *security =
      mylite_ast_find_first_symbol(create_view->node, "nt_view_sql_security");
  create_view->sql_security = mylite_ast_classify_view_sql_security(security);
  const MyliteAstNode *check_option =
      mylite_ast_find_first_symbol(create_view->node, "nt_view_check_option");
  create_view->check_option = mylite_ast_classify_view_check_option(check_option);

  const MyliteAstNode *definer =
      mylite_ast_find_first_symbol(create_view->node, "nt_view_definer");
  if (definer != NULL && definer->has_span) {
    create_view->definer_start = mylite_ast_node_start(definer);
    create_view->definer_end = mylite_ast_node_end(definer);
  }

  const MyliteAstNode *select =
      mylite_ast_find_first_symbol(create_view->node, "nt_create_view_select_opt");
  if (select != NULL && select->has_span) {
    create_view->select_node = select;
    create_view->select_start = mylite_ast_node_start(select);
    create_view->select_end = mylite_ast_node_end(select);
  }

  if (!mylite_ast_set_create_view_columns(ast, create_view,
                                          create_view->node)) {
    return 0;
  }
  statement->create_view = create_view;
  return 1;
}

static int mylite_ast_set_create_view_columns(MyliteAst *ast,
                                              MyliteAstCreateView *create_view,
                                              const MyliteAstNode *payload) {
  if (ast == NULL || create_view == NULL || payload == NULL) {
    return 1;
  }

  const MyliteAstNode *field_list =
      mylite_ast_find_first_symbol(payload, "nt_view_field_list");
  size_t count = mylite_ast_count_view_columns(field_list);
  if (count == 0) {
    return 1;
  }

  create_view->columns =
      mylite_ast_alloc(ast, count * sizeof(*create_view->columns));
  if (create_view->columns == NULL) {
    return 0;
  }
  create_view->column_count = count;

  size_t index = 0;
  int ok = 1;
  mylite_ast_fill_view_columns(ast, create_view->columns,
                               create_view->column_count, field_list, &index,
                               &ok);
  return ok && index == count;
}

static size_t mylite_ast_count_view_columns(const MyliteAstNode *node) {
  if (node == NULL) {
    return 0;
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, "nt_identifier") == 0) {
    return 1;
  }

  size_t count = 0;
  for (size_t i = 0; i < node->child_count; i++) {
    count += mylite_ast_count_view_columns(node->children[i]);
  }
  return count;
}

static void mylite_ast_fill_view_columns(MyliteAst *ast,
                                         MyliteAstViewColumn *columns,
                                         size_t column_count,
                                         const MyliteAstNode *node,
                                         size_t *index, int *ok) {
  if (columns == NULL || node == NULL || index == NULL || ok == NULL || !*ok ||
      *index >= column_count) {
    return;
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, "nt_identifier") == 0) {
    *ok = mylite_ast_fill_view_column(ast, &columns[*index], node);
    if (*ok) {
      (*index)++;
    }
    return;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    mylite_ast_fill_view_columns(ast, columns, column_count, node->children[i],
                                 index, ok);
    if (!*ok) {
      return;
    }
  }
}

static int mylite_ast_fill_view_column(MyliteAst *ast,
                                       MyliteAstViewColumn *column,
                                       const MyliteAstNode *node) {
  if (ast == NULL || column == NULL || node == NULL || !node->has_span) {
    return 1;
  }
  column->start = mylite_ast_node_start(node);
  column->end = mylite_ast_node_end(node);
  return mylite_ast_decode_identifier(ast, column->start, column->end,
                                      &column->name_value,
                                      &column->name_value_length);
}

static MyliteCreateViewAlgorithm mylite_ast_classify_create_view_algorithm(
    const MyliteAstNode *node) {
  if (node == NULL || !node->has_span) {
    return MYLITE_CREATE_VIEW_ALGORITHM_UNSPECIFIED;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_UNDEFINED) != NULL) {
    return MYLITE_CREATE_VIEW_ALGORITHM_UNDEFINED;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_MERGE) != NULL) {
    return MYLITE_CREATE_VIEW_ALGORITHM_MERGE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_TEMPTABLE) != NULL) {
    return MYLITE_CREATE_VIEW_ALGORITHM_TEMPTABLE;
  }
  return MYLITE_CREATE_VIEW_ALGORITHM_UNSPECIFIED;
}

static MyliteViewSqlSecurity mylite_ast_classify_view_sql_security(
    const MyliteAstNode *node) {
  if (node == NULL || !node->has_span) {
    return MYLITE_VIEW_SQL_SECURITY_UNSPECIFIED;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_DEFINER) != NULL) {
    return MYLITE_VIEW_SQL_SECURITY_DEFINER;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_INVOKER) != NULL) {
    return MYLITE_VIEW_SQL_SECURITY_INVOKER;
  }
  return MYLITE_VIEW_SQL_SECURITY_UNSPECIFIED;
}

static MyliteViewCheckOption mylite_ast_classify_view_check_option(
    const MyliteAstNode *node) {
  if (node == NULL || !node->has_span) {
    return MYLITE_VIEW_CHECK_OPTION_NONE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_LOCAL) != NULL) {
    return MYLITE_VIEW_CHECK_OPTION_LOCAL;
  }
  return MYLITE_VIEW_CHECK_OPTION_CASCADED;
}

static int mylite_ast_set_drop_index_view(MyliteAst *ast,
                                          MyliteAstStatement *statement,
                                          const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL) {
    return 1;
  }

  MyliteAstDropIndex *drop_index =
      mylite_ast_alloc(ast, sizeof(*drop_index));
  if (drop_index == NULL) {
    return 0;
  }
  drop_index->node = payload == NULL ? statement->node : payload;
  drop_index->start = mylite_ast_node_start(drop_index->node);
  drop_index->end = mylite_ast_node_end(drop_index->node);
  if (statement->target_count > 0) {
    drop_index->target = &statement->targets[0];
  }

  const MyliteAstNode *name =
      mylite_ast_find_first_symbol(drop_index->node, "nt_identifier");
  if (name != NULL && name->has_span) {
    drop_index->name_start = mylite_ast_node_start(name);
    drop_index->name_end = mylite_ast_node_end(name);
    if (!mylite_ast_decode_identifier(ast, drop_index->name_start,
                                      drop_index->name_end,
                                      &drop_index->name_value,
                                      &drop_index->name_value_length)) {
      return 0;
    }
  }

  const MyliteAstNode *if_exists =
      mylite_ast_find_first_symbol(drop_index->node, "nt_if_exists");
  drop_index->has_if_exists = if_exists != NULL && if_exists->has_span;
  drop_index->is_hypothetical =
      mylite_ast_find_first_token(drop_index->node, MYLITE_TOK_HYPO) != NULL;
  statement->drop_index = drop_index;
  return 1;
}

static int mylite_ast_set_drop_database_view(MyliteAst *ast,
                                             MyliteAstStatement *statement,
                                             const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL) {
    return 1;
  }
  MyliteAstDropDatabase *drop_database =
      mylite_ast_alloc(ast, sizeof(*drop_database));
  if (drop_database == NULL) {
    return 0;
  }
  drop_database->node = payload == NULL ? statement->node : payload;
  drop_database->start = mylite_ast_node_start(drop_database->node);
  drop_database->end = mylite_ast_node_end(drop_database->node);
  if (statement->target_count > 0) {
    drop_database->target = &statement->targets[0];
  }
  const MyliteAstNode *if_exists =
      mylite_ast_find_first_symbol(drop_database->node, "nt_if_exists");
  drop_database->has_if_exists = if_exists != NULL && if_exists->has_span;
  const MyliteAstNode *database_sym =
      mylite_ast_find_first_symbol(drop_database->node, "nt_database_sym");
  drop_database->uses_schema_keyword =
      mylite_ast_find_first_token(database_sym, MYLITE_TOK_SCHEMA) !=
      NULL;
  statement->drop_database = drop_database;
  return 1;
}

static int mylite_ast_set_drop_table_view(MyliteAst *ast,
                                          MyliteAstStatement *statement,
                                          const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL) {
    return 1;
  }
  MyliteAstDropTable *drop_table = mylite_ast_alloc(ast, sizeof(*drop_table));
  if (drop_table == NULL) {
    return 0;
  }
  drop_table->node = payload == NULL ? statement->node : payload;
  drop_table->start = mylite_ast_node_start(drop_table->node);
  drop_table->end = mylite_ast_node_end(drop_table->node);
  drop_table->targets = statement->targets;
  drop_table->target_count = statement->target_count;
  drop_table->is_temporary =
      mylite_ast_find_first_token(payload, MYLITE_TOK_TEMPORARY) != NULL;
  const MyliteAstNode *if_exists =
      mylite_ast_find_first_symbol(payload, "nt_if_exists");
  drop_table->has_if_exists = if_exists != NULL && if_exists->has_span;
  statement->drop_table = drop_table;
  return 1;
}

static int mylite_ast_set_drop_view_view(MyliteAst *ast,
                                         MyliteAstStatement *statement,
                                         const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL) {
    return 1;
  }
  MyliteAstDropView *drop_view = mylite_ast_alloc(ast, sizeof(*drop_view));
  if (drop_view == NULL) {
    return 0;
  }
  drop_view->node = payload == NULL ? statement->node : payload;
  drop_view->start = mylite_ast_node_start(drop_view->node);
  drop_view->end = mylite_ast_node_end(drop_view->node);
  drop_view->targets = statement->targets;
  drop_view->target_count = statement->target_count;
  drop_view->has_if_exists =
      mylite_ast_find_first_token(drop_view->node, MYLITE_TOK_IF_KWD) != NULL &&
      mylite_ast_find_first_token(drop_view->node, MYLITE_TOK_EXISTS) != NULL;
  drop_view->mode = mylite_ast_classify_drop_view_mode(drop_view->node);
  statement->drop_view = drop_view;
  return 1;
}

static MyliteDropViewMode mylite_ast_classify_drop_view_mode(
    const MyliteAstNode *node) {
  if (mylite_ast_find_first_token(node, MYLITE_TOK_RESTRICT) != NULL) {
    return MYLITE_DROP_VIEW_MODE_RESTRICT;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_CASCADE) != NULL) {
    return MYLITE_DROP_VIEW_MODE_CASCADE;
  }
  return MYLITE_DROP_VIEW_MODE_UNSPECIFIED;
}

static int mylite_ast_set_prepare_statement_view(MyliteAst *ast,
                                                 MyliteAstStatement *statement,
                                                 const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL) {
    return 1;
  }
  MyliteAstPrepareStatement *prepare_statement =
      mylite_ast_alloc(ast, sizeof(*prepare_statement));
  if (prepare_statement == NULL) {
    return 0;
  }
  prepare_statement->node = payload == NULL ? statement->node : payload;
  prepare_statement->start = mylite_ast_node_start(prepare_statement->node);
  prepare_statement->end = mylite_ast_node_end(prepare_statement->node);
  if (!mylite_ast_set_prepared_statement_name_value(
          ast, prepare_statement->node, &prepare_statement->name_start,
          &prepare_statement->name_end, &prepare_statement->name_value,
          &prepare_statement->name_value_length)) {
    return 0;
  }
  if (!mylite_ast_set_prepare_statement_source(ast, prepare_statement,
                                               prepare_statement->node)) {
    return 0;
  }
  statement->prepare_statement = prepare_statement;
  return 1;
}

static int mylite_ast_set_prepare_statement_source(
    MyliteAst *ast, MyliteAstPrepareStatement *prepare_statement,
    const MyliteAstNode *node) {
  if (ast == NULL || prepare_statement == NULL || node == NULL) {
    return 1;
  }
  const MyliteAstNode *source = mylite_ast_find_first_symbol(node,
                                                             "nt_prepare_sql");
  if (source == NULL) {
    return 1;
  }
  if (source->child_count == 1 && source->children[0] != NULL) {
    source = source->children[0];
  }

  const MyliteAstNode *string_token =
      source->kind == MYLITE_AST_NODE_TOKEN &&
              source->token == MYLITE_TOK_STRING_LIT
          ? source
          : mylite_ast_find_first_token(source, MYLITE_TOK_STRING_LIT);
  if (string_token != NULL) {
    prepare_statement->source_node = string_token;
    prepare_statement->source_kind = MYLITE_PREPARE_STATEMENT_SOURCE_STRING;
    prepare_statement->source_start = mylite_ast_node_start(string_token);
    prepare_statement->source_end = mylite_ast_node_end(string_token);
    return mylite_ast_decode_sql_string_literal(
        ast, prepare_statement->source_start, prepare_statement->source_end,
        &prepare_statement->source_value,
        &prepare_statement->source_value_length);
  }

  const MyliteAstNode *user_variable_token =
      source->kind == MYLITE_AST_NODE_TOKEN &&
              source->token == MYLITE_TOK_SINGLE_AT_IDENTIFIER
          ? source
          : mylite_ast_find_first_token(source, MYLITE_TOK_SINGLE_AT_IDENTIFIER);
  if (user_variable_token != NULL) {
    prepare_statement->source_node = source;
    prepare_statement->source_kind =
        MYLITE_PREPARE_STATEMENT_SOURCE_USER_VARIABLE;
    prepare_statement->source_start = mylite_ast_node_start(source);
    prepare_statement->source_end = mylite_ast_node_end(source);
    size_t value_start = 0;
    size_t value_end = 0;
    return mylite_ast_set_user_variable_name_value(
        ast, user_variable_token, &value_start, &value_end,
        &prepare_statement->source_value,
        &prepare_statement->source_value_length);
  }

  prepare_statement->source_node = source;
  prepare_statement->source_start = mylite_ast_node_start(source);
  prepare_statement->source_end = mylite_ast_node_end(source);
  return 1;
}

static int mylite_ast_set_execute_statement_view(MyliteAst *ast,
                                                 MyliteAstStatement *statement,
                                                 const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL) {
    return 1;
  }
  MyliteAstExecuteStatement *execute_statement =
      mylite_ast_alloc(ast, sizeof(*execute_statement));
  if (execute_statement == NULL) {
    return 0;
  }
  execute_statement->node = payload == NULL ? statement->node : payload;
  execute_statement->start = mylite_ast_node_start(execute_statement->node);
  execute_statement->end = mylite_ast_node_end(execute_statement->node);
  if (!mylite_ast_set_prepared_statement_name_value(
          ast, execute_statement->node, &execute_statement->name_start,
          &execute_statement->name_end, &execute_statement->name_value,
          &execute_statement->name_value_length)) {
    return 0;
  }
  if (!mylite_ast_set_execute_statement_variables(ast, execute_statement,
                                                  execute_statement->node)) {
    return 0;
  }
  statement->execute_statement = execute_statement;
  return 1;
}

static int mylite_ast_set_execute_statement_variables(
    MyliteAst *ast, MyliteAstExecuteStatement *execute_statement,
    const MyliteAstNode *node) {
  if (ast == NULL || execute_statement == NULL || node == NULL) {
    return 1;
  }
  const MyliteAstNode *variables = mylite_ast_find_first_symbol(
      node, "nt_user_variable_list");
  if (variables == NULL) {
    return 1;
  }
  execute_statement->using_count = mylite_ast_count_user_variables(variables);
  if (execute_statement->using_count == 0) {
    return 1;
  }
  execute_statement->using_variables =
      mylite_ast_alloc(ast, execute_statement->using_count *
                                sizeof(*execute_statement->using_variables));
  if (execute_statement->using_variables == NULL) {
    return 0;
  }

  size_t index = 0;
  int ok = 1;
  mylite_ast_fill_user_variables(ast, execute_statement->using_variables,
                                 execute_statement->using_count, variables,
                                 &index, &ok);
  return ok && index == execute_statement->using_count;
}

static size_t mylite_ast_count_user_variables(const MyliteAstNode *node) {
  if (node == NULL) {
    return 0;
  }
  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_user_variable") == 0) {
    return 1;
  }
  size_t count = 0;
  for (size_t i = 0; i < node->child_count; i++) {
    count += mylite_ast_count_user_variables(node->children[i]);
  }
  return count;
}

static void mylite_ast_fill_user_variables(
    MyliteAst *ast, MyliteAstPreparedStatementVariable *variables,
    size_t variable_count, const MyliteAstNode *node, size_t *index, int *ok) {
  if (ok == NULL || !*ok || node == NULL) {
    return;
  }
  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_user_variable") == 0) {
    if (index == NULL || *index >= variable_count ||
        !mylite_ast_fill_user_variable(ast, &variables[*index], node)) {
      *ok = 0;
      return;
    }
    (*index)++;
    return;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    mylite_ast_fill_user_variables(ast, variables, variable_count,
                                   node->children[i], index, ok);
  }
}

static int mylite_ast_fill_user_variable(
    MyliteAst *ast, MyliteAstPreparedStatementVariable *variable,
    const MyliteAstNode *node) {
  if (variable == NULL || node == NULL) {
    return 1;
  }
  variable->node = node;
  variable->start = mylite_ast_node_start(node);
  variable->end = mylite_ast_node_end(node);
  return mylite_ast_set_user_variable_name_value(
      ast, node, &variable->name_start, &variable->name_end,
      &variable->name_value, &variable->name_value_length);
}

static int mylite_ast_set_deallocate_statement_view(
    MyliteAst *ast, MyliteAstStatement *statement,
    const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL) {
    return 1;
  }
  MyliteAstDeallocateStatement *deallocate_statement =
      mylite_ast_alloc(ast, sizeof(*deallocate_statement));
  if (deallocate_statement == NULL) {
    return 0;
  }
  deallocate_statement->node = payload == NULL ? statement->node : payload;
  deallocate_statement->start = mylite_ast_node_start(deallocate_statement->node);
  deallocate_statement->end = mylite_ast_node_end(deallocate_statement->node);
  deallocate_statement->mode =
      mylite_ast_classify_deallocate_statement_mode(deallocate_statement->node);
  if (!mylite_ast_set_prepared_statement_name_value(
          ast, deallocate_statement->node, &deallocate_statement->name_start,
          &deallocate_statement->name_end, &deallocate_statement->name_value,
          &deallocate_statement->name_value_length)) {
    return 0;
  }
  statement->deallocate_statement = deallocate_statement;
  return 1;
}

static MyliteDeallocateStatementMode mylite_ast_classify_deallocate_statement_mode(
    const MyliteAstNode *node) {
  const MyliteAstNode *symbol = mylite_ast_find_first_symbol(node,
                                                             "nt_deallocate_sym");
  if (mylite_ast_find_first_token(symbol, MYLITE_TOK_DEALLOCATE) != NULL) {
    return MYLITE_DEALLOCATE_STATEMENT_MODE_DEALLOCATE;
  }
  if (mylite_ast_find_first_token(symbol, MYLITE_TOK_DROP) != NULL) {
    return MYLITE_DEALLOCATE_STATEMENT_MODE_DROP;
  }
  return MYLITE_DEALLOCATE_STATEMENT_MODE_UNKNOWN;
}

static int mylite_ast_set_prepared_statement_name_value(
    MyliteAst *ast, const MyliteAstNode *node, size_t *name_start,
    size_t *name_end, const char **name_value, size_t *name_value_length) {
  if (ast == NULL || node == NULL || name_start == NULL || name_end == NULL ||
      name_value == NULL || name_value_length == NULL) {
    return 1;
  }
  const MyliteAstNode *name = mylite_ast_find_first_symbol(node,
                                                           "nt_identifier");
  if (name == NULL) {
    return 1;
  }
  *name_start = mylite_ast_node_start(name);
  *name_end = mylite_ast_node_end(name);
  return mylite_ast_decode_identifier(ast, *name_start, *name_end, name_value,
                                      name_value_length);
}

static int mylite_ast_set_user_variable_name_value(
    MyliteAst *ast, const MyliteAstNode *node, size_t *name_start,
    size_t *name_end, const char **name_value, size_t *name_value_length) {
  if (ast == NULL || node == NULL || name_start == NULL || name_end == NULL ||
      name_value == NULL || name_value_length == NULL) {
    return 1;
  }
  const MyliteAstNode *token =
      node->kind == MYLITE_AST_NODE_TOKEN &&
              node->token == MYLITE_TOK_SINGLE_AT_IDENTIFIER
          ? node
          : mylite_ast_find_first_token(node, MYLITE_TOK_SINGLE_AT_IDENTIFIER);
  if (token == NULL || token->start >= token->end ||
      token->end > ast->source_length) {
    return 1;
  }

  size_t start = token->start;
  if (ast->source[start] == '@') {
    start++;
  }
  *name_start = start;
  *name_end = token->end;
  if (start < token->end &&
      (ast->source[start] == '`' || ast->source[start] == '"')) {
    return mylite_ast_decode_identifier(ast, start, token->end, name_value,
                                        name_value_length);
  }
  return mylite_ast_copy_source_span(ast, start, token->end, name_value,
                                     name_value_length);
}

static int mylite_ast_set_transaction_statement_view(
    MyliteAst *ast, MyliteAstStatement *statement,
    const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL) {
    return 1;
  }
  MyliteAstTransactionStatement *transaction_statement =
      mylite_ast_alloc(ast, sizeof(*transaction_statement));
  if (transaction_statement == NULL) {
    return 0;
  }
  transaction_statement->node = payload == NULL ? statement->node : payload;
  transaction_statement->start =
      mylite_ast_node_start(transaction_statement->node);
  transaction_statement->end = mylite_ast_node_end(transaction_statement->node);
  transaction_statement->kind =
      mylite_ast_classify_transaction_statement(statement->symbol_name);
  transaction_statement->has_work_keyword =
      mylite_ast_find_first_token(transaction_statement->node,
                                  MYLITE_TOK_WORK) != NULL;
  transaction_statement->has_savepoint_keyword =
      mylite_ast_find_first_token(transaction_statement->node,
                                  MYLITE_TOK_SAVEPOINT) != NULL;

  if (transaction_statement->kind == MYLITE_TRANSACTION_STATEMENT_BEGIN) {
    mylite_ast_set_transaction_begin_details(transaction_statement,
                                             transaction_statement->node);
  }
  if (transaction_statement->kind == MYLITE_TRANSACTION_STATEMENT_COMMIT ||
      transaction_statement->kind == MYLITE_TRANSACTION_STATEMENT_ROLLBACK) {
    mylite_ast_set_transaction_completion_details(transaction_statement,
                                                  transaction_statement->node);
  }
  if ((transaction_statement->kind == MYLITE_TRANSACTION_STATEMENT_ROLLBACK ||
       transaction_statement->kind == MYLITE_TRANSACTION_STATEMENT_SAVEPOINT ||
       transaction_statement->kind ==
           MYLITE_TRANSACTION_STATEMENT_RELEASE_SAVEPOINT) &&
      !mylite_ast_set_transaction_savepoint_name(
          ast, transaction_statement, transaction_statement->node)) {
    return 0;
  }

  statement->transaction_statement = transaction_statement;
  return 1;
}

static MyliteTransactionStatementKind mylite_ast_classify_transaction_statement(
    const char *symbol_name) {
  if (symbol_name == NULL) {
    return MYLITE_TRANSACTION_STATEMENT_UNKNOWN;
  }
  if (strcmp(symbol_name, "nt_begin_transaction_stmt") == 0) {
    return MYLITE_TRANSACTION_STATEMENT_BEGIN;
  }
  if (strcmp(symbol_name, "nt_commit_stmt") == 0) {
    return MYLITE_TRANSACTION_STATEMENT_COMMIT;
  }
  if (strcmp(symbol_name, "nt_rollback_stmt") == 0) {
    return MYLITE_TRANSACTION_STATEMENT_ROLLBACK;
  }
  if (strcmp(symbol_name, "nt_savepoint_stmt") == 0) {
    return MYLITE_TRANSACTION_STATEMENT_SAVEPOINT;
  }
  if (strcmp(symbol_name, "nt_release_savepoint_stmt") == 0) {
    return MYLITE_TRANSACTION_STATEMENT_RELEASE_SAVEPOINT;
  }
  return MYLITE_TRANSACTION_STATEMENT_UNKNOWN;
}

static void mylite_ast_set_transaction_begin_details(
    MyliteAstTransactionStatement *transaction_statement,
    const MyliteAstNode *node) {
  if (transaction_statement == NULL || node == NULL) {
    return;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_START) != NULL) {
    transaction_statement->begin_form =
        MYLITE_TRANSACTION_BEGIN_FORM_START_TRANSACTION;
  } else if (mylite_ast_find_first_token(node, MYLITE_TOK_BEGIN) != NULL) {
    transaction_statement->begin_form = MYLITE_TRANSACTION_BEGIN_FORM_BEGIN;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_PESSIMISTIC) != NULL) {
    transaction_statement->begin_mode =
        MYLITE_TRANSACTION_BEGIN_MODE_PESSIMISTIC;
  } else if (mylite_ast_find_first_token(node, MYLITE_TOK_OPTIMISTIC) != NULL) {
    transaction_statement->begin_mode =
        MYLITE_TRANSACTION_BEGIN_MODE_OPTIMISTIC;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_READ) != NULL &&
      mylite_ast_find_first_token(node, MYLITE_TOK_WRITE) != NULL) {
    transaction_statement->access_mode =
        MYLITE_TRANSACTION_ACCESS_READ_WRITE;
  } else if (mylite_ast_find_first_token(node, MYLITE_TOK_READ) != NULL &&
             mylite_ast_find_first_token(node, MYLITE_TOK_ONLY) != NULL) {
    transaction_statement->access_mode =
        MYLITE_TRANSACTION_ACCESS_READ_ONLY;
  }
  transaction_statement->has_consistent_snapshot =
      mylite_ast_find_first_token(node, MYLITE_TOK_CONSISTENT) != NULL &&
      mylite_ast_find_first_token(node, MYLITE_TOK_SNAPSHOT) != NULL;
  transaction_statement->has_causal_consistency =
      mylite_ast_find_first_token(node, MYLITE_TOK_CAUSAL) != NULL &&
      mylite_ast_find_first_token(node, MYLITE_TOK_CONSISTENCY) != NULL;
}

static void mylite_ast_set_transaction_completion_details(
    MyliteAstTransactionStatement *transaction_statement,
    const MyliteAstNode *node) {
  if (transaction_statement == NULL || node == NULL) {
    return;
  }
  const MyliteAstNode *completion = mylite_ast_find_first_symbol(
      node, "nt_completion_type_within_transaction");
  if (completion == NULL) {
    return;
  }
  for (size_t i = 0; i < completion->child_count; i++) {
    const MyliteAstNode *child = completion->children[i];
    if (child == NULL || child->kind != MYLITE_AST_NODE_TOKEN) {
      continue;
    }
    int previous_is_no = i > 0 && completion->children[i - 1] != NULL &&
                         completion->children[i - 1]->kind ==
                             MYLITE_AST_NODE_TOKEN &&
                         completion->children[i - 1]->token == MYLITE_TOK_NO;
    if (child->token == MYLITE_TOK_CHAIN) {
      if (previous_is_no) {
        transaction_statement->has_no_chain = 1;
      } else {
        transaction_statement->has_chain = 1;
      }
    } else if (child->token == MYLITE_TOK_RELEASE) {
      if (previous_is_no) {
        transaction_statement->has_no_release = 1;
      } else {
        transaction_statement->has_release = 1;
      }
    }
  }
}

static int mylite_ast_set_transaction_savepoint_name(
    MyliteAst *ast, MyliteAstTransactionStatement *transaction_statement,
    const MyliteAstNode *node) {
  if (ast == NULL || transaction_statement == NULL || node == NULL) {
    return 1;
  }
  const MyliteAstNode *name = mylite_ast_find_first_symbol(node,
                                                           "nt_identifier");
  if (name == NULL) {
    return 1;
  }
  transaction_statement->savepoint_name_start = mylite_ast_node_start(name);
  transaction_statement->savepoint_name_end = mylite_ast_node_end(name);
  return mylite_ast_decode_identifier(
      ast, transaction_statement->savepoint_name_start,
      transaction_statement->savepoint_name_end,
      &transaction_statement->savepoint_name_value,
      &transaction_statement->savepoint_name_value_length);
}

static int mylite_ast_set_set_statement_view(MyliteAst *ast,
                                             MyliteAstStatement *statement,
                                             const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL) {
    return 1;
  }
  MyliteAstSetStatement *set_statement =
      mylite_ast_alloc(ast, sizeof(*set_statement));
  if (set_statement == NULL) {
    return 0;
  }
  set_statement->node = payload == NULL ? statement->node : payload;
  set_statement->start = mylite_ast_node_start(set_statement->node);
  set_statement->end = mylite_ast_node_end(set_statement->node);
  set_statement->form =
      mylite_ast_classify_set_statement_form(statement->symbol_name,
                                             set_statement->node);
  if (!mylite_ast_set_set_assignments(ast, set_statement, set_statement->node)) {
    return 0;
  }
  statement->set_statement = set_statement;
  return 1;
}

static int mylite_ast_set_set_assignments(MyliteAst *ast,
                                          MyliteAstSetStatement *set_statement,
                                          const MyliteAstNode *payload) {
  if (ast == NULL || set_statement == NULL || payload == NULL) {
    return 1;
  }
  set_statement->assignment_count = mylite_ast_count_set_assignments(payload);
  if (set_statement->assignment_count == 0) {
    return 1;
  }
  set_statement->assignments =
      mylite_ast_alloc(ast, set_statement->assignment_count *
                                sizeof(*set_statement->assignments));
  if (set_statement->assignments == NULL) {
    return 0;
  }

  size_t index = 0;
  int ok = 1;
  mylite_ast_fill_set_assignments(ast, set_statement, payload, &index, &ok);
  return ok && index == set_statement->assignment_count;
}

static int mylite_ast_set_expression_summary(MyliteAst *ast,
                                             MyliteAstExpression *expression,
                                             const MyliteAstNode *node) {
  if (ast == NULL || expression == NULL || node == NULL) {
    return 1;
  }
  const MyliteAstNode *payload = mylite_ast_expression_payload(node);
  expression->node = payload == NULL ? node : payload;
  expression->start = mylite_ast_node_start(expression->node);
  expression->end = mylite_ast_node_end(expression->node);
  expression->kind = mylite_ast_classify_expression(expression->node);
  expression->literal_kind =
      expression->kind == MYLITE_EXPRESSION_LITERAL
          ? mylite_ast_expression_literal_kind(expression->node)
          : MYLITE_EXPRESSION_LITERAL_NONE;
  const MyliteAstNode *value_node =
      mylite_ast_expression_value_node(expression->node, expression->kind,
                                       expression->literal_kind);
  return mylite_ast_set_expression_value(ast, expression, value_node);
}

static const MyliteAstNode *mylite_ast_expression_payload(
    const MyliteAstNode *node) {
  const MyliteAstNode *current = node;
  while (current != NULL && current->kind == MYLITE_AST_NODE_RULE &&
         current->child_count == 1 && current->symbol_name != NULL) {
    static const char *const wrappers[] = {
        "nt_set_expr",
        "nt_expr_or_default",
        "nt_expression",
        "nt_bool_pri",
        "nt_predicate_expr",
        "nt_bit_expr",
        "nt_simple_expr",
        "nt_literal",
        "nt_string_literal",
        "nt_charset_name_or_default",
        "nt_charset_name",
        "nt_string_name",
    };
    if (!symbol_is_one_of(current->symbol_name, wrappers,
                          sizeof(wrappers) / sizeof(wrappers[0]))) {
      break;
    }
    current = current->children[0];
  }
  return current;
}

static MyliteExpressionKind mylite_ast_classify_expression(
    const MyliteAstNode *node) {
  if (node == NULL) {
    return MYLITE_EXPRESSION_UNKNOWN;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_DEFAULT_KWD) != NULL) {
    return MYLITE_EXPRESSION_DEFAULT;
  }
  if (mylite_ast_find_first_symbol(node, "nt_function_call_generic") != NULL ||
      mylite_ast_find_first_symbol(node, "nt_function_call_keyword") != NULL ||
      mylite_ast_find_first_symbol(node, "nt_function_call_nonkeyword") !=
          NULL) {
    return MYLITE_EXPRESSION_FUNCTION_CALL;
  }
  if (mylite_ast_expression_literal_kind(node) !=
      MYLITE_EXPRESSION_LITERAL_NONE) {
    return MYLITE_EXPRESSION_LITERAL;
  }
  if (mylite_ast_find_first_symbol(node, "nt_variable") != NULL ||
      mylite_ast_find_first_symbol(node, "nt_system_variable") != NULL ||
      mylite_ast_find_first_symbol(node, "nt_user_variable") != NULL ||
      mylite_ast_find_first_token(node, MYLITE_TOK_DOUBLE_AT_IDENTIFIER) !=
          NULL ||
      mylite_ast_find_first_token(node, MYLITE_TOK_SINGLE_AT_IDENTIFIER) !=
          NULL) {
    return MYLITE_EXPRESSION_VARIABLE;
  }
  if (node->symbol_name != NULL &&
      (strcmp(node->symbol_name, "nt_identifier") == 0 ||
       strcmp(node->symbol_name, "nt_column_name") == 0)) {
    return MYLITE_EXPRESSION_IDENTIFIER;
  }
  if (node->kind == MYLITE_AST_NODE_TOKEN && node->token == MYLITE_TOK_IDENTIFIER) {
    return MYLITE_EXPRESSION_IDENTIFIER;
  }
  return MYLITE_EXPRESSION_RAW;
}

static MyliteExpressionLiteralKind mylite_ast_expression_literal_kind(
    const MyliteAstNode *node) {
  const MyliteAstNode *literal = mylite_ast_expression_payload(node);
  if (literal == NULL) {
    return MYLITE_EXPRESSION_LITERAL_NONE;
  }
  if (literal->kind == MYLITE_AST_NODE_TOKEN) {
    switch (literal->token) {
      case MYLITE_TOK_STRING_LIT:
        return MYLITE_EXPRESSION_LITERAL_STRING;
      case MYLITE_TOK_INT_LIT:
      case MYLITE_TOK_NUMBER:
        return MYLITE_EXPRESSION_LITERAL_UNSIGNED_INTEGER;
      case MYLITE_TOK_FLOAT_LIT:
        return MYLITE_EXPRESSION_LITERAL_FLOAT;
      case MYLITE_TOK_HEX_LIT:
        return MYLITE_EXPRESSION_LITERAL_HEX;
      case MYLITE_TOK_BIT_LIT:
        return MYLITE_EXPRESSION_LITERAL_BIT;
      case MYLITE_TOK_NULL:
        return MYLITE_EXPRESSION_LITERAL_NULL;
      case MYLITE_TOK_TRUE_KWD:
        return MYLITE_EXPRESSION_LITERAL_TRUE;
      case MYLITE_TOK_FALSE_KWD:
        return MYLITE_EXPRESSION_LITERAL_FALSE;
      default:
        break;
    }
  }
  const MyliteAstNode *token = mylite_ast_find_first_token(literal,
                                                           MYLITE_TOK_STRING_LIT);
  if (token != NULL) {
    return MYLITE_EXPRESSION_LITERAL_STRING;
  }
  token = mylite_ast_find_first_token(literal, MYLITE_TOK_INT_LIT);
  if (token != NULL) {
    return MYLITE_EXPRESSION_LITERAL_UNSIGNED_INTEGER;
  }
  token = mylite_ast_find_first_token(literal, MYLITE_TOK_NUMBER);
  if (token != NULL) {
    return MYLITE_EXPRESSION_LITERAL_UNSIGNED_INTEGER;
  }
  token = mylite_ast_find_first_token(literal, MYLITE_TOK_FLOAT_LIT);
  if (token != NULL) {
    return MYLITE_EXPRESSION_LITERAL_FLOAT;
  }
  token = mylite_ast_find_first_token(literal, MYLITE_TOK_HEX_LIT);
  if (token != NULL) {
    return MYLITE_EXPRESSION_LITERAL_HEX;
  }
  token = mylite_ast_find_first_token(literal, MYLITE_TOK_BIT_LIT);
  if (token != NULL) {
    return MYLITE_EXPRESSION_LITERAL_BIT;
  }
  token = mylite_ast_find_first_token(literal, MYLITE_TOK_NULL);
  if (token != NULL) {
    return MYLITE_EXPRESSION_LITERAL_NULL;
  }
  token = mylite_ast_find_first_token(literal, MYLITE_TOK_TRUE_KWD);
  if (token != NULL) {
    return MYLITE_EXPRESSION_LITERAL_TRUE;
  }
  token = mylite_ast_find_first_token(literal, MYLITE_TOK_FALSE_KWD);
  if (token != NULL) {
    return MYLITE_EXPRESSION_LITERAL_FALSE;
  }
  return MYLITE_EXPRESSION_LITERAL_NONE;
}

static const MyliteAstNode *mylite_ast_expression_value_node(
    const MyliteAstNode *node, MyliteExpressionKind kind,
    MyliteExpressionLiteralKind literal_kind) {
  if (node == NULL) {
    return NULL;
  }
  if (kind == MYLITE_EXPRESSION_LITERAL) {
    switch (literal_kind) {
      case MYLITE_EXPRESSION_LITERAL_STRING:
        return mylite_ast_find_first_token(node, MYLITE_TOK_STRING_LIT);
      case MYLITE_EXPRESSION_LITERAL_UNSIGNED_INTEGER:
        {
          const MyliteAstNode *value =
              mylite_ast_find_first_token(node, MYLITE_TOK_INT_LIT);
          return value == NULL ? mylite_ast_find_first_token(node,
                                                             MYLITE_TOK_NUMBER)
                               : value;
        }
      case MYLITE_EXPRESSION_LITERAL_FLOAT:
        return mylite_ast_find_first_token(node, MYLITE_TOK_FLOAT_LIT);
      case MYLITE_EXPRESSION_LITERAL_HEX:
        return mylite_ast_find_first_token(node, MYLITE_TOK_HEX_LIT);
      case MYLITE_EXPRESSION_LITERAL_BIT:
        return mylite_ast_find_first_token(node, MYLITE_TOK_BIT_LIT);
      case MYLITE_EXPRESSION_LITERAL_NULL:
        return mylite_ast_find_first_token(node, MYLITE_TOK_NULL);
      case MYLITE_EXPRESSION_LITERAL_TRUE:
        return mylite_ast_find_first_token(node, MYLITE_TOK_TRUE_KWD);
      case MYLITE_EXPRESSION_LITERAL_FALSE:
        return mylite_ast_find_first_token(node, MYLITE_TOK_FALSE_KWD);
      case MYLITE_EXPRESSION_LITERAL_NONE:
        break;
    }
  }
  if (kind == MYLITE_EXPRESSION_DEFAULT) {
    return mylite_ast_find_first_token(node, MYLITE_TOK_DEFAULT_KWD);
  }
  if (kind == MYLITE_EXPRESSION_FUNCTION_CALL ||
      kind == MYLITE_EXPRESSION_IDENTIFIER) {
    return mylite_ast_find_first_token(node, MYLITE_TOK_IDENTIFIER);
  }
  if (kind == MYLITE_EXPRESSION_VARIABLE) {
    const MyliteAstNode *value =
        mylite_ast_find_first_token(node, MYLITE_TOK_DOUBLE_AT_IDENTIFIER);
    return value == NULL ? mylite_ast_find_first_token(
                               node, MYLITE_TOK_SINGLE_AT_IDENTIFIER)
                         : value;
  }
  return NULL;
}

static int mylite_ast_set_expression_value(MyliteAst *ast,
                                           MyliteAstExpression *expression,
                                           const MyliteAstNode *value_node) {
  if (ast == NULL || expression == NULL || value_node == NULL ||
      !value_node->has_span) {
    return 1;
  }
  expression->value_start = mylite_ast_node_start(value_node);
  expression->value_end = mylite_ast_node_end(value_node);
  if (expression->literal_kind == MYLITE_EXPRESSION_LITERAL_STRING) {
    return mylite_ast_decode_sql_string_literal(
        ast, expression->value_start, expression->value_end,
        &expression->value, &expression->value_length);
  }
  if (expression->kind == MYLITE_EXPRESSION_IDENTIFIER ||
      expression->kind == MYLITE_EXPRESSION_FUNCTION_CALL) {
    return mylite_ast_decode_identifier(ast, expression->value_start,
                                        expression->value_end,
                                        &expression->value,
                                        &expression->value_length);
  }
  if (expression->literal_kind ==
      MYLITE_EXPRESSION_LITERAL_UNSIGNED_INTEGER) {
    expression->has_unsigned_integer_value = mylite_ast_parse_unsigned_integer_value(
        ast->source, expression->value_start, expression->value_end,
        &expression->unsigned_integer_value);
  }
  return mylite_ast_copy_source_span(ast, expression->value_start,
                                     expression->value_end,
                                     &expression->value,
                                     &expression->value_length);
}

static size_t mylite_ast_count_set_assignments(const MyliteAstNode *node) {
  if (node == NULL) {
    return 0;
  }
  if (node->symbol_name != NULL) {
    if (strcmp(node->symbol_name, "nt_variable_assignment") == 0 ||
        strcmp(node->symbol_name, "nt_transaction_char") == 0) {
      return 1;
    }
    if (strcmp(node->symbol_name, "nt_set_stmt") == 0 &&
        mylite_ast_classify_set_statement_form("nt_set_stmt", node) ==
            MYLITE_SET_STATEMENT_CONFIG) {
      return 1;
    }
  }

  size_t count = 0;
  for (size_t i = 0; i < node->child_count; i++) {
    count += mylite_ast_count_set_assignments(node->children[i]);
  }
  return count;
}

static void mylite_ast_fill_set_assignments(
    MyliteAst *ast, MyliteAstSetStatement *set_statement,
    const MyliteAstNode *node, size_t *index, int *ok) {
  if (set_statement == NULL || node == NULL || index == NULL || ok == NULL ||
      !*ok || *index >= set_statement->assignment_count) {
    return;
  }
  if (node->symbol_name != NULL) {
    if (strcmp(node->symbol_name, "nt_variable_assignment") == 0 ||
        strcmp(node->symbol_name, "nt_transaction_char") == 0 ||
        (strcmp(node->symbol_name, "nt_set_stmt") == 0 &&
         mylite_ast_classify_set_statement_form("nt_set_stmt", node) ==
             MYLITE_SET_STATEMENT_CONFIG)) {
      *ok = mylite_ast_fill_set_assignment(
          ast, set_statement, &set_statement->assignments[*index], node);
      if (*ok) {
        (*index)++;
      }
      return;
    }
  }

  for (size_t i = 0; i < node->child_count; i++) {
    mylite_ast_fill_set_assignments(ast, set_statement, node->children[i],
                                    index, ok);
    if (!*ok) {
      return;
    }
  }
}

static int mylite_ast_fill_set_assignment(MyliteAst *ast,
                                          MyliteAstSetStatement *set_statement,
                                          MyliteAstSetAssignment *assignment,
                                          const MyliteAstNode *node) {
  if (ast == NULL || set_statement == NULL || assignment == NULL ||
      node == NULL) {
    return 1;
  }
  assignment->node = node;
  assignment->start = mylite_ast_node_start(node);
  assignment->end = mylite_ast_node_end(node);
  assignment->scope = MYLITE_SET_VARIABLE_SCOPE_UNSPECIFIED;
  assignment->operator_kind = mylite_ast_set_assignment_operator(node);

  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_transaction_char") == 0) {
    return mylite_ast_fill_transaction_characteristic_assignment(
        ast, set_statement, assignment, node);
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, "nt_set_stmt") == 0) {
    return mylite_ast_fill_config_assignment(ast, assignment, node);
  }
  return mylite_ast_fill_set_variable_assignment(ast, assignment, node);
}

static int mylite_ast_fill_set_variable_assignment(
    MyliteAst *ast, MyliteAstSetAssignment *assignment,
    const MyliteAstNode *node) {
  assignment->kind = mylite_ast_classify_set_variable_assignment(node);
  assignment->scope = mylite_ast_set_assignment_prefix_scope(node);

  const MyliteAstNode *name = mylite_ast_set_assignment_name_node(node);
  if (!mylite_ast_set_assignment_name_value(ast, assignment, name)) {
    return 0;
  }

  assignment->value_node = mylite_ast_set_assignment_value_node(node);
  if (assignment->value_node != NULL) {
    assignment->value_start = mylite_ast_node_start(assignment->value_node);
    assignment->value_end = mylite_ast_node_end(assignment->value_node);
    if (!mylite_ast_set_expression_summary(ast, &assignment->value_expression,
                                           assignment->value_node)) {
      return 0;
    }
  }

  assignment->extend_value_node = mylite_ast_set_names_extend_value_node(node);
  if (assignment->extend_value_node != NULL) {
    assignment->extend_value_start =
        mylite_ast_node_start(assignment->extend_value_node);
    assignment->extend_value_end =
        mylite_ast_node_end(assignment->extend_value_node);
  }
  return 1;
}

static int mylite_ast_fill_transaction_characteristic_assignment(
    MyliteAst *ast, MyliteAstSetStatement *set_statement,
    MyliteAstSetAssignment *assignment, const MyliteAstNode *node) {
  assignment->kind = MYLITE_SET_ASSIGNMENT_TRANSACTION_CHARACTERISTIC;
  assignment->scope = mylite_ast_set_statement_scope(set_statement->node);
  assignment->operator_kind = MYLITE_SET_ASSIGNMENT_OPERATOR_NONE;
  if (mylite_ast_find_first_token(node, MYLITE_TOK_ISOLATION) != NULL) {
    const MyliteAstNode *isolation =
        mylite_ast_find_first_token(node, MYLITE_TOK_ISOLATION);
    const MyliteAstNode *level = mylite_ast_find_first_token(node,
                                                             MYLITE_TOK_LEVEL);
    assignment->name_start = mylite_ast_node_start(isolation);
    assignment->name_end = level == NULL ? mylite_ast_node_end(isolation)
                                         : mylite_ast_node_end(level);
    if (!mylite_ast_set_assignment_constant_name(assignment, "tx_isolation")) {
      return 0;
    }
    assignment->value_node = mylite_ast_find_first_symbol(node,
                                                          "nt_isolation_level");
  } else if (mylite_ast_find_first_token(node, MYLITE_TOK_READ) != NULL) {
    const MyliteAstNode *read = mylite_ast_find_first_token(node, MYLITE_TOK_READ);
    const MyliteAstNode *mode = mylite_ast_find_first_token(node,
                                                            MYLITE_TOK_ONLY);
    assignment->name_start = mylite_ast_node_start(read);
    assignment->name_end = mode == NULL ? mylite_ast_node_end(read)
                                        : mylite_ast_node_end(mode);
    if (!mylite_ast_set_assignment_constant_name(assignment, "tx_read_only")) {
      return 0;
    }
    assignment->value_node = mode;
    if (assignment->value_node == NULL) {
      assignment->value_node = mylite_ast_find_first_token(node, MYLITE_TOK_WRITE);
      if (assignment->value_node != NULL) {
        assignment->name_end = mylite_ast_node_end(assignment->value_node);
      }
    }
  }
  if (assignment->value_node != NULL) {
    assignment->value_start = mylite_ast_node_start(assignment->value_node);
    assignment->value_end = mylite_ast_node_end(assignment->value_node);
    if (!mylite_ast_set_expression_summary(ast, &assignment->value_expression,
                                           assignment->value_node)) {
      return 0;
    }
  }
  return 1;
}

static int mylite_ast_fill_config_assignment(MyliteAst *ast,
                                             MyliteAstSetAssignment *assignment,
                                             const MyliteAstNode *node) {
  assignment->kind = MYLITE_SET_ASSIGNMENT_CONFIG;
  assignment->operator_kind = mylite_ast_set_assignment_operator(node);
  const MyliteAstNode *name = mylite_ast_find_first_symbol(node,
                                                           "nt_config_item_name");
  if (name != NULL) {
    assignment->name_start = mylite_ast_node_start(name);
    assignment->name_end = mylite_ast_node_end(name);
    if (!mylite_ast_copy_source_span(ast, assignment->name_start,
                                     assignment->name_end,
                                     &assignment->name_value,
                                     &assignment->name_value_length)) {
      return 0;
    }
  } else if (!mylite_ast_set_assignment_constant_name(assignment, "config")) {
    return 0;
  }
  assignment->value_node = mylite_ast_find_first_symbol(node, "nt_set_expr");
  if (assignment->value_node != NULL) {
    assignment->value_start = mylite_ast_node_start(assignment->value_node);
    assignment->value_end = mylite_ast_node_end(assignment->value_node);
    if (!mylite_ast_set_expression_summary(ast, &assignment->value_expression,
                                           assignment->value_node)) {
      return 0;
    }
  }
  return 1;
}

static MyliteSetStatementForm mylite_ast_classify_set_statement_form(
    const char *symbol_name, const MyliteAstNode *node) {
  if (symbol_name != NULL) {
    if (strcmp(symbol_name, "nt_set_role_stmt") == 0) {
      return MYLITE_SET_STATEMENT_ROLE;
    }
    if (strcmp(symbol_name, "nt_set_default_role_stmt") == 0) {
      return MYLITE_SET_STATEMENT_DEFAULT_ROLE;
    }
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_PASSWORD) != NULL) {
    return MYLITE_SET_STATEMENT_PASSWORD;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_CONFIG) != NULL) {
    return MYLITE_SET_STATEMENT_CONFIG;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_SESSION_STATES) != NULL) {
    return MYLITE_SET_STATEMENT_SESSION_STATES;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_RESOURCE) != NULL &&
      mylite_ast_find_first_token(node, MYLITE_TOK_GROUP) != NULL) {
    return MYLITE_SET_STATEMENT_RESOURCE_GROUP;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_TRANSACTION) != NULL) {
    return MYLITE_SET_STATEMENT_TRANSACTION;
  }
  if (symbol_name != NULL && strcmp(symbol_name, "nt_set_stmt") == 0) {
    return MYLITE_SET_STATEMENT_ASSIGNMENTS;
  }
  return MYLITE_SET_STATEMENT_UNKNOWN;
}

static MyliteSetVariableScope mylite_ast_set_statement_scope(
    const MyliteAstNode *node) {
  if (node == NULL) {
    return MYLITE_SET_VARIABLE_SCOPE_UNSPECIFIED;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *child = node->children[i];
    if (child == NULL || child->kind != MYLITE_AST_NODE_TOKEN) {
      continue;
    }
    if (child->token == MYLITE_TOK_GLOBAL) {
      return MYLITE_SET_VARIABLE_SCOPE_GLOBAL;
    }
    if (child->token == MYLITE_TOK_SESSION) {
      return MYLITE_SET_VARIABLE_SCOPE_SESSION;
    }
  }
  return MYLITE_SET_VARIABLE_SCOPE_UNSPECIFIED;
}

static MyliteSetVariableScope mylite_ast_set_assignment_prefix_scope(
    const MyliteAstNode *node) {
  if (node == NULL) {
    return MYLITE_SET_VARIABLE_SCOPE_UNSPECIFIED;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *child = node->children[i];
    if (child == NULL || child->kind != MYLITE_AST_NODE_TOKEN) {
      continue;
    }
    switch (child->token) {
      case MYLITE_TOK_GLOBAL:
        return MYLITE_SET_VARIABLE_SCOPE_GLOBAL;
      case MYLITE_TOK_SESSION:
        return MYLITE_SET_VARIABLE_SCOPE_SESSION;
      case MYLITE_TOK_LOCAL:
        return MYLITE_SET_VARIABLE_SCOPE_LOCAL;
      case MYLITE_TOK_INSTANCE:
        return MYLITE_SET_VARIABLE_SCOPE_INSTANCE;
      default:
        break;
    }
  }
  return MYLITE_SET_VARIABLE_SCOPE_UNSPECIFIED;
}

static MyliteSetAssignmentKind mylite_ast_classify_set_variable_assignment(
    const MyliteAstNode *node) {
  if (node == NULL) {
    return MYLITE_SET_ASSIGNMENT_UNKNOWN;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *child = node->children[i];
    if (child == NULL) {
      continue;
    }
    if (child->kind == MYLITE_AST_NODE_TOKEN) {
      if (child->token == MYLITE_TOK_NAMES) {
        return MYLITE_SET_ASSIGNMENT_NAMES;
      }
      if (child->token == MYLITE_TOK_SINGLE_AT_IDENTIFIER) {
        return MYLITE_SET_ASSIGNMENT_USER_VARIABLE;
      }
      if (child->token == MYLITE_TOK_DOUBLE_AT_IDENTIFIER) {
        return MYLITE_SET_ASSIGNMENT_SYSTEM_VARIABLE;
      }
    }
    if (child->symbol_name != NULL) {
      if (strcmp(child->symbol_name, "nt_charset_kw") == 0) {
        return MYLITE_SET_ASSIGNMENT_CHARACTER_SET;
      }
      if (strcmp(child->symbol_name, "nt_variable_name") == 0) {
        return MYLITE_SET_ASSIGNMENT_SYSTEM_VARIABLE;
      }
    }
  }
  return MYLITE_SET_ASSIGNMENT_UNKNOWN;
}

static MyliteSetAssignmentOperator mylite_ast_set_assignment_operator(
    const MyliteAstNode *node) {
  const MyliteAstNode *operator_node =
      mylite_ast_find_first_symbol(node, "nt_eq_or_assignment_eq");
  if (operator_node == NULL) {
    return MYLITE_SET_ASSIGNMENT_OPERATOR_NONE;
  }
  if (mylite_ast_find_first_token(operator_node, MYLITE_TOK_ASSIGNMENT_EQ) !=
      NULL) {
    return MYLITE_SET_ASSIGNMENT_OPERATOR_ASSIGNMENT_EQ;
  }
  if (mylite_ast_find_first_token(operator_node, MYLITE_TOK_EQ) != NULL) {
    return MYLITE_SET_ASSIGNMENT_OPERATOR_EQ;
  }
  return MYLITE_SET_ASSIGNMENT_OPERATOR_NONE;
}

static const MyliteAstNode *mylite_ast_set_assignment_name_node(
    const MyliteAstNode *node) {
  if (node == NULL) {
    return NULL;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *child = node->children[i];
    if (child == NULL) {
      continue;
    }
    if (child->kind == MYLITE_AST_NODE_TOKEN &&
        (child->token == MYLITE_TOK_DOUBLE_AT_IDENTIFIER ||
         child->token == MYLITE_TOK_SINGLE_AT_IDENTIFIER ||
         child->token == MYLITE_TOK_NAMES)) {
      return child;
    }
    if (child->symbol_name != NULL &&
        (strcmp(child->symbol_name, "nt_variable_name") == 0 ||
         strcmp(child->symbol_name, "nt_charset_kw") == 0)) {
      return child;
    }
  }
  return NULL;
}

static const MyliteAstNode *mylite_ast_set_assignment_value_node(
    const MyliteAstNode *node) {
  if (node == NULL) {
    return NULL;
  }
  const MyliteAstNode *value = mylite_ast_find_first_symbol(node, "nt_set_expr");
  if (value != NULL) {
    return value;
  }
  value = mylite_ast_find_first_symbol(node, "nt_expression");
  if (value != NULL) {
    return value;
  }
  value = mylite_ast_find_first_symbol(node, "nt_charset_name_or_default");
  if (value != NULL) {
    return value;
  }
  value = mylite_ast_find_first_symbol(node, "nt_charset_name");
  if (value != NULL) {
    return value;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *child = node->children[i];
    if (child != NULL && child->kind == MYLITE_AST_NODE_TOKEN &&
        child->token == MYLITE_TOK_DEFAULT_KWD) {
      return child;
    }
  }
  return NULL;
}

static const MyliteAstNode *mylite_ast_set_names_extend_value_node(
    const MyliteAstNode *node) {
  if (mylite_ast_classify_set_variable_assignment(node) !=
      MYLITE_SET_ASSIGNMENT_NAMES) {
    return NULL;
  }
  for (size_t i = 0; i + 1 < node->child_count; i++) {
    const MyliteAstNode *child = node->children[i];
    if (child != NULL && child->kind == MYLITE_AST_NODE_TOKEN &&
        child->token == MYLITE_TOK_COLLATE) {
      return node->children[i + 1];
    }
  }
  return NULL;
}

static int mylite_ast_set_assignment_name_value(
    MyliteAst *ast, MyliteAstSetAssignment *assignment,
    const MyliteAstNode *node) {
  if (assignment == NULL) {
    return 1;
  }
  switch (assignment->kind) {
    case MYLITE_SET_ASSIGNMENT_NAMES:
      if (node != NULL) {
        assignment->name_start = mylite_ast_node_start(node);
        assignment->name_end = mylite_ast_node_end(node);
      }
      return mylite_ast_set_assignment_constant_name(assignment, "names");
    case MYLITE_SET_ASSIGNMENT_CHARACTER_SET:
      if (node != NULL) {
        assignment->name_start = mylite_ast_node_start(node);
        assignment->name_end = mylite_ast_node_end(node);
      }
      return mylite_ast_set_assignment_constant_name(assignment, "character_set");
    case MYLITE_SET_ASSIGNMENT_SYSTEM_VARIABLE:
      if (node != NULL && node->kind == MYLITE_AST_NODE_TOKEN &&
          node->token == MYLITE_TOK_DOUBLE_AT_IDENTIFIER) {
        return mylite_ast_set_at_variable_name_value(ast, assignment, node);
      }
      return mylite_ast_set_variable_name_value(ast, assignment, node);
    case MYLITE_SET_ASSIGNMENT_USER_VARIABLE:
      return mylite_ast_set_at_variable_name_value(ast, assignment, node);
    case MYLITE_SET_ASSIGNMENT_CONFIG:
    case MYLITE_SET_ASSIGNMENT_TRANSACTION_CHARACTERISTIC:
    case MYLITE_SET_ASSIGNMENT_UNKNOWN:
      break;
  }
  if (node == NULL) {
    return 1;
  }
  assignment->name_start = mylite_ast_node_start(node);
  assignment->name_end = mylite_ast_node_end(node);
  return mylite_ast_copy_source_span(ast, assignment->name_start,
                                     assignment->name_end,
                                     &assignment->name_value,
                                     &assignment->name_value_length);
}

static int mylite_ast_set_assignment_constant_name(
    MyliteAstSetAssignment *assignment, const char *name) {
  if (assignment == NULL || name == NULL) {
    return 1;
  }
  assignment->name_value = name;
  assignment->name_value_length = strlen(name);
  return 1;
}

static int mylite_ast_set_variable_name_value(
    MyliteAst *ast, MyliteAstSetAssignment *assignment,
    const MyliteAstNode *node) {
  if (ast == NULL || assignment == NULL || node == NULL) {
    return 1;
  }

  const MyliteAstNode *parts[2] = {NULL, NULL};
  size_t part_count = 0;
  for (size_t i = 0; i < node->child_count && part_count < 2; i++) {
    const MyliteAstNode *child = node->children[i];
    if (child != NULL && child->symbol_name != NULL &&
        strcmp(child->symbol_name, "nt_identifier") == 0) {
      parts[part_count++] = child;
    }
  }
  if (part_count == 0) {
    assignment->name_start = mylite_ast_node_start(node);
    assignment->name_end = mylite_ast_node_end(node);
    return mylite_ast_copy_source_span(ast, assignment->name_start,
                                       assignment->name_end,
                                       &assignment->name_value,
                                       &assignment->name_value_length);
  }

  assignment->name_start = mylite_ast_node_start(parts[0]);
  assignment->name_end = mylite_ast_node_end(parts[part_count - 1]);
  if (part_count == 1) {
    return mylite_ast_decode_identifier(ast, assignment->name_start,
                                        assignment->name_end,
                                        &assignment->name_value,
                                        &assignment->name_value_length);
  }

  const char *left = NULL;
  const char *right = NULL;
  size_t left_length = 0;
  size_t right_length = 0;
  if (!mylite_ast_decode_identifier(ast, mylite_ast_node_start(parts[0]),
                                    mylite_ast_node_end(parts[0]), &left,
                                    &left_length) ||
      !mylite_ast_decode_identifier(ast, mylite_ast_node_start(parts[1]),
                                    mylite_ast_node_end(parts[1]), &right,
                                    &right_length)) {
    return 0;
  }

  char *value = mylite_ast_alloc(ast, left_length + 1 + right_length + 1);
  if (value == NULL) {
    return 0;
  }
  memcpy(value, left, left_length);
  value[left_length] = '.';
  memcpy(value + left_length + 1, right, right_length);
  value[left_length + 1 + right_length] = '\0';
  assignment->name_value = value;
  assignment->name_value_length = left_length + 1 + right_length;
  return 1;
}

static int mylite_ast_set_at_variable_name_value(
    MyliteAst *ast, MyliteAstSetAssignment *assignment,
    const MyliteAstNode *node) {
  if (ast == NULL || assignment == NULL || node == NULL ||
      node->start >= node->end || node->end > ast->source_length) {
    return 1;
  }

  size_t start = node->start;
  const char *raw = ast->source + node->start;
  size_t length = node->end - node->start;
  if (length >= 2 && raw[0] == '@' && raw[1] == '@') {
    start += 2;
    raw += 2;
    length -= 2;
    const struct {
      const char *prefix;
      MyliteSetVariableScope scope;
    } prefixes[] = {
        {"global.", MYLITE_SET_VARIABLE_SCOPE_GLOBAL},
        {"session.", MYLITE_SET_VARIABLE_SCOPE_SESSION},
        {"local.", MYLITE_SET_VARIABLE_SCOPE_LOCAL},
        {"instance.", MYLITE_SET_VARIABLE_SCOPE_INSTANCE},
    };
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
      size_t prefix_length = strlen(prefixes[i].prefix);
      if (mylite_ast_ascii_case_has_prefix(raw, length, prefixes[i].prefix)) {
        assignment->scope = prefixes[i].scope;
        start += prefix_length;
        raw += prefix_length;
        length -= prefix_length;
        break;
      }
    }
  } else if (length >= 1 && raw[0] == '@') {
    start += 1;
    raw += 1;
    length -= 1;
  }

  assignment->name_start = start;
  assignment->name_end = node->end;
  if (start < node->end &&
      (ast->source[start] == '`' || ast->source[start] == '"')) {
    return mylite_ast_decode_identifier(ast, start, node->end,
                                        &assignment->name_value,
                                        &assignment->name_value_length);
  }
  return mylite_ast_copy_source_span(ast, start, node->end,
                                     &assignment->name_value,
                                     &assignment->name_value_length);
}

static int mylite_ast_set_rename_table_view(MyliteAst *ast,
                                            MyliteAstStatement *statement,
                                            const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL) {
    return 1;
  }
  MyliteAstRenameTable *rename_table =
      mylite_ast_alloc(ast, sizeof(*rename_table));
  if (rename_table == NULL) {
    return 0;
  }
  rename_table->node = payload == NULL ? statement->node : payload;
  rename_table->start = mylite_ast_node_start(rename_table->node);
  rename_table->end = mylite_ast_node_end(rename_table->node);
  rename_table->targets = statement->targets;
  rename_table->target_count = statement->target_count;
  statement->rename_table = rename_table;
  return 1;
}

static int mylite_ast_set_truncate_table_view(MyliteAst *ast,
                                              MyliteAstStatement *statement,
                                              const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL) {
    return 1;
  }
  MyliteAstTruncateTable *truncate_table =
      mylite_ast_alloc(ast, sizeof(*truncate_table));
  if (truncate_table == NULL) {
    return 0;
  }
  truncate_table->node = payload == NULL ? statement->node : payload;
  truncate_table->start = mylite_ast_node_start(truncate_table->node);
  truncate_table->end = mylite_ast_node_end(truncate_table->node);
  if (statement->target_count > 0) {
    truncate_table->target = &statement->targets[0];
  }
  truncate_table->has_table_keyword =
      mylite_ast_find_first_token(truncate_table->node, MYLITE_TOK_TABLE_KWD) !=
      NULL;
  statement->truncate_table = truncate_table;
  return 1;
}

static int mylite_ast_set_use_database_view(MyliteAst *ast,
                                            MyliteAstStatement *statement,
                                            const MyliteAstNode *payload) {
  if (ast == NULL || statement == NULL) {
    return 1;
  }
  MyliteAstUseDatabase *use_database =
      mylite_ast_alloc(ast, sizeof(*use_database));
  if (use_database == NULL) {
    return 0;
  }
  use_database->node = payload == NULL ? statement->node : payload;
  use_database->start = mylite_ast_node_start(use_database->node);
  use_database->end = mylite_ast_node_end(use_database->node);
  if (statement->target_count > 0) {
    use_database->target = &statement->targets[0];
  }
  statement->use_database = use_database;
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
    case MYLITE_STATEMENT_USE:
      return MYLITE_STATEMENT_TARGET_DATABASE;
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
    if (statement->symbol_name != NULL &&
        strcmp(statement->symbol_name, "nt_drop_view_stmt") == 0) {
      return mylite_ast_collect_symbol_targets(ast, statement, payload,
                                               "nt_table_name", kind,
                                               MYLITE_STATEMENT_TARGET_ROLE_PRIMARY);
    }
    const MyliteAstNode *view_name =
        mylite_ast_find_first_symbol(payload, "nt_view_name");
    if (view_name != NULL) {
      target = mylite_ast_find_first_symbol(view_name, "nt_table_name");
      if (target == NULL) {
        target = view_name;
      }
    } else {
      target = mylite_ast_find_first_symbol(payload, "nt_table_name");
    }
  } else if (kind == MYLITE_STATEMENT_TARGET_ROUTINE) {
    target = mylite_ast_find_first_symbol(payload, "nt_table_name");
  } else if (kind == MYLITE_STATEMENT_TARGET_ACCOUNT) {
    target = mylite_ast_find_first_symbol(payload, "nt_username");
  } else if (kind == MYLITE_STATEMENT_TARGET_VARIABLE) {
    if (statement->kind == MYLITE_STATEMENT_SET) {
      return mylite_ast_collect_set_targets(ast, statement, payload);
    }
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

static int mylite_ast_collect_set_targets(MyliteAst *ast,
                                          MyliteAstStatement *statement,
                                          const MyliteAstNode *node) {
  if (node == NULL) {
    return 1;
  }
  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_variable_assignment") == 0) {
    const MyliteAstNode *target = mylite_ast_set_target_node(node);
    if (target == NULL) {
      return 1;
    }
    return mylite_ast_append_statement_target(
        ast, statement, MYLITE_STATEMENT_TARGET_VARIABLE,
        MYLITE_STATEMENT_TARGET_ROLE_PRIMARY, target);
  }
  if (mylite_ast_is_nested_target_boundary(node)) {
    return 1;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    if (!mylite_ast_collect_set_targets(ast, statement, node->children[i])) {
      return 0;
    }
  }
  return 1;
}

static const MyliteAstNode *mylite_ast_set_target_node(
    const MyliteAstNode *node) {
  if (node == NULL) {
    return NULL;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *child = node->children[i];
    if (child == NULL) {
      continue;
    }
    if (child->kind == MYLITE_AST_NODE_TOKEN &&
        (child->token == MYLITE_TOK_DOUBLE_AT_IDENTIFIER ||
         child->token == MYLITE_TOK_SINGLE_AT_IDENTIFIER)) {
      return child;
    }
    if (child->symbol_name != NULL &&
        strcmp(child->symbol_name, "nt_variable_name") == 0) {
      return child;
    }
  }
  return NULL;
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
  if (!mylite_ast_fill_statement_target(ast, &targets[statement->target_count],
                                        kind, role, target)) {
    return 0;
  }
  statement->targets = targets;
  statement->target_count = next_count;
  return 1;
}

static int mylite_ast_fill_statement_target(MyliteAst *ast,
                                            MyliteAstStatementTarget *target,
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
  return mylite_ast_set_statement_target_values(ast, target);
}

static int mylite_ast_set_statement_target_values(MyliteAst *ast,
                                                  MyliteAstStatementTarget *target) {
  if (ast == NULL || target == NULL) {
    return 1;
  }
  if (target->schema_start < target->schema_end &&
      target->schema_end <= ast->source_length &&
      !mylite_ast_decode_identifier(ast, target->schema_start,
                                    target->schema_end, &target->schema_value,
                                    &target->schema_value_length)) {
    return 0;
  }
  if (target->name_start < target->name_end &&
      target->name_end <= ast->source_length &&
      !mylite_ast_decode_identifier(ast, target->name_start, target->name_end,
                                    &target->name_value,
                                    &target->name_value_length)) {
    return 0;
  }
  return 1;
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
  return mylite_ast_set_create_table_column_metadata(ast, column);
}

static int mylite_ast_set_create_table_column_metadata(
    MyliteAst *ast, MyliteAstCreateTableColumn *column) {
  mylite_ast_set_create_table_column_nullability(column);
  mylite_ast_set_create_table_column_generated_storage(column);
  return mylite_ast_set_create_table_column_type_value_metadata(ast, column) &&
         mylite_ast_set_create_table_column_default_value_metadata(ast,
                                                                   column) &&
         mylite_ast_set_create_table_column_on_update_value_metadata(ast,
                                                                     column) &&
         mylite_ast_set_create_table_column_comment_value_metadata(ast,
                                                                   column);
}

static void mylite_ast_set_create_table_column_nullability(
    MyliteAstCreateTableColumn *column) {
  if (column == NULL) {
    return;
  }
  if ((column->flags & MYLITE_CREATE_TABLE_COLUMN_FLAG_NOT_NULL) != 0) {
    column->nullability = MYLITE_CREATE_TABLE_COLUMN_NULLABILITY_NOT_NULL;
  } else if ((column->flags & MYLITE_CREATE_TABLE_COLUMN_FLAG_NULL) != 0) {
    column->nullability = MYLITE_CREATE_TABLE_COLUMN_NULLABILITY_NULL;
  }
}

static void mylite_ast_set_create_table_column_generated_storage(
    MyliteAstCreateTableColumn *column) {
  if (column == NULL ||
      (column->flags & MYLITE_CREATE_TABLE_COLUMN_FLAG_GENERATED) == 0) {
    return;
  }
  if ((column->flags & MYLITE_CREATE_TABLE_COLUMN_FLAG_STORED) != 0) {
    column->generated_storage_kind =
        MYLITE_CREATE_TABLE_COLUMN_GENERATED_STORAGE_STORED;
  } else {
    column->generated_storage_kind =
        MYLITE_CREATE_TABLE_COLUMN_GENERATED_STORAGE_VIRTUAL;
  }
}

static int mylite_ast_set_create_table_column_type_value_metadata(
    MyliteAst *ast, MyliteAstCreateTableColumn *column) {
  if (ast == NULL || column == NULL) {
    return 1;
  }
  if (column->type_charset_value_start < column->type_charset_value_end &&
      !mylite_ast_decode_identifier(ast, column->type_charset_value_start,
                                    column->type_charset_value_end,
                                    &column->type_charset_value,
                                    &column->type_charset_value_length)) {
    return 0;
  }
  if (column->type_collation_value_start < column->type_collation_value_end &&
      !mylite_ast_decode_identifier(ast, column->type_collation_value_start,
                                    column->type_collation_value_end,
                                    &column->type_collation_value,
                                    &column->type_collation_value_length)) {
    return 0;
  }
  return 1;
}

static int mylite_ast_set_create_table_column_default_value_metadata(
    MyliteAst *ast, MyliteAstCreateTableColumn *column) {
  if (column == NULL || column->default_value_start >= column->default_value_end) {
    return 1;
  }
  return mylite_ast_set_create_table_column_value_metadata(
      ast, &column->default_value_kind, &column->default_value,
      &column->default_value_length, &column->default_unsigned_integer_value,
      &column->has_default_unsigned_integer_value, column->default_value_node,
      column->default_value_start, column->default_value_end);
}

static int mylite_ast_set_create_table_column_on_update_value_metadata(
    MyliteAst *ast, MyliteAstCreateTableColumn *column) {
  if (column == NULL ||
      column->on_update_value_start >= column->on_update_value_end) {
    return 1;
  }
  return mylite_ast_set_create_table_column_value_metadata(
      ast, &column->on_update_value_kind, &column->on_update_value,
      &column->on_update_value_length, NULL, NULL, column->on_update_value_node,
      column->on_update_value_start, column->on_update_value_end);
}

static int mylite_ast_set_create_table_column_comment_value_metadata(
    MyliteAst *ast, MyliteAstCreateTableColumn *column) {
  if (ast == NULL || ast->source == NULL || column == NULL ||
      column->comment_value_start >= column->comment_value_end ||
      column->comment_value_end > ast->source_length) {
    return 1;
  }
  column->comment_value = ast->source + column->comment_value_start;
  column->comment_value_length =
      column->comment_value_end - column->comment_value_start;

  const MyliteAstNode *token = mylite_ast_find_value_token_in_span(
      column->comment_node, column->comment_value_start, column->comment_value_end);
  if (token != NULL && token->token == MYLITE_TOK_STRING_LIT) {
    return mylite_ast_decode_sql_string_literal(
        ast, token->start, token->end, &column->comment_value,
        &column->comment_value_length);
  }
  return 1;
}

static int mylite_ast_set_create_table_column_value_metadata(
    MyliteAst *ast, MyliteCreateTableColumnValueKind *kind,
    const char **value, size_t *value_length,
    unsigned long long *unsigned_integer_value,
    int *has_unsigned_integer_value, const MyliteAstNode *node, size_t start,
    size_t end) {
  if (ast == NULL || ast->source == NULL || kind == NULL || value == NULL ||
      value_length == NULL || start >= end || end > ast->source_length) {
    return 1;
  }

  *kind = MYLITE_CREATE_TABLE_COLUMN_VALUE_RAW;
  *value = ast->source + start;
  *value_length = end - start;

  const MyliteAstNode *token =
      mylite_ast_find_value_token_in_span(node, start, end);
  if (token == NULL) {
    return 1;
  }

  if (token->token == MYLITE_TOK_STRING_LIT) {
    *kind = MYLITE_CREATE_TABLE_COLUMN_VALUE_STRING;
    return mylite_ast_decode_sql_string_literal(ast, token->start, token->end,
                                                value, value_length);
  }
  if (token->token == MYLITE_TOK_NULL) {
    *kind = MYLITE_CREATE_TABLE_COLUMN_VALUE_NULL;
    return 1;
  }
  if (token->token == MYLITE_TOK_CURRENT_TS ||
      token->token == MYLITE_TOK_LOCAL_TS) {
    *kind = MYLITE_CREATE_TABLE_COLUMN_VALUE_CURRENT_TIMESTAMP;
    return 1;
  }
  if (token->token == MYLITE_TOK_INT_LIT && token->start == start &&
      token->end == end && unsigned_integer_value != NULL &&
      has_unsigned_integer_value != NULL &&
      mylite_ast_parse_unsigned_integer_value(ast->source, start, end,
                                              unsigned_integer_value)) {
    *kind = MYLITE_CREATE_TABLE_COLUMN_VALUE_UNSIGNED_INTEGER;
    *has_unsigned_integer_value = 1;
    return 1;
  }
  return 1;
}

static const MyliteAstNode *mylite_ast_find_value_token_in_span(
    const MyliteAstNode *node, size_t start, size_t end) {
  if (node == NULL || !node->has_span || node->end <= start || node->start >= end) {
    return NULL;
  }
  if (node->kind == MYLITE_AST_NODE_TOKEN) {
    return node->start >= start && node->end <= end ? node : NULL;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *found =
        mylite_ast_find_value_token_in_span(node->children[i], start, end);
    if (found != NULL) {
      return found;
    }
  }
  return NULL;
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

  MyliteAstCreateTableColumnTypeElement *elements =
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
    MyliteAstCreateTableColumnTypeElement *elements, const MyliteAstNode *node,
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
    MyliteAstCreateTableColumnTypeElement *element = &column->type_elements[i];
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

static int mylite_ast_copy_source_span(MyliteAst *ast, size_t start,
                                       size_t end, const char **value,
                                       size_t *value_length) {
  if (ast == NULL || ast->source == NULL || value == NULL ||
      value_length == NULL || start >= end || end > ast->source_length) {
    return 1;
  }
  char *copy = mylite_ast_alloc(ast, end - start + 1);
  if (copy == NULL) {
    return 0;
  }
  memcpy(copy, ast->source + start, end - start);
  copy[end - start] = '\0';
  *value = copy;
  *value_length = end - start;
  return 1;
}

static int mylite_ast_ascii_case_equal_span(const char *value, size_t length,
                                            const char *expected) {
  if (value == NULL || expected == NULL || strlen(expected) != length) {
    return 0;
  }
  for (size_t i = 0; i < length; i++) {
    if (tolower((unsigned char)value[i]) !=
        tolower((unsigned char)expected[i])) {
      return 0;
    }
  }
  return 1;
}

static int mylite_ast_ascii_case_has_prefix(const char *value, size_t length,
                                            const char *prefix) {
  size_t prefix_length = prefix == NULL ? 0 : strlen(prefix);
  if (value == NULL || prefix == NULL || length < prefix_length) {
    return 0;
  }
  return mylite_ast_ascii_case_equal_span(value, prefix_length, prefix);
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
  if (!mylite_ast_set_create_table_key_name_values(ast, key)) {
    return 0;
  }
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
    if (!mylite_ast_set_create_table_key_reference(ast, key, elem)) {
      return 0;
    }
  } else if (key->kind == MYLITE_CREATE_TABLE_KEY_CHECK) {
    mylite_ast_set_create_table_key_check(key, elem);
  }
  mylite_ast_set_create_table_key_summary(key);
  return 1;
}

static void mylite_ast_set_create_table_key_summary(
    MyliteAstCreateTableKey *key) {
  if (key == NULL) {
    return;
  }
  for (size_t i = 0; i < key->option_count; i++) {
    const MyliteAstCreateTableKeyOption *option = &key->options[i];
    switch (option->kind) {
      case MYLITE_CREATE_TABLE_KEY_OPTION_INDEX_TYPE:
        if (option->index_type_kind !=
            MYLITE_CREATE_TABLE_INDEX_TYPE_UNSPECIFIED) {
          key->index_type_kind = option->index_type_kind;
        }
        break;
      case MYLITE_CREATE_TABLE_KEY_OPTION_COMMENT:
        key->comment_option = option;
        break;
      case MYLITE_CREATE_TABLE_KEY_OPTION_WITH_PARSER:
        key->parser_option = option;
        break;
      case MYLITE_CREATE_TABLE_KEY_OPTION_KEY_BLOCK_SIZE:
        key->key_block_size_option = option;
        break;
      case MYLITE_CREATE_TABLE_KEY_OPTION_VISIBLE:
        key->visibility = MYLITE_CREATE_TABLE_KEY_VISIBILITY_VISIBLE;
        break;
      case MYLITE_CREATE_TABLE_KEY_OPTION_INVISIBLE:
        key->visibility = MYLITE_CREATE_TABLE_KEY_VISIBILITY_INVISIBLE;
        break;
      case MYLITE_CREATE_TABLE_KEY_OPTION_UNKNOWN:
      case MYLITE_CREATE_TABLE_KEY_OPTION_SECONDARY_ENGINE_ATTRIBUTE:
      case MYLITE_CREATE_TABLE_KEY_OPTION_WHERE:
        break;
    }
  }
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

static int mylite_ast_set_create_table_key_name_values(
    MyliteAst *ast, MyliteAstCreateTableKey *key) {
  if (key == NULL) {
    return 1;
  }
  if (!mylite_ast_decode_identifier(ast, key->constraint_name_start,
                                    key->constraint_name_end,
                                    &key->constraint_name_value,
                                    &key->constraint_name_value_length)) {
    return 0;
  }
  return mylite_ast_decode_identifier(ast, key->name_start, key->name_end,
                                      &key->name_value,
                                      &key->name_value_length);
}

static void mylite_ast_set_create_table_key_index_type(
    MyliteAstCreateTableKey *key, const MyliteAstNode *constraint_elem) {
  const MyliteAstNode *index_type =
      mylite_ast_find_first_symbol(constraint_elem, "nt_index_type");
  if (index_type != NULL && index_type->has_span) {
    key->index_type_start = mylite_ast_node_start(index_type);
    key->index_type_end = mylite_ast_node_end(index_type);
    key->index_type_kind = mylite_ast_classify_index_type(index_type);
  }
}

static MyliteCreateTableIndexType mylite_ast_classify_index_type(
    const MyliteAstNode *node) {
  if (mylite_ast_find_first_token(node, MYLITE_TOK_BTREE) != NULL) {
    return MYLITE_CREATE_TABLE_INDEX_TYPE_BTREE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_HASH) != NULL) {
    return MYLITE_CREATE_TABLE_INDEX_TYPE_HASH;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_RTREE) != NULL) {
    return MYLITE_CREATE_TABLE_INDEX_TYPE_RTREE;
  }
  return MYLITE_CREATE_TABLE_INDEX_TYPE_UNSPECIFIED;
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
    if (!mylite_ast_set_create_table_key_referenced_table_values(ast, key)) {
      return 0;
    }
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

static int mylite_ast_set_create_table_key_referenced_table_values(
    MyliteAst *ast, MyliteAstCreateTableKey *key) {
  if (key == NULL) {
    return 1;
  }
  if (!mylite_ast_decode_identifier(
          ast, key->referenced_table_schema_start,
          key->referenced_table_schema_end,
          &key->referenced_table_schema_value,
          &key->referenced_table_schema_value_length)) {
    return 0;
  }
  return mylite_ast_decode_identifier(ast, key->referenced_table_name_start,
                                      key->referenced_table_name_end,
                                      &key->referenced_table_name_value,
                                      &key->referenced_table_name_value_length);
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
  if (index != count ||
      !mylite_ast_set_create_table_key_part_name_values(ast, parts, count)) {
    return 0;
  }
  if (referenced) {
    key->referenced_columns = parts;
    key->referenced_column_count = count;
  } else {
    key->columns = parts;
    key->column_count = count;
  }
  return 1;
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
  int ok = 1;
  mylite_ast_fill_index_options(options, count, list, &index, &ok, ast);
  key->options = options;
  key->option_count = count;
  return ok && index == count;
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

static int mylite_ast_set_create_table_key_part_name_values(
    MyliteAst *ast, MyliteAstCreateTableKeyPart *parts, size_t count) {
  if (parts == NULL) {
    return 1;
  }
  for (size_t i = 0; i < count; i++) {
    if (!mylite_ast_decode_identifier(ast, parts[i].name_start,
                                      parts[i].name_end, &parts[i].name_value,
                                      &parts[i].name_value_length)) {
      return 0;
    }
  }
  return 1;
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
                                          size_t *index, int *ok,
                                          MyliteAst *ast) {
  if (options == NULL || node == NULL || index == NULL || ok == NULL ||
      !*ok || *index >= count) {
    return;
  }
  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_index_option") == 0) {
    *ok = mylite_ast_fill_key_option(ast, &options[*index], node);
    if (*ok) {
      (*index)++;
    }
    return;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    mylite_ast_fill_index_options(options, count, node->children[i], index, ok,
                                  ast);
    if (!*ok) {
      return;
    }
  }
}

static int mylite_ast_fill_key_option(MyliteAst *ast,
                                      MyliteAstCreateTableKeyOption *option,
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
  return mylite_ast_set_key_option_value_metadata(ast, option, node);
}

static int mylite_ast_set_key_option_value_metadata(
    MyliteAst *ast, MyliteAstCreateTableKeyOption *option,
    const MyliteAstNode *node) {
  if (ast == NULL || ast->source == NULL || option == NULL ||
      option->value_start >= option->value_end ||
      option->value_end > ast->source_length) {
    return 1;
  }

  option->value = ast->source + option->value_start;
  option->value_length = option->value_end - option->value_start;

  const MyliteAstNode *token =
      mylite_ast_find_key_option_value_token(node, option->name_end);
  if (token == NULL) {
    option->value_kind = MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_RAW;
    return 1;
  }

  if (option->kind == MYLITE_CREATE_TABLE_KEY_OPTION_INDEX_TYPE) {
    option->index_type_kind = mylite_ast_classify_index_type(node);
    option->value_kind = MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_INDEX_TYPE;
    return 1;
  }
  if (option->kind == MYLITE_CREATE_TABLE_KEY_OPTION_KEY_BLOCK_SIZE) {
    unsigned long long value = 0;
    if (mylite_ast_parse_unsigned_integer_value(ast->source,
                                                option->value_start,
                                                option->value_end, &value)) {
      option->value_kind =
          MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_UNSIGNED_INTEGER;
      option->unsigned_integer_value = value;
      option->has_unsigned_integer_value = 1;
      return 1;
    }
  }
  if (token->token == MYLITE_TOK_STRING_LIT) {
    option->value_kind = MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_STRING;
    return mylite_ast_decode_sql_string_literal(ast, token->start, token->end,
                                                &option->value,
                                                &option->value_length);
  }
  if (token->start == option->value_start && token->end == option->value_end) {
    option->value_kind = MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_IDENTIFIER;
    return mylite_ast_decode_identifier(ast, token->start, token->end,
                                        &option->value,
                                        &option->value_length);
  }
  option->value_kind = MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_RAW;
  return 1;
}

static const MyliteAstNode *mylite_ast_find_key_option_value_token(
    const MyliteAstNode *node, size_t min_start) {
  if (node == NULL || !node->has_span || node->end <= min_start) {
    return NULL;
  }
  if (node->kind == MYLITE_AST_NODE_TOKEN) {
    return node->start >= min_start && node->token != MYLITE_TOK_EQ ? node
                                                                    : NULL;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *found =
        mylite_ast_find_key_option_value_token(node->children[i], min_start);
    if (found != NULL) {
      return found;
    }
  }
  return NULL;
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

static int mylite_ast_set_database_options(
    MyliteAst *ast, MyliteAstCreateDatabase *create_database,
    const MyliteAstNode *payload) {
  if (ast == NULL || create_database == NULL || payload == NULL) {
    return 1;
  }

  const MyliteAstNode *option_list =
      mylite_ast_find_first_symbol(payload, "nt_database_option_list_opt");
  if (option_list == NULL) {
    option_list = mylite_ast_find_first_symbol(payload,
                                               "nt_database_option_list");
  }
  size_t count = mylite_ast_count_database_options(option_list);
  if (count == 0) {
    return 1;
  }

  create_database->options =
      mylite_ast_alloc(ast, count * sizeof(*create_database->options));
  if (create_database->options == NULL) {
    return 0;
  }
  create_database->option_count = count;

  size_t index = 0;
  int ok = 1;
  mylite_ast_fill_database_options(ast, create_database->options,
                                   create_database->option_count, option_list,
                                   &index, &ok);
  return ok && index == count;
}

static size_t mylite_ast_count_database_options(const MyliteAstNode *node) {
  if (node == NULL) {
    return 0;
  }
  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_database_option") == 0) {
    return 1;
  }

  size_t count = 0;
  for (size_t i = 0; i < node->child_count; i++) {
    count += mylite_ast_count_database_options(node->children[i]);
  }
  return count;
}

static void mylite_ast_fill_database_options(
    MyliteAst *ast, MyliteAstDatabaseOption *options, size_t option_count,
    const MyliteAstNode *node, size_t *index, int *ok) {
  if (options == NULL || node == NULL || index == NULL || ok == NULL || !*ok ||
      *index >= option_count) {
    return;
  }
  if (node->symbol_name != NULL &&
      strcmp(node->symbol_name, "nt_database_option") == 0) {
    *ok = mylite_ast_fill_database_option(ast, &options[*index], node);
    if (*ok) {
      (*index)++;
    }
    return;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    mylite_ast_fill_database_options(ast, options, option_count,
                                     node->children[i], index, ok);
    if (!*ok) {
      return;
    }
  }
}

static int mylite_ast_fill_database_option(
    MyliteAst *ast, MyliteAstDatabaseOption *option,
    const MyliteAstNode *node) {
  option->node = node;
  option->kind = mylite_ast_classify_database_option(node);
  option->start = mylite_ast_node_start(node);
  option->end = mylite_ast_node_end(node);
  mylite_ast_set_database_option_name(option, node);
  mylite_ast_set_database_option_value(option, node);
  return mylite_ast_set_database_option_value_metadata(ast, option, node);
}

static MyliteDatabaseOptionKind mylite_ast_classify_database_option(
    const MyliteAstNode *node) {
  if (node == NULL) {
    return MYLITE_DATABASE_OPTION_UNKNOWN;
  }
  if (mylite_ast_find_first_symbol(node, "nt_charset_kw") != NULL) {
    return MYLITE_DATABASE_OPTION_CHARSET;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_COLLATE) != NULL) {
    return MYLITE_DATABASE_OPTION_COLLATE;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_ENCRYPTION) != NULL) {
    return MYLITE_DATABASE_OPTION_ENCRYPTION;
  }
  if (mylite_ast_find_first_token(node, MYLITE_TOK_SET) != NULL &&
      mylite_ast_find_first_token(node, MYLITE_TOK_TI_FLASH) != NULL &&
      mylite_ast_find_first_token(node, MYLITE_TOK_REPLICA) != NULL) {
    return MYLITE_DATABASE_OPTION_TI_FLASH_REPLICA;
  }
  if ((mylite_ast_find_first_token(node, MYLITE_TOK_READ) != NULL &&
       mylite_ast_find_first_token(node, MYLITE_TOK_ONLY) != NULL) ||
      mylite_ast_find_first_token(node, MYLITE_TOK_READ_ONLY) != NULL) {
    return MYLITE_DATABASE_OPTION_READ_ONLY;
  }
  if (mylite_ast_find_first_symbol(node, "nt_placement_policy_option") != NULL) {
    return MYLITE_DATABASE_OPTION_PLACEMENT_POLICY;
  }
  return MYLITE_DATABASE_OPTION_UNKNOWN;
}

static void mylite_ast_set_database_option_name(
    MyliteAstDatabaseOption *option, const MyliteAstNode *node) {
  if (option == NULL || node == NULL) {
    return;
  }

  const MyliteAstNode *name =
      mylite_ast_find_database_option_name(node, option->kind);
  if (name != NULL) {
    option->name_start = mylite_ast_node_start(name);
    option->name_end = mylite_ast_node_end(name);
  }

  if (option->kind == MYLITE_DATABASE_OPTION_READ_ONLY) {
    const MyliteAstNode *only = mylite_ast_find_first_token(node,
                                                           MYLITE_TOK_ONLY);
    if (only != NULL) {
      option->name_end = mylite_ast_node_end(only);
    } else {
      const MyliteAstNode *read_only =
          mylite_ast_find_first_token(node, MYLITE_TOK_READ_ONLY);
      if (read_only != NULL) {
        option->name_start = mylite_ast_node_start(read_only);
        option->name_end = mylite_ast_node_end(read_only);
      }
    }
  } else if (option->kind == MYLITE_DATABASE_OPTION_TI_FLASH_REPLICA) {
    const MyliteAstNode *replica =
        mylite_ast_find_first_token(node, MYLITE_TOK_REPLICA);
    if (replica != NULL) {
      option->name_end = mylite_ast_node_end(replica);
    }
  }
}

static void mylite_ast_set_database_option_value(
    MyliteAstDatabaseOption *option, const MyliteAstNode *node) {
  if (option == NULL || option->name_end == 0) {
    return;
  }
  mylite_ast_collect_value_span_after(node, option->name_end,
                                      &option->value_start,
                                      &option->value_end);
}

static int mylite_ast_set_database_option_value_metadata(
    MyliteAst *ast, MyliteAstDatabaseOption *option,
    const MyliteAstNode *node) {
  if (ast == NULL || ast->source == NULL || option == NULL ||
      option->value_start >= option->value_end ||
      option->value_end > ast->source_length) {
    return 1;
  }

  option->value = ast->source + option->value_start;
  option->value_length = option->value_end - option->value_start;

  const MyliteAstNode *token =
      mylite_ast_find_database_option_value_token(node, option->name_end);
  if (token == NULL) {
    option->value_kind = MYLITE_DATABASE_OPTION_VALUE_RAW;
    return 1;
  }

  if (token->token == MYLITE_TOK_DEFAULT_KWD) {
    option->value_kind = MYLITE_DATABASE_OPTION_VALUE_DEFAULT;
    return 1;
  }

  if (token->token == MYLITE_TOK_STRING_LIT) {
    option->value_kind = MYLITE_DATABASE_OPTION_VALUE_STRING;
    return mylite_ast_decode_sql_string_literal(ast, token->start, token->end,
                                                &option->value,
                                                &option->value_length);
  }

  if (token->start == option->value_start && token->end == option->value_end) {
    option->value_kind = MYLITE_DATABASE_OPTION_VALUE_IDENTIFIER;
    return mylite_ast_decode_identifier(ast, token->start, token->end,
                                        &option->value,
                                        &option->value_length);
  }

  option->value_kind = MYLITE_DATABASE_OPTION_VALUE_RAW;
  return 1;
}

static const MyliteAstNode *mylite_ast_find_database_option_value_token(
    const MyliteAstNode *node, size_t min_start) {
  if (node == NULL || !node->has_span || node->end <= min_start) {
    return NULL;
  }
  if (node->kind == MYLITE_AST_NODE_TOKEN) {
    return node->start >= min_start && node->token != MYLITE_TOK_EQ ? node
                                                                    : NULL;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *found =
        mylite_ast_find_database_option_value_token(node->children[i],
                                                    min_start);
    if (found != NULL) {
      return found;
    }
  }
  return NULL;
}

static const MyliteAstNode *mylite_ast_find_database_option_name(
    const MyliteAstNode *node, MyliteDatabaseOptionKind kind) {
  if (kind == MYLITE_DATABASE_OPTION_CHARSET) {
    return mylite_ast_find_first_symbol(node, "nt_charset_kw");
  }
  if (kind == MYLITE_DATABASE_OPTION_PLACEMENT_POLICY) {
    const MyliteAstNode *placement =
        mylite_ast_find_first_symbol(node, "nt_placement_policy_option");
    return placement != NULL ? placement
                             : mylite_ast_find_first_spanned_child(node);
  }

  int token = 0;
  switch (kind) {
    case MYLITE_DATABASE_OPTION_COLLATE:
      token = MYLITE_TOK_COLLATE;
      break;
    case MYLITE_DATABASE_OPTION_ENCRYPTION:
      token = MYLITE_TOK_ENCRYPTION;
      break;
    case MYLITE_DATABASE_OPTION_TI_FLASH_REPLICA:
      token = MYLITE_TOK_SET;
      break;
    case MYLITE_DATABASE_OPTION_READ_ONLY:
      token = MYLITE_TOK_READ;
      break;
    case MYLITE_DATABASE_OPTION_UNKNOWN:
    case MYLITE_DATABASE_OPTION_CHARSET:
    case MYLITE_DATABASE_OPTION_PLACEMENT_POLICY:
      break;
  }
  return token == 0 ? mylite_ast_find_first_spanned_child(node)
                    : mylite_ast_find_first_token(node, token);
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
  int ok = 1;
  mylite_ast_fill_create_table_options(ast, statement, option_list, &index,
                                       &ok);
  return ok && index == count;
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
    MyliteAst *ast, MyliteAstStatement *statement, const MyliteAstNode *node,
    size_t *index, int *ok) {
  if (statement == NULL || node == NULL || index == NULL || ok == NULL ||
      !*ok || *index >= statement->create_table_option_count) {
    return;
  }
  if (node->symbol_name != NULL && strcmp(node->symbol_name, "nt_table_option") == 0) {
    *ok = mylite_ast_fill_create_table_option(
        ast, &statement->create_table_options[*index], node);
    if (*ok) {
      (*index)++;
    }
    return;
  }

  for (size_t i = 0; i < node->child_count; i++) {
    mylite_ast_fill_create_table_options(ast, statement, node->children[i],
                                         index, ok);
    if (!*ok) {
      return;
    }
  }
}

static int mylite_ast_fill_create_table_option(
    MyliteAst *ast, MyliteAstCreateTableOption *option,
    const MyliteAstNode *node) {
  option->kind = mylite_ast_classify_create_table_option(node);
  option->start = mylite_ast_node_start(node);
  option->end = mylite_ast_node_end(node);
  mylite_ast_set_create_table_option_name(option, node);
  mylite_ast_set_create_table_option_value(option, node);
  return mylite_ast_set_create_table_option_value_metadata(ast, option, node);
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

static int mylite_ast_set_create_table_option_value_metadata(
    MyliteAst *ast, MyliteAstCreateTableOption *option,
    const MyliteAstNode *node) {
  if (ast == NULL || ast->source == NULL || option == NULL ||
      option->value_start >= option->value_end ||
      option->value_end > ast->source_length) {
    return 1;
  }

  option->value = ast->source + option->value_start;
  option->value_length = option->value_end - option->value_start;
  if (option->kind == MYLITE_CREATE_TABLE_OPTION_UNION) {
    option->value_kind = MYLITE_CREATE_TABLE_OPTION_VALUE_LIST;
    return 1;
  }

  const MyliteAstNode *token =
      mylite_ast_find_create_table_option_value_token(node, option->name_end);
  if (token == NULL) {
    option->value_kind = MYLITE_CREATE_TABLE_OPTION_VALUE_RAW;
    return 1;
  }

  if (mylite_ast_is_unsigned_integer_table_option(option->kind)) {
    unsigned long long value = 0;
    if (mylite_ast_parse_unsigned_integer_value(ast->source,
                                                option->value_start,
                                                option->value_end, &value)) {
      option->value_kind = MYLITE_CREATE_TABLE_OPTION_VALUE_UNSIGNED_INTEGER;
      option->unsigned_integer_value = value;
      option->has_unsigned_integer_value = 1;
      return 1;
    }
  }

  if (token->token == MYLITE_TOK_STRING_LIT) {
    option->value_kind = MYLITE_CREATE_TABLE_OPTION_VALUE_STRING;
    return mylite_ast_decode_sql_string_literal(ast, token->start, token->end,
                                                &option->value,
                                                &option->value_length);
  }

  if (token->start == option->value_start && token->end == option->value_end) {
    option->value_kind = MYLITE_CREATE_TABLE_OPTION_VALUE_IDENTIFIER;
    return mylite_ast_decode_identifier(ast, token->start, token->end,
                                        &option->value,
                                        &option->value_length);
  }

  option->value_kind = MYLITE_CREATE_TABLE_OPTION_VALUE_RAW;
  return 1;
}

static const MyliteAstNode *mylite_ast_find_create_table_option_value_token(
    const MyliteAstNode *node, size_t min_start) {
  if (node == NULL || !node->has_span || node->end <= min_start) {
    return NULL;
  }
  if (node->kind == MYLITE_AST_NODE_TOKEN) {
    return node->start >= min_start && node->token != MYLITE_TOK_EQ ? node
                                                                    : NULL;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    const MyliteAstNode *found =
        mylite_ast_find_create_table_option_value_token(node->children[i],
                                                        min_start);
    if (found != NULL) {
      return found;
    }
  }
  return NULL;
}

static int mylite_ast_is_unsigned_integer_table_option(
    MyliteCreateTableOptionKind kind) {
  switch (kind) {
    case MYLITE_CREATE_TABLE_OPTION_AUTO_INCREMENT:
    case MYLITE_CREATE_TABLE_OPTION_KEY_BLOCK_SIZE:
    case MYLITE_CREATE_TABLE_OPTION_AUTOEXTEND_SIZE:
    case MYLITE_CREATE_TABLE_OPTION_AVG_ROW_LENGTH:
    case MYLITE_CREATE_TABLE_OPTION_MAX_ROWS:
    case MYLITE_CREATE_TABLE_OPTION_MIN_ROWS:
    case MYLITE_CREATE_TABLE_OPTION_DELAY_KEY_WRITE:
      return 1;
    case MYLITE_CREATE_TABLE_OPTION_UNKNOWN:
    case MYLITE_CREATE_TABLE_OPTION_ENGINE:
    case MYLITE_CREATE_TABLE_OPTION_SECONDARY_ENGINE:
    case MYLITE_CREATE_TABLE_OPTION_CHARSET:
    case MYLITE_CREATE_TABLE_OPTION_COLLATE:
    case MYLITE_CREATE_TABLE_OPTION_COMMENT:
    case MYLITE_CREATE_TABLE_OPTION_ROW_FORMAT:
    case MYLITE_CREATE_TABLE_OPTION_ENCRYPTION:
    case MYLITE_CREATE_TABLE_OPTION_STATS_PERSISTENT:
    case MYLITE_CREATE_TABLE_OPTION_PACK_KEYS:
    case MYLITE_CREATE_TABLE_OPTION_TABLESPACE:
    case MYLITE_CREATE_TABLE_OPTION_STORAGE:
    case MYLITE_CREATE_TABLE_OPTION_COMPRESSION:
    case MYLITE_CREATE_TABLE_OPTION_CONNECTION:
    case MYLITE_CREATE_TABLE_OPTION_PASSWORD:
    case MYLITE_CREATE_TABLE_OPTION_INSERT_METHOD:
    case MYLITE_CREATE_TABLE_OPTION_DATA_DIRECTORY:
    case MYLITE_CREATE_TABLE_OPTION_INDEX_DIRECTORY:
    case MYLITE_CREATE_TABLE_OPTION_UNION:
    case MYLITE_CREATE_TABLE_OPTION_ENGINE_ATTRIBUTE:
    case MYLITE_CREATE_TABLE_OPTION_SECONDARY_ENGINE_ATTRIBUTE:
      return 0;
  }
  return 0;
}

static int mylite_ast_parse_unsigned_integer_value(const char *source,
                                                   size_t start, size_t end,
                                                   unsigned long long *value) {
  if (source == NULL || value == NULL || start >= end) {
    return 0;
  }
  unsigned long long parsed = 0;
  for (size_t offset = start; offset < end; offset++) {
    unsigned char ch = (unsigned char)source[offset];
    if (!isdigit(ch)) {
      return 0;
    }
    unsigned digit = (unsigned)(ch - '0');
    if (parsed > (ULLONG_MAX - digit) / 10) {
      return 0;
    }
    parsed = parsed * 10 + digit;
  }
  *value = parsed;
  return 1;
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

static const MyliteAstCreateTableColumnTypeElement *
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
