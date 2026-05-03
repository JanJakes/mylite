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
static void count_expression_tree(const MyliteAstExpression *expression,
                                  size_t *nodes, size_t *operators,
                                  size_t *leaf_values);
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
  size_t select_statement_views = 0;
  size_t select_statement_query_blocks = 0;
  size_t select_statement_projections = 0;
  size_t select_statement_projection_expressions = 0;
  size_t select_statement_projection_alias_values = 0;
  size_t select_statement_projection_wildcards = 0;
  size_t select_statement_projection_table_wildcards = 0;
  size_t select_statement_where_expressions = 0;
  size_t select_statement_having_expressions = 0;
  size_t select_statement_from_clauses = 0;
  size_t select_statement_group_by_clauses = 0;
  size_t select_statement_order_by_clauses = 0;
  size_t select_statement_limit_clauses = 0;
  size_t select_statement_into_clauses = 0;
  size_t select_statement_lock_clauses = 0;
  size_t select_statement_with_clauses = 0;
  size_t select_statement_set_operations = 0;
  size_t select_statement_expression_tree_nodes = 0;
  size_t select_statement_expression_tree_operators = 0;
  size_t select_statement_expression_tree_leaf_values = 0;
  size_t insert_statement_views = 0;
  size_t insert_statement_values_sources = 0;
  size_t insert_statement_set_sources = 0;
  size_t insert_statement_select_sources = 0;
  size_t insert_statement_priorities = 0;
  size_t insert_statement_ignores = 0;
  size_t insert_statement_partition_clauses = 0;
  size_t insert_statement_duplicate_clauses = 0;
  size_t insert_statement_columns = 0;
  size_t insert_statement_column_name_values = 0;
  size_t insert_statement_value_rows = 0;
  size_t insert_statement_values = 0;
  size_t insert_statement_default_values = 0;
  size_t insert_statement_set_assignments = 0;
  size_t insert_statement_duplicate_assignments = 0;
  size_t insert_statement_assignment_name_values = 0;
  size_t insert_statement_expression_tree_nodes = 0;
  size_t insert_statement_expression_tree_operators = 0;
  size_t insert_statement_expression_tree_leaf_values = 0;
  size_t replace_statement_views = 0;
  size_t replace_statement_values_sources = 0;
  size_t replace_statement_set_sources = 0;
  size_t replace_statement_select_sources = 0;
  size_t replace_statement_priorities = 0;
  size_t replace_statement_into_clauses = 0;
  size_t replace_statement_partition_clauses = 0;
  size_t replace_statement_columns = 0;
  size_t replace_statement_column_name_values = 0;
  size_t replace_statement_value_rows = 0;
  size_t replace_statement_values = 0;
  size_t replace_statement_default_values = 0;
  size_t replace_statement_set_assignments = 0;
  size_t replace_statement_assignment_name_values = 0;
  size_t replace_statement_expression_tree_nodes = 0;
  size_t replace_statement_expression_tree_operators = 0;
  size_t replace_statement_expression_tree_leaf_values = 0;
  size_t delete_statement_views = 0;
  size_t delete_statement_with_clauses = 0;
  size_t delete_statement_multi_table = 0;
  size_t delete_statement_multi_table_from = 0;
  size_t delete_statement_multi_table_using = 0;
  size_t delete_statement_priorities = 0;
  size_t delete_statement_quicks = 0;
  size_t delete_statement_ignores = 0;
  size_t delete_statement_targets = 0;
  size_t delete_statement_target_schema_values = 0;
  size_t delete_statement_target_name_values = 0;
  size_t delete_statement_target_wildcards = 0;
  size_t delete_statement_where_expressions = 0;
  size_t delete_statement_order_by_clauses = 0;
  size_t delete_statement_limit_clauses = 0;
  size_t delete_statement_expression_tree_nodes = 0;
  size_t delete_statement_expression_tree_operators = 0;
  size_t delete_statement_expression_tree_leaf_values = 0;
  size_t update_statement_views = 0;
  size_t update_statement_with_clauses = 0;
  size_t update_statement_multi_table = 0;
  size_t update_statement_priorities = 0;
  size_t update_statement_ignores = 0;
  size_t update_statement_assignments = 0;
  size_t update_statement_assignment_name_values = 0;
  size_t update_statement_where_expressions = 0;
  size_t update_statement_order_by_clauses = 0;
  size_t update_statement_limit_clauses = 0;
  size_t update_statement_expression_tree_nodes = 0;
  size_t update_statement_expression_tree_operators = 0;
  size_t update_statement_expression_tree_leaf_values = 0;
  size_t create_table_views = 0;
  size_t create_table_view_schema_values = 0;
  size_t create_table_view_name_values = 0;
  size_t create_table_view_summary_engines = 0;
  size_t create_table_view_summary_comments = 0;
  size_t create_table_view_summary_auto_increments = 0;
  size_t create_table_view_columns = 0;
  size_t create_table_view_keys = 0;
  size_t create_table_view_options = 0;
  size_t create_table_view_column_handles = 0;
  size_t create_table_view_known_column_types = 0;
  size_t create_table_view_column_type_numeric_params = 0;
  size_t create_table_view_column_type_element_handles = 0;
  size_t create_table_view_column_type_element_values = 0;
  size_t create_table_view_column_type_lengths = 0;
  size_t create_table_view_column_type_unsigned_attrs = 0;
  size_t create_table_view_column_type_charset_values = 0;
  size_t create_table_view_column_type_collation_values = 0;
  size_t create_table_view_column_option_spans = 0;
  size_t create_table_view_column_defaults = 0;
  size_t create_table_view_column_default_values = 0;
  size_t create_table_view_column_default_unsigned_values = 0;
  size_t create_table_view_column_on_update_values = 0;
  size_t create_table_view_column_comments = 0;
  size_t create_table_view_column_comment_values = 0;
  size_t create_table_view_column_checks = 0;
  size_t create_table_view_column_nullabilities = 0;
  size_t create_table_view_column_generated_storage_kinds = 0;
  size_t create_table_view_column_type_nodes = 0;
  size_t create_table_view_column_options_nodes = 0;
  size_t create_table_view_column_expression_roots = 0;
  size_t create_table_view_column_expression_tree_nodes = 0;
  size_t create_table_view_column_expression_tree_operators = 0;
  size_t create_table_view_column_expression_tree_leaf_values = 0;
  size_t create_table_view_key_handles = 0;
  size_t create_table_view_named_keys = 0;
  size_t create_table_view_key_index_types = 0;
  size_t create_table_view_key_visibilities = 0;
  size_t create_table_view_key_comments = 0;
  size_t create_table_view_key_parsers = 0;
  size_t create_table_view_key_block_sizes = 0;
  size_t create_table_view_key_column_handles = 0;
  size_t create_table_view_named_key_columns = 0;
  size_t create_table_view_ordered_key_columns = 0;
  size_t create_table_view_prefixed_key_columns = 0;
  size_t create_table_view_expression_key_columns = 0;
  size_t create_table_view_key_expression_roots = 0;
  size_t create_table_view_key_expression_tree_nodes = 0;
  size_t create_table_view_key_expression_tree_operators = 0;
  size_t create_table_view_key_expression_tree_leaf_values = 0;
  size_t create_table_view_referenced_column_handles = 0;
  size_t create_table_view_named_referenced_columns = 0;
  size_t create_table_view_key_option_handles = 0;
  size_t create_table_view_key_option_values = 0;
  size_t create_table_view_key_option_identifier_values = 0;
  size_t create_table_view_key_option_string_values = 0;
  size_t create_table_view_key_option_unsigned_integer_values = 0;
  size_t create_table_view_key_option_index_type_values = 0;
  size_t create_table_view_option_handles = 0;
  size_t create_table_view_option_values = 0;
  size_t create_table_view_option_identifier_values = 0;
  size_t create_table_view_option_string_values = 0;
  size_t create_table_view_option_unsigned_integer_values = 0;
  size_t create_table_view_option_list_values = 0;
  size_t alter_table_views = 0;
  size_t alter_table_schema_values = 0;
  size_t alter_table_name_values = 0;
  size_t alter_table_specs = 0;
  size_t alter_table_named_specs = 0;
  size_t alter_table_secondary_named_specs = 0;
  size_t alter_table_renamed_tables = 0;
  size_t alter_table_column_payloads = 0;
  size_t alter_table_column_known_types = 0;
  size_t alter_table_key_payloads = 0;
  size_t alter_table_key_columns = 0;
  size_t alter_table_options = 0;
  size_t alter_table_if_exists = 0;
  size_t alter_table_if_not_exists = 0;
  size_t create_database_views = 0;
  size_t create_database_name_values = 0;
  size_t create_database_options = 0;
  size_t create_database_option_values = 0;
  size_t create_database_charset_values = 0;
  size_t create_database_collation_values = 0;
  size_t create_database_encryption_values = 0;
  size_t create_database_if_not_exists = 0;
  size_t create_database_schema_keywords = 0;
  size_t create_index_views = 0;
  size_t create_index_name_values = 0;
  size_t create_index_table_name_values = 0;
  size_t create_index_columns = 0;
  size_t create_index_options = 0;
  size_t create_index_comments = 0;
  size_t create_index_key_block_sizes = 0;
  size_t create_view_views = 0;
  size_t create_view_schema_values = 0;
  size_t create_view_name_values = 0;
  size_t create_view_columns = 0;
  size_t create_view_column_values = 0;
  size_t create_view_or_replace = 0;
  size_t create_view_algorithms = 0;
  size_t create_view_sql_securities = 0;
  size_t create_view_check_options = 0;
  size_t create_view_select_nodes = 0;
  size_t drop_database_views = 0;
  size_t drop_database_name_values = 0;
  size_t drop_database_if_exists = 0;
  size_t drop_database_schema_keywords = 0;
  size_t drop_index_views = 0;
  size_t drop_index_name_values = 0;
  size_t drop_index_table_name_values = 0;
  size_t drop_index_if_exists = 0;
  size_t drop_table_views = 0;
  size_t drop_table_tables = 0;
  size_t drop_table_if_exists = 0;
  size_t drop_view_views = 0;
  size_t drop_view_view_targets = 0;
  size_t drop_view_if_exists = 0;
  size_t drop_view_modes = 0;
  size_t prepare_statement_views = 0;
  size_t prepare_statement_name_values = 0;
  size_t prepare_statement_string_sources = 0;
  size_t prepare_statement_user_variable_sources = 0;
  size_t prepare_statement_source_values = 0;
  size_t execute_statement_views = 0;
  size_t execute_statement_name_values = 0;
  size_t execute_statement_using_variables = 0;
  size_t execute_statement_using_variable_name_values = 0;
  size_t deallocate_statement_views = 0;
  size_t deallocate_statement_name_values = 0;
  size_t deallocate_statement_modes = 0;
  size_t rename_table_views = 0;
  size_t rename_table_pairs = 0;
  size_t set_statement_views = 0;
  size_t set_statement_assignments = 0;
  size_t set_assignment_name_values = 0;
  size_t set_assignment_scopes = 0;
  size_t set_assignment_value_nodes = 0;
  size_t set_assignment_value_expressions = 0;
  size_t set_assignment_expression_values = 0;
  size_t set_assignment_expression_unsigned_integers = 0;
  size_t set_assignment_expression_literals = 0;
  size_t set_assignment_expression_function_calls = 0;
  size_t set_assignment_expression_defaults = 0;
  size_t set_assignment_expression_tree_nodes = 0;
  size_t set_assignment_expression_tree_operators = 0;
  size_t set_assignment_expression_tree_leaf_values = 0;
  size_t set_assignment_extend_value_nodes = 0;
  size_t set_assignment_system_variables = 0;
  size_t set_assignment_user_variables = 0;
  size_t set_assignment_names = 0;
  size_t set_assignment_character_sets = 0;
  size_t set_assignment_transaction_characteristics = 0;
  size_t set_assignment_configs = 0;
  size_t truncate_table_views = 0;
  size_t truncate_table_name_values = 0;
  size_t truncate_table_table_keywords = 0;
  size_t transaction_statement_views = 0;
  size_t transaction_statement_begins = 0;
  size_t transaction_statement_commits = 0;
  size_t transaction_statement_rollbacks = 0;
  size_t transaction_statement_savepoints = 0;
  size_t transaction_statement_release_savepoints = 0;
  size_t transaction_statement_work_keywords = 0;
  size_t transaction_statement_access_modes = 0;
  size_t transaction_statement_consistent_snapshots = 0;
  size_t transaction_statement_completion_modifiers = 0;
  size_t transaction_statement_savepoint_names = 0;
  size_t use_database_views = 0;
  size_t use_database_name_values = 0;
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
            const MyliteAstSelectStatement *select_statement =
                mylite_ast_select_statement_view(ast, i);
            if (select_statement != NULL) {
              select_statement_views++;
              select_statement_query_blocks +=
                  mylite_ast_select_statement_view_query_block_count(
                      select_statement);
              select_statement_projections +=
                  mylite_ast_select_statement_view_projection_count(
                      select_statement);
              if (mylite_ast_select_statement_view_has_with_clause(
                      select_statement)) {
                select_statement_with_clauses++;
              }
              if (mylite_ast_select_statement_view_has_set_operation(
                      select_statement)) {
                select_statement_set_operations++;
              }
              if (mylite_ast_select_statement_view_from_start(
                      select_statement) !=
                  mylite_ast_select_statement_view_from_end(
                      select_statement)) {
                select_statement_from_clauses++;
              }
              if (mylite_ast_select_statement_view_group_by_start(
                      select_statement) !=
                  mylite_ast_select_statement_view_group_by_end(
                      select_statement)) {
                select_statement_group_by_clauses++;
              }
              if (mylite_ast_select_statement_view_order_by_start(
                      select_statement) !=
                  mylite_ast_select_statement_view_order_by_end(
                      select_statement)) {
                select_statement_order_by_clauses++;
              }
              if (mylite_ast_select_statement_view_limit_start(
                      select_statement) !=
                  mylite_ast_select_statement_view_limit_end(
                      select_statement)) {
                select_statement_limit_clauses++;
              }
              if (mylite_ast_select_statement_view_into_start(
                      select_statement) !=
                  mylite_ast_select_statement_view_into_end(select_statement)) {
                select_statement_into_clauses++;
              }
              if (mylite_ast_select_statement_view_lock_start(
                      select_statement) !=
                  mylite_ast_select_statement_view_lock_end(select_statement)) {
                select_statement_lock_clauses++;
              }
              const MyliteAstExpression *where_expression =
                  mylite_ast_select_statement_view_where_expression(
                      select_statement);
              if (where_expression != NULL) {
                select_statement_where_expressions++;
                count_expression_tree(
                    where_expression,
                    &select_statement_expression_tree_nodes,
                    &select_statement_expression_tree_operators,
                    &select_statement_expression_tree_leaf_values);
              }
              const MyliteAstExpression *having_expression =
                  mylite_ast_select_statement_view_having_expression(
                      select_statement);
              if (having_expression != NULL) {
                select_statement_having_expressions++;
                count_expression_tree(
                    having_expression,
                    &select_statement_expression_tree_nodes,
                    &select_statement_expression_tree_operators,
                    &select_statement_expression_tree_leaf_values);
              }
              for (size_t j = 0;
                   j < mylite_ast_select_statement_view_projection_count(
                           select_statement);
                   j++) {
                const MyliteAstSelectProjection *projection =
                    mylite_ast_select_statement_view_projection_at(
                        select_statement, j);
                if (mylite_ast_select_projection_view_expression(projection) !=
                    NULL) {
                  select_statement_projection_expressions++;
                  count_expression_tree(
                      mylite_ast_select_projection_view_expression(projection),
                      &select_statement_expression_tree_nodes,
                      &select_statement_expression_tree_operators,
                      &select_statement_expression_tree_leaf_values);
                }
                if (mylite_ast_select_projection_view_alias_value(projection) !=
                    NULL) {
                  select_statement_projection_alias_values++;
                }
                switch (mylite_ast_select_projection_view_kind(projection)) {
                  case MYLITE_SELECT_PROJECTION_WILDCARD:
                    select_statement_projection_wildcards++;
                    break;
                  case MYLITE_SELECT_PROJECTION_TABLE_WILDCARD:
                    select_statement_projection_table_wildcards++;
                    break;
                  case MYLITE_SELECT_PROJECTION_EXPRESSION:
                  case MYLITE_SELECT_PROJECTION_UNKNOWN:
                    break;
                }
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
              if (mylite_ast_create_table_view_engine_value(create_table) !=
                  NULL) {
                create_table_view_summary_engines++;
              }
              if (mylite_ast_create_table_view_comment_value(create_table) !=
                  NULL) {
                create_table_view_summary_comments++;
              }
              if (mylite_ast_create_table_view_has_auto_increment_value(
                      create_table)) {
                create_table_view_summary_auto_increments++;
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
                create_table_view_column_type_numeric_params +=
                    mylite_ast_create_table_column_view_type_numeric_parameter_count(
                        column);
                create_table_view_column_type_element_handles +=
                    mylite_ast_create_table_column_view_type_element_count(
                        column);
                for (size_t l = 0;
                     l <
                     mylite_ast_create_table_column_view_type_element_count(
                         column);
                     l++) {
                  const MyliteAstCreateTableColumnTypeElement *element =
                      mylite_ast_create_table_column_view_type_element_at(
                          column, l);
                  if (mylite_ast_create_table_column_type_element_view_value(
                          element) != NULL) {
                    create_table_view_column_type_element_values++;
                  }
                }
                if (mylite_ast_create_table_column_view_type_has_length(
                        column)) {
                  create_table_view_column_type_lengths++;
                }
                if (mylite_ast_create_table_column_view_type_unsigned_end(
                        column) != 0) {
                  create_table_view_column_type_unsigned_attrs++;
                }
                if (mylite_ast_create_table_column_view_type_charset_value(
                        column) != NULL) {
                  create_table_view_column_type_charset_values++;
                }
                if (mylite_ast_create_table_column_view_type_collation_value(
                        column) != NULL) {
                  create_table_view_column_type_collation_values++;
                }
                if (mylite_ast_create_table_column_view_options_start(
                        column) !=
                    mylite_ast_create_table_column_view_options_end(column)) {
                  create_table_view_column_option_spans++;
                }
                if (mylite_ast_create_table_column_view_default_start(
                        column) !=
                    mylite_ast_create_table_column_view_default_end(column)) {
                  create_table_view_column_defaults++;
                }
                if (mylite_ast_create_table_column_view_default_value_kind(
                        column) != MYLITE_CREATE_TABLE_COLUMN_VALUE_UNKNOWN) {
                  create_table_view_column_default_values++;
                }
                if (mylite_ast_create_table_column_view_has_default_unsigned_integer(
                        column)) {
                  create_table_view_column_default_unsigned_values++;
                }
                if (mylite_ast_create_table_column_view_on_update_value_kind(
                        column) != MYLITE_CREATE_TABLE_COLUMN_VALUE_UNKNOWN) {
                  create_table_view_column_on_update_values++;
                }
                if (mylite_ast_create_table_column_view_comment_start(
                        column) !=
                    mylite_ast_create_table_column_view_comment_end(column)) {
                  create_table_view_column_comments++;
                }
                if (mylite_ast_create_table_column_view_comment_value(column) !=
                    NULL) {
                  create_table_view_column_comment_values++;
                }
                if (mylite_ast_create_table_column_view_check_start(column) !=
                    mylite_ast_create_table_column_view_check_end(column)) {
                  create_table_view_column_checks++;
                }
                if (mylite_ast_create_table_column_view_nullability(column) !=
                    MYLITE_CREATE_TABLE_COLUMN_NULLABILITY_UNSPECIFIED) {
                  create_table_view_column_nullabilities++;
                }
                if (mylite_ast_create_table_column_view_generated_storage_kind(
                        column) !=
                    MYLITE_CREATE_TABLE_COLUMN_GENERATED_STORAGE_UNSPECIFIED) {
                  create_table_view_column_generated_storage_kinds++;
                }
                if (mylite_ast_create_table_column_view_type_node(column) !=
                    NULL) {
                  create_table_view_column_type_nodes++;
                }
                if (mylite_ast_create_table_column_view_options_node(column) !=
                    NULL) {
                  create_table_view_column_options_nodes++;
                }
                const MyliteAstExpression *column_expressions[] = {
                    mylite_ast_create_table_column_view_default_value_expression(
                        column),
                    mylite_ast_create_table_column_view_on_update_value_expression(
                        column),
                    mylite_ast_create_table_column_view_generated_expression(
                        column),
                    mylite_ast_create_table_column_view_check_expression(
                        column)};
                for (size_t l = 0;
                     l < sizeof(column_expressions) /
                             sizeof(column_expressions[0]);
                     l++) {
                  if (column_expressions[l] != NULL) {
                    create_table_view_column_expression_roots++;
                    count_expression_tree(
                        column_expressions[l],
                        &create_table_view_column_expression_tree_nodes,
                        &create_table_view_column_expression_tree_operators,
                        &create_table_view_column_expression_tree_leaf_values);
                  }
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
                if (mylite_ast_create_table_key_view_index_type_kind(key) !=
                    MYLITE_CREATE_TABLE_INDEX_TYPE_UNSPECIFIED) {
                  create_table_view_key_index_types++;
                }
                if (mylite_ast_create_table_key_view_visibility(key) !=
                    MYLITE_CREATE_TABLE_KEY_VISIBILITY_UNSPECIFIED) {
                  create_table_view_key_visibilities++;
                }
                if (mylite_ast_create_table_key_view_comment_value(key) !=
                    NULL) {
                  create_table_view_key_comments++;
                }
                if (mylite_ast_create_table_key_view_parser_value(key) !=
                    NULL) {
                  create_table_view_key_parsers++;
                }
                if (mylite_ast_create_table_key_view_has_key_block_size_value(
                        key)) {
                  create_table_view_key_block_sizes++;
                }
                const MyliteAstExpression *key_check_expression =
                    mylite_ast_create_table_key_view_check_expression(key);
                if (key_check_expression != NULL) {
                  create_table_view_key_expression_roots++;
                  count_expression_tree(
                      key_check_expression,
                      &create_table_view_key_expression_tree_nodes,
                      &create_table_view_key_expression_tree_operators,
                      &create_table_view_key_expression_tree_leaf_values);
                }
                for (size_t l = 0;
                     l < mylite_ast_create_table_key_view_column_count(key);
                     l++) {
                  const MyliteAstCreateTableKeyPart *part =
                      mylite_ast_create_table_key_view_column_at(key, l);
                  if (part != NULL) {
                    create_table_view_key_column_handles++;
                  }
                  if (mylite_ast_create_table_key_part_view_name_value(part) !=
                      NULL) {
                    create_table_view_named_key_columns++;
                  }
                  if (mylite_ast_create_table_key_part_view_order(part) !=
                      MYLITE_CREATE_TABLE_KEY_PART_ORDER_UNSPECIFIED) {
                    create_table_view_ordered_key_columns++;
                  }
                  if (mylite_ast_create_table_key_part_view_prefix_value_start(
                          part) !=
                      mylite_ast_create_table_key_part_view_prefix_value_end(
                          part)) {
                    create_table_view_prefixed_key_columns++;
                  }
                  if (mylite_ast_create_table_key_part_view_expression_start(
                          part) !=
                      mylite_ast_create_table_key_part_view_expression_end(
                          part)) {
                    create_table_view_expression_key_columns++;
                  }
                  const MyliteAstExpression *part_expression =
                      mylite_ast_create_table_key_part_view_expression(part);
                  if (part_expression != NULL) {
                    create_table_view_key_expression_roots++;
                    count_expression_tree(
                        part_expression,
                        &create_table_view_key_expression_tree_nodes,
                        &create_table_view_key_expression_tree_operators,
                        &create_table_view_key_expression_tree_leaf_values);
                  }
                }
                for (size_t l = 0;
                     l <
                     mylite_ast_create_table_key_view_referenced_column_count(
                         key);
                     l++) {
                  const MyliteAstCreateTableKeyPart *part =
                      mylite_ast_create_table_key_view_referenced_column_at(key,
                                                                            l);
                  if (part != NULL) {
                    create_table_view_referenced_column_handles++;
                  }
                  if (mylite_ast_create_table_key_part_view_name_value(part) !=
                      NULL) {
                    create_table_view_named_referenced_columns++;
                  }
                }
                for (size_t l = 0;
                     l < mylite_ast_create_table_key_view_option_count(key);
                     l++) {
                  const MyliteAstCreateTableKeyOption *option =
                      mylite_ast_create_table_key_view_option_at(key, l);
                  if (option != NULL) {
                    create_table_view_key_option_handles++;
                  }
                  if (mylite_ast_create_table_key_option_view_value(option) !=
                      NULL) {
                    create_table_view_key_option_values++;
                  }
                  switch (mylite_ast_create_table_key_option_view_value_kind(
                      option)) {
                    case MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_IDENTIFIER:
                      create_table_view_key_option_identifier_values++;
                      break;
                    case MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_STRING:
                      create_table_view_key_option_string_values++;
                      break;
                    case MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_UNSIGNED_INTEGER:
                      create_table_view_key_option_unsigned_integer_values++;
                      break;
                    case MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_INDEX_TYPE:
                      create_table_view_key_option_index_type_values++;
                      break;
                    case MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_UNKNOWN:
                    case MYLITE_CREATE_TABLE_KEY_OPTION_VALUE_RAW:
                      break;
                  }
                }
              }
              for (size_t k = 0;
                   k < mylite_ast_create_table_view_option_count(create_table);
                   k++) {
                const MyliteAstCreateTableOption *option =
                    mylite_ast_create_table_view_option_at(create_table, k);
                if (option != NULL) {
                  create_table_view_option_handles++;
                }
                if (mylite_ast_create_table_option_view_value(option) != NULL) {
                  create_table_view_option_values++;
                }
                switch (mylite_ast_create_table_option_view_value_kind(option)) {
                  case MYLITE_CREATE_TABLE_OPTION_VALUE_IDENTIFIER:
                    create_table_view_option_identifier_values++;
                    break;
                  case MYLITE_CREATE_TABLE_OPTION_VALUE_STRING:
                    create_table_view_option_string_values++;
                    break;
                  case MYLITE_CREATE_TABLE_OPTION_VALUE_UNSIGNED_INTEGER:
                    create_table_view_option_unsigned_integer_values++;
                    break;
                  case MYLITE_CREATE_TABLE_OPTION_VALUE_LIST:
                    create_table_view_option_list_values++;
                    break;
                  case MYLITE_CREATE_TABLE_OPTION_VALUE_UNKNOWN:
                  case MYLITE_CREATE_TABLE_OPTION_VALUE_RAW:
                    break;
                }
              }
            }
            const MyliteAstAlterTable *alter_table =
                mylite_ast_alter_table_view(ast, i);
            if (alter_table != NULL) {
              alter_table_views++;
              if (mylite_ast_alter_table_view_schema_value(alter_table) !=
                  NULL) {
                alter_table_schema_values++;
              }
              if (mylite_ast_alter_table_view_name_value(alter_table) != NULL) {
                alter_table_name_values++;
              }
              alter_table_options +=
                  mylite_ast_alter_table_view_option_count(alter_table);
              for (size_t j = 0;
                   j < mylite_ast_alter_table_view_spec_count(alter_table);
                   j++) {
                const MyliteAstAlterTableSpec *spec =
                    mylite_ast_alter_table_view_spec_at(alter_table, j);
                alter_table_specs++;
                if (mylite_ast_alter_table_spec_view_name_value(spec) != NULL) {
                  alter_table_named_specs++;
                }
                if (mylite_ast_alter_table_spec_view_secondary_name_value(
                        spec) != NULL) {
                  alter_table_secondary_named_specs++;
                }
                if (mylite_ast_alter_table_spec_view_table_name_value(spec) !=
                    NULL) {
                  alter_table_renamed_tables++;
                }
                size_t alter_column_count =
                    mylite_ast_alter_table_spec_view_column_count(spec);
                alter_table_column_payloads += alter_column_count;
                for (size_t k = 0; k < alter_column_count; k++) {
                  const MyliteAstCreateTableColumn *column =
                      mylite_ast_alter_table_spec_view_column_at(spec, k);
                  if (mylite_ast_create_table_column_view_type_kind(column) !=
                      MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_UNKNOWN) {
                    alter_table_column_known_types++;
                  }
                }
                size_t alter_key_count =
                    mylite_ast_alter_table_spec_view_key_count(spec);
                alter_table_key_payloads += alter_key_count;
                for (size_t k = 0; k < alter_key_count; k++) {
                  const MyliteAstCreateTableKey *key =
                      mylite_ast_alter_table_spec_view_key_at(spec, k);
                  alter_table_key_columns +=
                      mylite_ast_create_table_key_view_column_count(key);
                }
                if (mylite_ast_alter_table_spec_view_has_if_exists(spec)) {
                  alter_table_if_exists++;
                }
                if (mylite_ast_alter_table_spec_view_has_if_not_exists(spec)) {
                  alter_table_if_not_exists++;
                }
              }
            }
            const MyliteAstCreateDatabase *create_database =
                mylite_ast_create_database_view(ast, i);
            if (create_database != NULL) {
              create_database_views++;
              if (mylite_ast_create_database_view_name_value(
                      create_database) != NULL) {
                create_database_name_values++;
              }
              create_database_options +=
                  mylite_ast_create_database_view_option_count(create_database);
              for (size_t j = 0;
                   j <
                   mylite_ast_create_database_view_option_count(create_database);
                   j++) {
                const MyliteAstDatabaseOption *option =
                    mylite_ast_create_database_view_option_at(create_database,
                                                              j);
                if (mylite_ast_database_option_view_value(option) != NULL) {
                  create_database_option_values++;
                }
              }
              if (mylite_ast_create_database_view_charset_value(
                      create_database) != NULL) {
                create_database_charset_values++;
              }
              if (mylite_ast_create_database_view_collation_value(
                      create_database) != NULL) {
                create_database_collation_values++;
              }
              if (mylite_ast_create_database_view_encryption_value(
                      create_database) != NULL) {
                create_database_encryption_values++;
              }
              if (mylite_ast_create_database_view_has_if_not_exists(
                      create_database)) {
                create_database_if_not_exists++;
              }
              if (mylite_ast_create_database_view_uses_schema_keyword(
                      create_database)) {
                create_database_schema_keywords++;
              }
            }
            const MyliteAstCreateIndex *create_index =
                mylite_ast_create_index_view(ast, i);
            if (create_index != NULL) {
              create_index_views++;
              if (mylite_ast_create_index_view_name_value(create_index) !=
                  NULL) {
                create_index_name_values++;
              }
              if (mylite_ast_create_index_view_table_name_value(create_index) !=
                  NULL) {
                create_index_table_name_values++;
              }
              create_index_columns +=
                  mylite_ast_create_index_view_column_count(create_index);
              create_index_options +=
                  mylite_ast_create_index_view_option_count(create_index);
              if (mylite_ast_create_index_view_comment_value(create_index) !=
                  NULL) {
                create_index_comments++;
              }
              if (mylite_ast_create_index_view_has_key_block_size_value(
                      create_index)) {
                create_index_key_block_sizes++;
              }
            }
            const MyliteAstCreateView *create_view =
                mylite_ast_create_view_view(ast, i);
            if (create_view != NULL) {
              create_view_views++;
              if (mylite_ast_create_view_view_schema_value(create_view) !=
                  NULL) {
                create_view_schema_values++;
              }
              if (mylite_ast_create_view_view_name_value(create_view) != NULL) {
                create_view_name_values++;
              }
              create_view_columns +=
                  mylite_ast_create_view_view_column_count(create_view);
              for (size_t j = 0;
                   j < mylite_ast_create_view_view_column_count(create_view);
                   j++) {
                const MyliteAstViewColumn *column =
                    mylite_ast_create_view_view_column_at(create_view, j);
                if (mylite_ast_view_column_view_name_value(column) != NULL) {
                  create_view_column_values++;
                }
              }
              if (mylite_ast_create_view_view_has_or_replace(create_view)) {
                create_view_or_replace++;
              }
              if (mylite_ast_create_view_view_algorithm(create_view) !=
                  MYLITE_CREATE_VIEW_ALGORITHM_UNSPECIFIED) {
                create_view_algorithms++;
              }
              if (mylite_ast_create_view_view_sql_security(create_view) !=
                  MYLITE_VIEW_SQL_SECURITY_UNSPECIFIED) {
                create_view_sql_securities++;
              }
              if (mylite_ast_create_view_view_check_option(create_view) !=
                  MYLITE_VIEW_CHECK_OPTION_NONE) {
                create_view_check_options++;
              }
              if (mylite_ast_create_view_view_select_node(create_view) != NULL) {
                create_view_select_nodes++;
              }
            }
            const MyliteAstDropIndex *drop_index =
                mylite_ast_drop_index_view(ast, i);
            if (drop_index != NULL) {
              drop_index_views++;
              if (mylite_ast_drop_index_view_name_value(drop_index) != NULL) {
                drop_index_name_values++;
              }
              if (mylite_ast_drop_index_view_table_name_value(drop_index) !=
                  NULL) {
                drop_index_table_name_values++;
              }
              if (mylite_ast_drop_index_view_has_if_exists(drop_index)) {
                drop_index_if_exists++;
              }
            }
            const MyliteAstDropDatabase *drop_database =
                mylite_ast_drop_database_view(ast, i);
            if (drop_database != NULL) {
              drop_database_views++;
              if (mylite_ast_drop_database_view_name_value(drop_database) !=
                  NULL) {
                drop_database_name_values++;
              }
              if (mylite_ast_drop_database_view_has_if_exists(drop_database)) {
                drop_database_if_exists++;
              }
              if (mylite_ast_drop_database_view_uses_schema_keyword(
                      drop_database)) {
                drop_database_schema_keywords++;
              }
            }
            const MyliteAstDropTable *drop_table =
                mylite_ast_drop_table_view(ast, i);
            if (drop_table != NULL) {
              drop_table_views++;
              drop_table_tables +=
                  mylite_ast_drop_table_view_table_count(drop_table);
              if (mylite_ast_drop_table_view_has_if_exists(drop_table)) {
                drop_table_if_exists++;
              }
            }
            const MyliteAstDropView *drop_view =
                mylite_ast_drop_view_view(ast, i);
            if (drop_view != NULL) {
              drop_view_views++;
              drop_view_view_targets +=
                  mylite_ast_drop_view_view_view_count(drop_view);
              if (mylite_ast_drop_view_view_has_if_exists(drop_view)) {
                drop_view_if_exists++;
              }
              if (mylite_ast_drop_view_view_mode(drop_view) !=
                  MYLITE_DROP_VIEW_MODE_UNSPECIFIED) {
                drop_view_modes++;
              }
            }
            const MyliteAstInsertStatement *insert_statement =
                mylite_ast_insert_statement_view(ast, i);
            if (insert_statement != NULL) {
              insert_statement_views++;
              switch (mylite_ast_insert_statement_view_source_kind(
                  insert_statement)) {
                case MYLITE_INSERT_SOURCE_VALUES:
                  insert_statement_values_sources++;
                  break;
                case MYLITE_INSERT_SOURCE_SET:
                  insert_statement_set_sources++;
                  break;
                case MYLITE_INSERT_SOURCE_SELECT:
                  insert_statement_select_sources++;
                  break;
                case MYLITE_INSERT_SOURCE_UNKNOWN:
                  break;
              }
              if (mylite_ast_insert_statement_view_priority(insert_statement) !=
                  MYLITE_INSERT_PRIORITY_NONE) {
                insert_statement_priorities++;
              }
              if (mylite_ast_insert_statement_view_has_ignore(
                      insert_statement)) {
                insert_statement_ignores++;
              }
              if (mylite_ast_insert_statement_view_has_partition_clause(
                      insert_statement)) {
                insert_statement_partition_clauses++;
              }
              if (mylite_ast_insert_statement_view_has_on_duplicate_key_update(
                      insert_statement)) {
                insert_statement_duplicate_clauses++;
              }
              insert_statement_columns +=
                  mylite_ast_insert_statement_view_column_count(
                      insert_statement);
              for (size_t j = 0;
                   j < mylite_ast_insert_statement_view_column_count(
                           insert_statement);
                   j++) {
                const MyliteAstInsertColumn *column =
                    mylite_ast_insert_statement_view_column_at(
                        insert_statement, j);
                if (mylite_ast_insert_column_view_name_value(column) != NULL) {
                  insert_statement_column_name_values++;
                }
              }
              insert_statement_value_rows +=
                  mylite_ast_insert_statement_view_value_row_count(
                      insert_statement);
              insert_statement_values +=
                  mylite_ast_insert_statement_view_value_count(
                      insert_statement);
              for (size_t j = 0;
                   j < mylite_ast_insert_statement_view_value_count(
                           insert_statement);
                   j++) {
                const MyliteAstInsertValue *value =
                    mylite_ast_insert_statement_view_value_at(insert_statement,
                                                              j);
                if (mylite_ast_insert_value_view_is_default(value)) {
                  insert_statement_default_values++;
                }
                count_expression_tree(
                    mylite_ast_insert_value_view_expression(value),
                    &insert_statement_expression_tree_nodes,
                    &insert_statement_expression_tree_operators,
                    &insert_statement_expression_tree_leaf_values);
              }
              insert_statement_set_assignments +=
                  mylite_ast_insert_statement_view_set_assignment_count(
                      insert_statement);
              for (size_t j = 0;
                   j < mylite_ast_insert_statement_view_set_assignment_count(
                           insert_statement);
                   j++) {
                const MyliteAstInsertAssignment *assignment =
                    mylite_ast_insert_statement_view_set_assignment_at(
                        insert_statement, j);
                if (mylite_ast_insert_assignment_view_name_value(assignment) !=
                    NULL) {
                  insert_statement_assignment_name_values++;
                }
                count_expression_tree(
                    mylite_ast_insert_assignment_view_value_expression(
                        assignment),
                    &insert_statement_expression_tree_nodes,
                    &insert_statement_expression_tree_operators,
                    &insert_statement_expression_tree_leaf_values);
              }
              insert_statement_duplicate_assignments +=
                  mylite_ast_insert_statement_view_duplicate_assignment_count(
                      insert_statement);
              for (size_t j = 0;
                   j <
                   mylite_ast_insert_statement_view_duplicate_assignment_count(
                       insert_statement);
                   j++) {
                const MyliteAstInsertAssignment *assignment =
                    mylite_ast_insert_statement_view_duplicate_assignment_at(
                        insert_statement, j);
                if (mylite_ast_insert_assignment_view_name_value(assignment) !=
                    NULL) {
                  insert_statement_assignment_name_values++;
                }
                count_expression_tree(
                    mylite_ast_insert_assignment_view_value_expression(
                        assignment),
                    &insert_statement_expression_tree_nodes,
                    &insert_statement_expression_tree_operators,
                    &insert_statement_expression_tree_leaf_values);
              }
            }
            const MyliteAstReplaceStatement *replace_statement =
                mylite_ast_replace_statement_view(ast, i);
            if (replace_statement != NULL) {
              replace_statement_views++;
              switch (mylite_ast_replace_statement_view_source_kind(
                  replace_statement)) {
                case MYLITE_REPLACE_SOURCE_VALUES:
                  replace_statement_values_sources++;
                  break;
                case MYLITE_REPLACE_SOURCE_SET:
                  replace_statement_set_sources++;
                  break;
                case MYLITE_REPLACE_SOURCE_SELECT:
                  replace_statement_select_sources++;
                  break;
                case MYLITE_REPLACE_SOURCE_UNKNOWN:
                  break;
              }
              if (mylite_ast_replace_statement_view_priority(
                      replace_statement) != MYLITE_REPLACE_PRIORITY_NONE) {
                replace_statement_priorities++;
              }
              if (mylite_ast_replace_statement_view_has_into(
                      replace_statement)) {
                replace_statement_into_clauses++;
              }
              if (mylite_ast_replace_statement_view_has_partition_clause(
                      replace_statement)) {
                replace_statement_partition_clauses++;
              }
              replace_statement_columns +=
                  mylite_ast_replace_statement_view_column_count(
                      replace_statement);
              for (size_t j = 0;
                   j < mylite_ast_replace_statement_view_column_count(
                           replace_statement);
                   j++) {
                const MyliteAstReplaceColumn *column =
                    mylite_ast_replace_statement_view_column_at(
                        replace_statement, j);
                if (mylite_ast_replace_column_view_name_value(column) != NULL) {
                  replace_statement_column_name_values++;
                }
              }
              replace_statement_value_rows +=
                  mylite_ast_replace_statement_view_value_row_count(
                      replace_statement);
              replace_statement_values +=
                  mylite_ast_replace_statement_view_value_count(
                      replace_statement);
              for (size_t j = 0;
                   j < mylite_ast_replace_statement_view_value_count(
                           replace_statement);
                   j++) {
                const MyliteAstReplaceValue *value =
                    mylite_ast_replace_statement_view_value_at(
                        replace_statement, j);
                if (mylite_ast_replace_value_view_is_default(value)) {
                  replace_statement_default_values++;
                }
                count_expression_tree(
                    mylite_ast_replace_value_view_expression(value),
                    &replace_statement_expression_tree_nodes,
                    &replace_statement_expression_tree_operators,
                    &replace_statement_expression_tree_leaf_values);
              }
              replace_statement_set_assignments +=
                  mylite_ast_replace_statement_view_set_assignment_count(
                      replace_statement);
              for (size_t j = 0;
                   j < mylite_ast_replace_statement_view_set_assignment_count(
                           replace_statement);
                   j++) {
                const MyliteAstReplaceAssignment *assignment =
                    mylite_ast_replace_statement_view_set_assignment_at(
                        replace_statement, j);
                if (mylite_ast_replace_assignment_view_name_value(assignment) !=
                    NULL) {
                  replace_statement_assignment_name_values++;
                }
                count_expression_tree(
                    mylite_ast_replace_assignment_view_value_expression(
                        assignment),
                    &replace_statement_expression_tree_nodes,
                    &replace_statement_expression_tree_operators,
                    &replace_statement_expression_tree_leaf_values);
              }
            }
            const MyliteAstDeleteStatement *delete_statement =
                mylite_ast_delete_statement_view(ast, i);
            if (delete_statement != NULL) {
              delete_statement_views++;
              if (mylite_ast_delete_statement_view_has_with_clause(
                      delete_statement)) {
                delete_statement_with_clauses++;
              }
              if (mylite_ast_delete_statement_view_is_multi_table(
                      delete_statement)) {
                delete_statement_multi_table++;
              }
              MyliteDeleteStatementKind delete_kind =
                  mylite_ast_delete_statement_view_kind(delete_statement);
              if (delete_kind == MYLITE_DELETE_STATEMENT_MULTI_TABLE_FROM) {
                delete_statement_multi_table_from++;
              }
              if (delete_kind == MYLITE_DELETE_STATEMENT_MULTI_TABLE_USING) {
                delete_statement_multi_table_using++;
              }
              if (mylite_ast_delete_statement_view_priority(delete_statement) !=
                  MYLITE_DELETE_PRIORITY_NONE) {
                delete_statement_priorities++;
              }
              if (mylite_ast_delete_statement_view_has_quick(
                      delete_statement)) {
                delete_statement_quicks++;
              }
              if (mylite_ast_delete_statement_view_has_ignore(
                      delete_statement)) {
                delete_statement_ignores++;
              }
              if (mylite_ast_delete_statement_view_where_expression(
                      delete_statement) != NULL) {
                delete_statement_where_expressions++;
                count_expression_tree(
                    mylite_ast_delete_statement_view_where_expression(
                        delete_statement),
                    &delete_statement_expression_tree_nodes,
                    &delete_statement_expression_tree_operators,
                    &delete_statement_expression_tree_leaf_values);
              }
              if (mylite_ast_delete_statement_view_order_by_end(
                      delete_statement) != 0) {
                delete_statement_order_by_clauses++;
              }
              if (mylite_ast_delete_statement_view_limit_end(
                      delete_statement) != 0) {
                delete_statement_limit_clauses++;
              }
              delete_statement_targets +=
                  mylite_ast_delete_statement_view_target_count(
                      delete_statement);
              for (size_t j = 0;
                   j < mylite_ast_delete_statement_view_target_count(
                           delete_statement);
                   j++) {
                const MyliteAstDeleteTarget *target =
                    mylite_ast_delete_statement_view_target_at(
                        delete_statement, j);
                if (mylite_ast_delete_target_view_schema_value(target) !=
                    NULL) {
                  delete_statement_target_schema_values++;
                }
                if (mylite_ast_delete_target_view_name_value(target) != NULL) {
                  delete_statement_target_name_values++;
                }
                if (mylite_ast_delete_target_view_has_wildcard(target)) {
                  delete_statement_target_wildcards++;
                }
              }
            }
            const MyliteAstUpdateStatement *update_statement =
                mylite_ast_update_statement_view(ast, i);
            if (update_statement != NULL) {
              update_statement_views++;
              if (mylite_ast_update_statement_view_has_with_clause(
                      update_statement)) {
                update_statement_with_clauses++;
              }
              if (mylite_ast_update_statement_view_is_multi_table(
                      update_statement)) {
                update_statement_multi_table++;
              }
              if (mylite_ast_update_statement_view_priority(update_statement) !=
                  MYLITE_UPDATE_PRIORITY_NONE) {
                update_statement_priorities++;
              }
              if (mylite_ast_update_statement_view_has_ignore(
                      update_statement)) {
                update_statement_ignores++;
              }
              if (mylite_ast_update_statement_view_where_expression(
                      update_statement) != NULL) {
                update_statement_where_expressions++;
                count_expression_tree(
                    mylite_ast_update_statement_view_where_expression(
                        update_statement),
                    &update_statement_expression_tree_nodes,
                    &update_statement_expression_tree_operators,
                    &update_statement_expression_tree_leaf_values);
              }
              if (mylite_ast_update_statement_view_order_by_end(
                      update_statement) != 0) {
                update_statement_order_by_clauses++;
              }
              if (mylite_ast_update_statement_view_limit_end(
                      update_statement) != 0) {
                update_statement_limit_clauses++;
              }
              update_statement_assignments +=
                  mylite_ast_update_statement_view_assignment_count(
                      update_statement);
              for (size_t j = 0;
                   j < mylite_ast_update_statement_view_assignment_count(
                           update_statement);
                   j++) {
                const MyliteAstUpdateAssignment *assignment =
                    mylite_ast_update_statement_view_assignment_at(
                        update_statement, j);
                if (mylite_ast_update_assignment_view_name_value(assignment) !=
                    NULL) {
                  update_statement_assignment_name_values++;
                }
                count_expression_tree(
                    mylite_ast_update_assignment_view_value_expression(
                        assignment),
                    &update_statement_expression_tree_nodes,
                    &update_statement_expression_tree_operators,
                    &update_statement_expression_tree_leaf_values);
              }
            }
            const MyliteAstPrepareStatement *prepare_statement =
                mylite_ast_prepare_statement_view(ast, i);
            if (prepare_statement != NULL) {
              prepare_statement_views++;
              if (mylite_ast_prepare_statement_view_name_value(
                      prepare_statement) != NULL) {
                prepare_statement_name_values++;
              }
              if (mylite_ast_prepare_statement_view_source_value(
                      prepare_statement) != NULL) {
                prepare_statement_source_values++;
              }
              switch (mylite_ast_prepare_statement_view_source_kind(
                  prepare_statement)) {
                case MYLITE_PREPARE_STATEMENT_SOURCE_STRING:
                  prepare_statement_string_sources++;
                  break;
                case MYLITE_PREPARE_STATEMENT_SOURCE_USER_VARIABLE:
                  prepare_statement_user_variable_sources++;
                  break;
                case MYLITE_PREPARE_STATEMENT_SOURCE_UNKNOWN:
                  break;
              }
            }
            const MyliteAstExecuteStatement *execute_statement =
                mylite_ast_execute_statement_view(ast, i);
            if (execute_statement != NULL) {
              execute_statement_views++;
              if (mylite_ast_execute_statement_view_name_value(
                      execute_statement) != NULL) {
                execute_statement_name_values++;
              }
              size_t using_count =
                  mylite_ast_execute_statement_view_using_count(
                      execute_statement);
              execute_statement_using_variables += using_count;
              for (size_t j = 0; j < using_count; j++) {
                const MyliteAstPreparedStatementVariable *variable =
                    mylite_ast_execute_statement_view_using_variable_at(
                        execute_statement, j);
                if (mylite_ast_prepared_statement_variable_view_name_value(
                        variable) != NULL) {
                  execute_statement_using_variable_name_values++;
                }
              }
            }
            const MyliteAstDeallocateStatement *deallocate_statement =
                mylite_ast_deallocate_statement_view(ast, i);
            if (deallocate_statement != NULL) {
              deallocate_statement_views++;
              if (mylite_ast_deallocate_statement_view_name_value(
                      deallocate_statement) != NULL) {
                deallocate_statement_name_values++;
              }
              if (mylite_ast_deallocate_statement_view_mode(
                      deallocate_statement) !=
                  MYLITE_DEALLOCATE_STATEMENT_MODE_UNKNOWN) {
                deallocate_statement_modes++;
              }
            }
            const MyliteAstSetStatement *set_statement =
                mylite_ast_set_statement_view(ast, i);
            if (set_statement != NULL) {
              set_statement_views++;
              set_statement_assignments +=
                  mylite_ast_set_statement_view_assignment_count(
                      set_statement);
              for (size_t j = 0;
                   j < mylite_ast_set_statement_view_assignment_count(
                           set_statement);
                   j++) {
                const MyliteAstSetAssignment *assignment =
                    mylite_ast_set_statement_view_assignment_at(set_statement, j);
                if (mylite_ast_set_assignment_view_name_value(assignment) !=
                    NULL) {
                  set_assignment_name_values++;
                }
                if (mylite_ast_set_assignment_view_scope(assignment) !=
                    MYLITE_SET_VARIABLE_SCOPE_UNSPECIFIED) {
                  set_assignment_scopes++;
                }
                if (mylite_ast_set_assignment_view_value_node(assignment) !=
                    NULL) {
                  set_assignment_value_nodes++;
                }
                const MyliteAstExpression *expression =
                    mylite_ast_set_assignment_view_value_expression(assignment);
                if (expression != NULL) {
                  set_assignment_value_expressions++;
                  if (mylite_ast_expression_view_value(expression) != NULL) {
                    set_assignment_expression_values++;
                  }
                  if (mylite_ast_expression_view_has_unsigned_integer(
                          expression)) {
                    set_assignment_expression_unsigned_integers++;
                  }
                  count_expression_tree(
                      expression, &set_assignment_expression_tree_nodes,
                      &set_assignment_expression_tree_operators,
                      &set_assignment_expression_tree_leaf_values);
                  switch (mylite_ast_expression_view_kind(expression)) {
                    case MYLITE_EXPRESSION_LITERAL:
                      set_assignment_expression_literals++;
                      break;
                    case MYLITE_EXPRESSION_FUNCTION_CALL:
                      set_assignment_expression_function_calls++;
                      break;
                    case MYLITE_EXPRESSION_DEFAULT:
                      set_assignment_expression_defaults++;
                      break;
                    case MYLITE_EXPRESSION_UNKNOWN:
                    case MYLITE_EXPRESSION_RAW:
                    case MYLITE_EXPRESSION_IDENTIFIER:
                    case MYLITE_EXPRESSION_VARIABLE:
                    case MYLITE_EXPRESSION_PARAMETER:
                    case MYLITE_EXPRESSION_UNARY:
                    case MYLITE_EXPRESSION_BINARY:
                    case MYLITE_EXPRESSION_PARENTHESIZED:
                      break;
                  }
                }
                if (mylite_ast_set_assignment_view_extend_value_node(
                        assignment) != NULL) {
                  set_assignment_extend_value_nodes++;
                }
                switch (mylite_ast_set_assignment_view_kind(assignment)) {
                  case MYLITE_SET_ASSIGNMENT_SYSTEM_VARIABLE:
                    set_assignment_system_variables++;
                    break;
                  case MYLITE_SET_ASSIGNMENT_USER_VARIABLE:
                    set_assignment_user_variables++;
                    break;
                  case MYLITE_SET_ASSIGNMENT_NAMES:
                    set_assignment_names++;
                    break;
                  case MYLITE_SET_ASSIGNMENT_CHARACTER_SET:
                    set_assignment_character_sets++;
                    break;
                  case MYLITE_SET_ASSIGNMENT_TRANSACTION_CHARACTERISTIC:
                    set_assignment_transaction_characteristics++;
                    break;
                  case MYLITE_SET_ASSIGNMENT_CONFIG:
                    set_assignment_configs++;
                    break;
                  case MYLITE_SET_ASSIGNMENT_UNKNOWN:
                    break;
                }
              }
            }
            const MyliteAstRenameTable *rename_table =
                mylite_ast_rename_table_view(ast, i);
            if (rename_table != NULL) {
              rename_table_views++;
              rename_table_pairs +=
                  mylite_ast_rename_table_view_pair_count(rename_table);
            }
            const MyliteAstTruncateTable *truncate_table =
                mylite_ast_truncate_table_view(ast, i);
            if (truncate_table != NULL) {
              truncate_table_views++;
              if (mylite_ast_truncate_table_view_name_value(truncate_table) !=
                  NULL) {
                truncate_table_name_values++;
              }
              if (mylite_ast_truncate_table_view_has_table_keyword(
                      truncate_table)) {
                truncate_table_table_keywords++;
              }
            }
            const MyliteAstTransactionStatement *transaction_statement =
                mylite_ast_transaction_statement_view(ast, i);
            if (transaction_statement != NULL) {
              transaction_statement_views++;
              switch (mylite_ast_transaction_statement_view_kind(
                  transaction_statement)) {
                case MYLITE_TRANSACTION_STATEMENT_BEGIN:
                  transaction_statement_begins++;
                  break;
                case MYLITE_TRANSACTION_STATEMENT_COMMIT:
                  transaction_statement_commits++;
                  break;
                case MYLITE_TRANSACTION_STATEMENT_ROLLBACK:
                  transaction_statement_rollbacks++;
                  break;
                case MYLITE_TRANSACTION_STATEMENT_SAVEPOINT:
                  transaction_statement_savepoints++;
                  break;
                case MYLITE_TRANSACTION_STATEMENT_RELEASE_SAVEPOINT:
                  transaction_statement_release_savepoints++;
                  break;
                case MYLITE_TRANSACTION_STATEMENT_UNKNOWN:
                  break;
              }
              if (mylite_ast_transaction_statement_view_has_work_keyword(
                      transaction_statement)) {
                transaction_statement_work_keywords++;
              }
              if (mylite_ast_transaction_statement_view_access_mode(
                      transaction_statement) !=
                  MYLITE_TRANSACTION_ACCESS_UNSPECIFIED) {
                transaction_statement_access_modes++;
              }
              if (mylite_ast_transaction_statement_view_has_consistent_snapshot(
                      transaction_statement)) {
                transaction_statement_consistent_snapshots++;
              }
              if (mylite_ast_transaction_statement_view_has_chain(
                      transaction_statement) ||
                  mylite_ast_transaction_statement_view_has_no_chain(
                      transaction_statement) ||
                  mylite_ast_transaction_statement_view_has_release(
                      transaction_statement) ||
                  mylite_ast_transaction_statement_view_has_no_release(
                      transaction_statement)) {
                transaction_statement_completion_modifiers++;
              }
              if (mylite_ast_transaction_statement_view_savepoint_name_value(
                      transaction_statement) != NULL) {
                transaction_statement_savepoint_names++;
              }
            }
            const MyliteAstUseDatabase *use_database =
                mylite_ast_use_database_view(ast, i);
            if (use_database != NULL) {
              use_database_views++;
              if (mylite_ast_use_database_view_name_value(use_database) !=
                  NULL) {
                use_database_name_values++;
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
           "avg_select_statement_views=%.2f "
           "avg_select_statement_query_blocks=%.2f "
           "avg_select_statement_projections=%.2f "
           "avg_select_statement_projection_expressions=%.2f "
           "avg_select_statement_projection_alias_values=%.2f "
           "avg_select_statement_projection_wildcards=%.2f "
           "avg_select_statement_projection_table_wildcards=%.2f "
           "avg_select_statement_where_expressions=%.2f "
           "avg_select_statement_having_expressions=%.2f "
           "avg_select_statement_from_clauses=%.2f "
           "avg_select_statement_group_by_clauses=%.2f "
           "avg_select_statement_order_by_clauses=%.2f "
           "avg_select_statement_limit_clauses=%.2f "
           "avg_select_statement_into_clauses=%.2f "
           "avg_select_statement_lock_clauses=%.2f "
           "avg_select_statement_with_clauses=%.2f "
           "avg_select_statement_set_operations=%.2f "
           "avg_select_statement_expression_tree_nodes=%.2f "
           "avg_select_statement_expression_tree_operators=%.2f "
           "avg_select_statement_expression_tree_leaf_values=%.2f "
           "avg_insert_statement_views=%.2f "
           "avg_insert_statement_values_sources=%.2f "
           "avg_insert_statement_set_sources=%.2f "
           "avg_insert_statement_select_sources=%.2f "
           "avg_insert_statement_priorities=%.2f "
           "avg_insert_statement_ignores=%.2f "
           "avg_insert_statement_partition_clauses=%.2f "
           "avg_insert_statement_duplicate_clauses=%.2f "
           "avg_insert_statement_columns=%.2f "
           "avg_insert_statement_column_name_values=%.2f "
           "avg_insert_statement_value_rows=%.2f "
           "avg_insert_statement_values=%.2f "
           "avg_insert_statement_default_values=%.2f "
           "avg_insert_statement_set_assignments=%.2f "
           "avg_insert_statement_duplicate_assignments=%.2f "
           "avg_insert_statement_assignment_name_values=%.2f "
           "avg_insert_statement_expression_tree_nodes=%.2f "
           "avg_insert_statement_expression_tree_operators=%.2f "
           "avg_insert_statement_expression_tree_leaf_values=%.2f "
           "avg_replace_statement_views=%.2f "
           "avg_replace_statement_values_sources=%.2f "
           "avg_replace_statement_set_sources=%.2f "
           "avg_replace_statement_select_sources=%.2f "
           "avg_replace_statement_priorities=%.2f "
           "avg_replace_statement_into_clauses=%.2f "
           "avg_replace_statement_partition_clauses=%.2f "
           "avg_replace_statement_columns=%.2f "
           "avg_replace_statement_column_name_values=%.2f "
           "avg_replace_statement_value_rows=%.2f "
           "avg_replace_statement_values=%.2f "
           "avg_replace_statement_default_values=%.2f "
           "avg_replace_statement_set_assignments=%.2f "
           "avg_replace_statement_assignment_name_values=%.2f "
           "avg_replace_statement_expression_tree_nodes=%.2f "
           "avg_replace_statement_expression_tree_operators=%.2f "
           "avg_replace_statement_expression_tree_leaf_values=%.2f "
           "avg_delete_statement_views=%.2f "
           "avg_delete_statement_with_clauses=%.2f "
           "avg_delete_statement_multi_table=%.2f "
           "avg_delete_statement_multi_table_from=%.2f "
           "avg_delete_statement_multi_table_using=%.2f "
           "avg_delete_statement_priorities=%.2f "
           "avg_delete_statement_quicks=%.2f "
           "avg_delete_statement_ignores=%.2f "
           "avg_delete_statement_targets=%.2f "
           "avg_delete_statement_target_schema_values=%.2f "
           "avg_delete_statement_target_name_values=%.2f "
           "avg_delete_statement_target_wildcards=%.2f "
           "avg_delete_statement_where_expressions=%.2f "
           "avg_delete_statement_order_by_clauses=%.2f "
           "avg_delete_statement_limit_clauses=%.2f "
           "avg_delete_statement_expression_tree_nodes=%.2f "
           "avg_delete_statement_expression_tree_operators=%.2f "
           "avg_delete_statement_expression_tree_leaf_values=%.2f "
           "avg_update_statement_views=%.2f "
           "avg_update_statement_with_clauses=%.2f "
           "avg_update_statement_multi_table=%.2f "
           "avg_update_statement_priorities=%.2f "
           "avg_update_statement_ignores=%.2f "
           "avg_update_statement_assignments=%.2f "
           "avg_update_statement_assignment_name_values=%.2f "
           "avg_update_statement_where_expressions=%.2f "
           "avg_update_statement_order_by_clauses=%.2f "
           "avg_update_statement_limit_clauses=%.2f "
           "avg_update_statement_expression_tree_nodes=%.2f "
           "avg_update_statement_expression_tree_operators=%.2f "
           "avg_update_statement_expression_tree_leaf_values=%.2f "
           "avg_create_table_views=%.2f "
           "avg_create_table_view_schema_values=%.2f "
           "avg_create_table_view_name_values=%.2f "
           "avg_create_table_view_summary_engines=%.2f "
           "avg_create_table_view_summary_comments=%.2f "
           "avg_create_table_view_summary_auto_increments=%.2f "
           "avg_create_table_view_columns=%.2f "
           "avg_create_table_view_keys=%.2f "
           "avg_create_table_view_options=%.2f "
           "avg_create_table_view_column_handles=%.2f "
           "avg_create_table_view_known_column_types=%.2f "
           "avg_create_table_view_column_type_numeric_params=%.2f "
           "avg_create_table_view_column_type_element_handles=%.2f "
           "avg_create_table_view_column_type_element_values=%.2f "
           "avg_create_table_view_column_type_lengths=%.2f "
           "avg_create_table_view_column_type_unsigned_attrs=%.2f "
           "avg_create_table_view_column_type_charset_values=%.2f "
           "avg_create_table_view_column_type_collation_values=%.2f "
           "avg_create_table_view_column_option_spans=%.2f "
           "avg_create_table_view_column_defaults=%.2f "
           "avg_create_table_view_column_default_values=%.2f "
           "avg_create_table_view_column_default_unsigned_values=%.2f "
           "avg_create_table_view_column_on_update_values=%.2f "
           "avg_create_table_view_column_comments=%.2f "
           "avg_create_table_view_column_comment_values=%.2f "
           "avg_create_table_view_column_checks=%.2f "
           "avg_create_table_view_column_nullabilities=%.2f "
           "avg_create_table_view_column_generated_storage_kinds=%.2f "
           "avg_create_table_view_column_type_nodes=%.2f "
           "avg_create_table_view_column_options_nodes=%.2f "
           "avg_create_table_view_column_expression_roots=%.2f "
           "avg_create_table_view_column_expression_tree_nodes=%.2f "
           "avg_create_table_view_column_expression_tree_operators=%.2f "
           "avg_create_table_view_column_expression_tree_leaf_values=%.2f "
           "avg_create_table_view_key_handles=%.2f "
           "avg_create_table_view_named_keys=%.2f "
           "avg_create_table_view_key_index_types=%.2f "
           "avg_create_table_view_key_visibilities=%.2f "
           "avg_create_table_view_key_comments=%.2f "
           "avg_create_table_view_key_parsers=%.2f "
           "avg_create_table_view_key_block_sizes=%.2f "
           "avg_create_table_view_key_column_handles=%.2f "
           "avg_create_table_view_named_key_columns=%.2f "
           "avg_create_table_view_ordered_key_columns=%.2f "
           "avg_create_table_view_prefixed_key_columns=%.2f "
           "avg_create_table_view_expression_key_columns=%.2f "
           "avg_create_table_view_key_expression_roots=%.2f "
           "avg_create_table_view_key_expression_tree_nodes=%.2f "
           "avg_create_table_view_key_expression_tree_operators=%.2f "
           "avg_create_table_view_key_expression_tree_leaf_values=%.2f "
           "avg_create_table_view_referenced_column_handles=%.2f "
           "avg_create_table_view_named_referenced_columns=%.2f "
           "avg_create_table_view_key_option_handles=%.2f "
           "avg_create_table_view_key_option_values=%.2f "
           "avg_create_table_view_key_option_identifier_values=%.2f "
           "avg_create_table_view_key_option_string_values=%.2f "
           "avg_create_table_view_key_option_unsigned_integer_values=%.2f "
           "avg_create_table_view_key_option_index_type_values=%.2f "
           "avg_create_table_view_option_handles=%.2f "
           "avg_create_table_view_option_values=%.2f "
           "avg_create_table_view_option_identifier_values=%.2f "
           "avg_create_table_view_option_string_values=%.2f "
           "avg_create_table_view_option_unsigned_integer_values=%.2f "
           "avg_create_table_view_option_list_values=%.2f "
           "avg_alter_table_views=%.2f "
           "avg_alter_table_schema_values=%.2f "
           "avg_alter_table_name_values=%.2f "
           "avg_alter_table_specs=%.2f "
           "avg_alter_table_named_specs=%.2f "
           "avg_alter_table_secondary_named_specs=%.2f "
           "avg_alter_table_renamed_tables=%.2f "
           "avg_alter_table_column_payloads=%.2f "
           "avg_alter_table_column_known_types=%.2f "
           "avg_alter_table_key_payloads=%.2f "
           "avg_alter_table_key_columns=%.2f "
           "avg_alter_table_options=%.2f "
           "avg_alter_table_if_exists=%.2f "
           "avg_alter_table_if_not_exists=%.2f "
           "avg_create_database_views=%.2f "
           "avg_create_database_name_values=%.2f "
           "avg_create_database_options=%.2f "
           "avg_create_database_option_values=%.2f "
           "avg_create_database_charset_values=%.2f "
           "avg_create_database_collation_values=%.2f "
           "avg_create_database_encryption_values=%.2f "
           "avg_create_database_if_not_exists=%.2f "
           "avg_create_database_schema_keywords=%.2f "
           "avg_create_index_views=%.2f "
           "avg_create_index_name_values=%.2f "
           "avg_create_index_table_name_values=%.2f "
           "avg_create_index_columns=%.2f avg_create_index_options=%.2f "
           "avg_create_index_comments=%.2f "
           "avg_create_index_key_block_sizes=%.2f "
           "avg_create_view_views=%.2f "
           "avg_create_view_schema_values=%.2f "
           "avg_create_view_name_values=%.2f "
           "avg_create_view_columns=%.2f "
           "avg_create_view_column_values=%.2f "
           "avg_create_view_or_replace=%.2f "
           "avg_create_view_algorithms=%.2f "
           "avg_create_view_sql_securities=%.2f "
           "avg_create_view_check_options=%.2f "
           "avg_create_view_select_nodes=%.2f "
           "avg_drop_database_views=%.2f "
           "avg_drop_database_name_values=%.2f "
           "avg_drop_database_if_exists=%.2f "
           "avg_drop_database_schema_keywords=%.2f "
           "avg_drop_index_views=%.2f "
           "avg_drop_index_name_values=%.2f "
           "avg_drop_index_table_name_values=%.2f "
           "avg_drop_index_if_exists=%.2f "
           "avg_drop_table_views=%.2f avg_drop_table_tables=%.2f "
           "avg_drop_table_if_exists=%.2f "
           "avg_drop_view_views=%.2f avg_drop_view_view_targets=%.2f "
           "avg_drop_view_if_exists=%.2f avg_drop_view_modes=%.2f "
           "avg_prepare_statement_views=%.2f "
           "avg_prepare_statement_name_values=%.2f "
           "avg_prepare_statement_string_sources=%.2f "
           "avg_prepare_statement_user_variable_sources=%.2f "
           "avg_prepare_statement_source_values=%.2f "
           "avg_execute_statement_views=%.2f "
           "avg_execute_statement_name_values=%.2f "
           "avg_execute_statement_using_variables=%.2f "
           "avg_execute_statement_using_variable_name_values=%.2f "
           "avg_deallocate_statement_views=%.2f "
           "avg_deallocate_statement_name_values=%.2f "
           "avg_deallocate_statement_modes=%.2f "
           "avg_set_statement_views=%.2f "
           "avg_set_statement_assignments=%.2f "
           "avg_set_assignment_name_values=%.2f "
           "avg_set_assignment_scopes=%.2f "
           "avg_set_assignment_value_nodes=%.2f "
           "avg_set_assignment_value_expressions=%.2f "
           "avg_set_assignment_expression_values=%.2f "
           "avg_set_assignment_expression_unsigned_integers=%.2f "
           "avg_set_assignment_expression_literals=%.2f "
           "avg_set_assignment_expression_function_calls=%.2f "
           "avg_set_assignment_expression_defaults=%.2f "
           "avg_set_assignment_expression_tree_nodes=%.2f "
           "avg_set_assignment_expression_tree_operators=%.2f "
           "avg_set_assignment_expression_tree_leaf_values=%.2f "
           "avg_set_assignment_extend_value_nodes=%.2f "
           "avg_set_assignment_system_variables=%.2f "
           "avg_set_assignment_user_variables=%.2f "
           "avg_set_assignment_names=%.2f "
           "avg_set_assignment_character_sets=%.2f "
           "avg_set_assignment_transaction_characteristics=%.2f "
           "avg_set_assignment_configs=%.2f "
           "avg_rename_table_views=%.2f avg_rename_table_pairs=%.2f "
           "avg_truncate_table_views=%.2f "
           "avg_truncate_table_name_values=%.2f "
           "avg_truncate_table_table_keywords=%.2f "
           "avg_transaction_statement_views=%.2f "
           "avg_transaction_statement_begins=%.2f "
           "avg_transaction_statement_commits=%.2f "
           "avg_transaction_statement_rollbacks=%.2f "
           "avg_transaction_statement_savepoints=%.2f "
           "avg_transaction_statement_release_savepoints=%.2f "
           "avg_transaction_statement_work_keywords=%.2f "
           "avg_transaction_statement_access_modes=%.2f "
           "avg_transaction_statement_consistent_snapshots=%.2f "
           "avg_transaction_statement_completion_modifiers=%.2f "
           "avg_transaction_statement_savepoint_names=%.2f "
           "avg_use_database_views=%.2f "
           "avg_use_database_name_values=%.2f "
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
           (double)select_statement_views / (double)parsed,
           (double)select_statement_query_blocks / (double)parsed,
           (double)select_statement_projections / (double)parsed,
           (double)select_statement_projection_expressions / (double)parsed,
           (double)select_statement_projection_alias_values / (double)parsed,
           (double)select_statement_projection_wildcards / (double)parsed,
           (double)select_statement_projection_table_wildcards /
               (double)parsed,
           (double)select_statement_where_expressions / (double)parsed,
           (double)select_statement_having_expressions / (double)parsed,
           (double)select_statement_from_clauses / (double)parsed,
           (double)select_statement_group_by_clauses / (double)parsed,
           (double)select_statement_order_by_clauses / (double)parsed,
           (double)select_statement_limit_clauses / (double)parsed,
           (double)select_statement_into_clauses / (double)parsed,
           (double)select_statement_lock_clauses / (double)parsed,
           (double)select_statement_with_clauses / (double)parsed,
           (double)select_statement_set_operations / (double)parsed,
           (double)select_statement_expression_tree_nodes / (double)parsed,
           (double)select_statement_expression_tree_operators /
               (double)parsed,
           (double)select_statement_expression_tree_leaf_values /
               (double)parsed,
           (double)insert_statement_views / (double)parsed,
           (double)insert_statement_values_sources / (double)parsed,
           (double)insert_statement_set_sources / (double)parsed,
           (double)insert_statement_select_sources / (double)parsed,
           (double)insert_statement_priorities / (double)parsed,
           (double)insert_statement_ignores / (double)parsed,
           (double)insert_statement_partition_clauses / (double)parsed,
           (double)insert_statement_duplicate_clauses / (double)parsed,
           (double)insert_statement_columns / (double)parsed,
           (double)insert_statement_column_name_values / (double)parsed,
           (double)insert_statement_value_rows / (double)parsed,
           (double)insert_statement_values / (double)parsed,
           (double)insert_statement_default_values / (double)parsed,
           (double)insert_statement_set_assignments / (double)parsed,
           (double)insert_statement_duplicate_assignments / (double)parsed,
           (double)insert_statement_assignment_name_values / (double)parsed,
           (double)insert_statement_expression_tree_nodes / (double)parsed,
           (double)insert_statement_expression_tree_operators /
               (double)parsed,
           (double)insert_statement_expression_tree_leaf_values /
               (double)parsed,
           (double)replace_statement_views / (double)parsed,
           (double)replace_statement_values_sources / (double)parsed,
           (double)replace_statement_set_sources / (double)parsed,
           (double)replace_statement_select_sources / (double)parsed,
           (double)replace_statement_priorities / (double)parsed,
           (double)replace_statement_into_clauses / (double)parsed,
           (double)replace_statement_partition_clauses / (double)parsed,
           (double)replace_statement_columns / (double)parsed,
           (double)replace_statement_column_name_values / (double)parsed,
           (double)replace_statement_value_rows / (double)parsed,
           (double)replace_statement_values / (double)parsed,
           (double)replace_statement_default_values / (double)parsed,
           (double)replace_statement_set_assignments / (double)parsed,
           (double)replace_statement_assignment_name_values / (double)parsed,
           (double)replace_statement_expression_tree_nodes / (double)parsed,
           (double)replace_statement_expression_tree_operators /
               (double)parsed,
           (double)replace_statement_expression_tree_leaf_values /
               (double)parsed,
           (double)delete_statement_views / (double)parsed,
           (double)delete_statement_with_clauses / (double)parsed,
           (double)delete_statement_multi_table / (double)parsed,
           (double)delete_statement_multi_table_from / (double)parsed,
           (double)delete_statement_multi_table_using / (double)parsed,
           (double)delete_statement_priorities / (double)parsed,
           (double)delete_statement_quicks / (double)parsed,
           (double)delete_statement_ignores / (double)parsed,
           (double)delete_statement_targets / (double)parsed,
           (double)delete_statement_target_schema_values / (double)parsed,
           (double)delete_statement_target_name_values / (double)parsed,
           (double)delete_statement_target_wildcards / (double)parsed,
           (double)delete_statement_where_expressions / (double)parsed,
           (double)delete_statement_order_by_clauses / (double)parsed,
           (double)delete_statement_limit_clauses / (double)parsed,
           (double)delete_statement_expression_tree_nodes / (double)parsed,
           (double)delete_statement_expression_tree_operators /
               (double)parsed,
           (double)delete_statement_expression_tree_leaf_values /
               (double)parsed,
           (double)update_statement_views / (double)parsed,
           (double)update_statement_with_clauses / (double)parsed,
           (double)update_statement_multi_table / (double)parsed,
           (double)update_statement_priorities / (double)parsed,
           (double)update_statement_ignores / (double)parsed,
           (double)update_statement_assignments / (double)parsed,
           (double)update_statement_assignment_name_values / (double)parsed,
           (double)update_statement_where_expressions / (double)parsed,
           (double)update_statement_order_by_clauses / (double)parsed,
           (double)update_statement_limit_clauses / (double)parsed,
           (double)update_statement_expression_tree_nodes / (double)parsed,
           (double)update_statement_expression_tree_operators /
               (double)parsed,
           (double)update_statement_expression_tree_leaf_values /
               (double)parsed,
           (double)create_table_views / (double)parsed,
           (double)create_table_view_schema_values / (double)parsed,
           (double)create_table_view_name_values / (double)parsed,
           (double)create_table_view_summary_engines / (double)parsed,
           (double)create_table_view_summary_comments / (double)parsed,
           (double)create_table_view_summary_auto_increments / (double)parsed,
           (double)create_table_view_columns / (double)parsed,
           (double)create_table_view_keys / (double)parsed,
           (double)create_table_view_options / (double)parsed,
           (double)create_table_view_column_handles / (double)parsed,
           (double)create_table_view_known_column_types / (double)parsed,
           (double)create_table_view_column_type_numeric_params /
               (double)parsed,
           (double)create_table_view_column_type_element_handles /
               (double)parsed,
           (double)create_table_view_column_type_element_values /
               (double)parsed,
           (double)create_table_view_column_type_lengths / (double)parsed,
           (double)create_table_view_column_type_unsigned_attrs /
               (double)parsed,
           (double)create_table_view_column_type_charset_values /
               (double)parsed,
           (double)create_table_view_column_type_collation_values /
               (double)parsed,
           (double)create_table_view_column_option_spans / (double)parsed,
           (double)create_table_view_column_defaults / (double)parsed,
           (double)create_table_view_column_default_values / (double)parsed,
           (double)create_table_view_column_default_unsigned_values /
               (double)parsed,
           (double)create_table_view_column_on_update_values /
               (double)parsed,
           (double)create_table_view_column_comments / (double)parsed,
           (double)create_table_view_column_comment_values / (double)parsed,
           (double)create_table_view_column_checks / (double)parsed,
           (double)create_table_view_column_nullabilities / (double)parsed,
           (double)create_table_view_column_generated_storage_kinds /
               (double)parsed,
           (double)create_table_view_column_type_nodes / (double)parsed,
           (double)create_table_view_column_options_nodes / (double)parsed,
           (double)create_table_view_column_expression_roots / (double)parsed,
           (double)create_table_view_column_expression_tree_nodes /
               (double)parsed,
           (double)create_table_view_column_expression_tree_operators /
               (double)parsed,
           (double)create_table_view_column_expression_tree_leaf_values /
               (double)parsed,
           (double)create_table_view_key_handles / (double)parsed,
           (double)create_table_view_named_keys / (double)parsed,
           (double)create_table_view_key_index_types / (double)parsed,
           (double)create_table_view_key_visibilities / (double)parsed,
           (double)create_table_view_key_comments / (double)parsed,
           (double)create_table_view_key_parsers / (double)parsed,
           (double)create_table_view_key_block_sizes / (double)parsed,
           (double)create_table_view_key_column_handles / (double)parsed,
           (double)create_table_view_named_key_columns / (double)parsed,
           (double)create_table_view_ordered_key_columns / (double)parsed,
           (double)create_table_view_prefixed_key_columns / (double)parsed,
           (double)create_table_view_expression_key_columns / (double)parsed,
           (double)create_table_view_key_expression_roots / (double)parsed,
           (double)create_table_view_key_expression_tree_nodes /
               (double)parsed,
           (double)create_table_view_key_expression_tree_operators /
               (double)parsed,
           (double)create_table_view_key_expression_tree_leaf_values /
               (double)parsed,
           (double)create_table_view_referenced_column_handles /
               (double)parsed,
           (double)create_table_view_named_referenced_columns / (double)parsed,
           (double)create_table_view_key_option_handles / (double)parsed,
           (double)create_table_view_key_option_values / (double)parsed,
           (double)create_table_view_key_option_identifier_values /
               (double)parsed,
           (double)create_table_view_key_option_string_values /
               (double)parsed,
           (double)create_table_view_key_option_unsigned_integer_values /
               (double)parsed,
           (double)create_table_view_key_option_index_type_values /
               (double)parsed,
           (double)create_table_view_option_handles / (double)parsed,
           (double)create_table_view_option_values / (double)parsed,
           (double)create_table_view_option_identifier_values /
               (double)parsed,
           (double)create_table_view_option_string_values / (double)parsed,
           (double)create_table_view_option_unsigned_integer_values /
               (double)parsed,
           (double)create_table_view_option_list_values / (double)parsed,
           (double)alter_table_views / (double)parsed,
           (double)alter_table_schema_values / (double)parsed,
           (double)alter_table_name_values / (double)parsed,
           (double)alter_table_specs / (double)parsed,
           (double)alter_table_named_specs / (double)parsed,
           (double)alter_table_secondary_named_specs / (double)parsed,
           (double)alter_table_renamed_tables / (double)parsed,
           (double)alter_table_column_payloads / (double)parsed,
           (double)alter_table_column_known_types / (double)parsed,
           (double)alter_table_key_payloads / (double)parsed,
           (double)alter_table_key_columns / (double)parsed,
           (double)alter_table_options / (double)parsed,
           (double)alter_table_if_exists / (double)parsed,
           (double)alter_table_if_not_exists / (double)parsed,
           (double)create_database_views / (double)parsed,
           (double)create_database_name_values / (double)parsed,
           (double)create_database_options / (double)parsed,
           (double)create_database_option_values / (double)parsed,
           (double)create_database_charset_values / (double)parsed,
           (double)create_database_collation_values / (double)parsed,
           (double)create_database_encryption_values / (double)parsed,
           (double)create_database_if_not_exists / (double)parsed,
           (double)create_database_schema_keywords / (double)parsed,
           (double)create_index_views / (double)parsed,
           (double)create_index_name_values / (double)parsed,
           (double)create_index_table_name_values / (double)parsed,
           (double)create_index_columns / (double)parsed,
           (double)create_index_options / (double)parsed,
           (double)create_index_comments / (double)parsed,
           (double)create_index_key_block_sizes / (double)parsed,
           (double)create_view_views / (double)parsed,
           (double)create_view_schema_values / (double)parsed,
           (double)create_view_name_values / (double)parsed,
           (double)create_view_columns / (double)parsed,
           (double)create_view_column_values / (double)parsed,
           (double)create_view_or_replace / (double)parsed,
           (double)create_view_algorithms / (double)parsed,
           (double)create_view_sql_securities / (double)parsed,
           (double)create_view_check_options / (double)parsed,
           (double)create_view_select_nodes / (double)parsed,
           (double)drop_database_views / (double)parsed,
           (double)drop_database_name_values / (double)parsed,
           (double)drop_database_if_exists / (double)parsed,
           (double)drop_database_schema_keywords / (double)parsed,
           (double)drop_index_views / (double)parsed,
           (double)drop_index_name_values / (double)parsed,
           (double)drop_index_table_name_values / (double)parsed,
           (double)drop_index_if_exists / (double)parsed,
           (double)drop_table_views / (double)parsed,
           (double)drop_table_tables / (double)parsed,
           (double)drop_table_if_exists / (double)parsed,
           (double)drop_view_views / (double)parsed,
           (double)drop_view_view_targets / (double)parsed,
           (double)drop_view_if_exists / (double)parsed,
           (double)drop_view_modes / (double)parsed,
           (double)prepare_statement_views / (double)parsed,
           (double)prepare_statement_name_values / (double)parsed,
           (double)prepare_statement_string_sources / (double)parsed,
           (double)prepare_statement_user_variable_sources / (double)parsed,
           (double)prepare_statement_source_values / (double)parsed,
           (double)execute_statement_views / (double)parsed,
           (double)execute_statement_name_values / (double)parsed,
           (double)execute_statement_using_variables / (double)parsed,
           (double)execute_statement_using_variable_name_values /
               (double)parsed,
           (double)deallocate_statement_views / (double)parsed,
           (double)deallocate_statement_name_values / (double)parsed,
           (double)deallocate_statement_modes / (double)parsed,
           (double)set_statement_views / (double)parsed,
           (double)set_statement_assignments / (double)parsed,
           (double)set_assignment_name_values / (double)parsed,
           (double)set_assignment_scopes / (double)parsed,
           (double)set_assignment_value_nodes / (double)parsed,
           (double)set_assignment_value_expressions / (double)parsed,
           (double)set_assignment_expression_values / (double)parsed,
           (double)set_assignment_expression_unsigned_integers /
               (double)parsed,
           (double)set_assignment_expression_literals / (double)parsed,
           (double)set_assignment_expression_function_calls / (double)parsed,
           (double)set_assignment_expression_defaults / (double)parsed,
           (double)set_assignment_expression_tree_nodes / (double)parsed,
           (double)set_assignment_expression_tree_operators / (double)parsed,
           (double)set_assignment_expression_tree_leaf_values /
               (double)parsed,
           (double)set_assignment_extend_value_nodes / (double)parsed,
           (double)set_assignment_system_variables / (double)parsed,
           (double)set_assignment_user_variables / (double)parsed,
           (double)set_assignment_names / (double)parsed,
           (double)set_assignment_character_sets / (double)parsed,
           (double)set_assignment_transaction_characteristics /
               (double)parsed,
           (double)set_assignment_configs / (double)parsed,
           (double)rename_table_views / (double)parsed,
           (double)rename_table_pairs / (double)parsed,
           (double)truncate_table_views / (double)parsed,
           (double)truncate_table_name_values / (double)parsed,
           (double)truncate_table_table_keywords / (double)parsed,
           (double)transaction_statement_views / (double)parsed,
           (double)transaction_statement_begins / (double)parsed,
           (double)transaction_statement_commits / (double)parsed,
           (double)transaction_statement_rollbacks / (double)parsed,
           (double)transaction_statement_savepoints / (double)parsed,
           (double)transaction_statement_release_savepoints / (double)parsed,
           (double)transaction_statement_work_keywords / (double)parsed,
           (double)transaction_statement_access_modes / (double)parsed,
           (double)transaction_statement_consistent_snapshots /
               (double)parsed,
           (double)transaction_statement_completion_modifiers /
               (double)parsed,
           (double)transaction_statement_savepoint_names / (double)parsed,
           (double)use_database_views / (double)parsed,
           (double)use_database_name_values / (double)parsed,
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

static void count_expression_tree(const MyliteAstExpression *expression,
                                  size_t *nodes, size_t *operators,
                                  size_t *leaf_values) {
  if (expression == NULL) {
    return;
  }
  if (nodes != NULL) {
    (*nodes)++;
  }
  if (operators != NULL &&
      mylite_ast_expression_view_operator_kind(expression) !=
          MYLITE_EXPRESSION_OPERATOR_NONE) {
    (*operators)++;
  }
  if (leaf_values != NULL &&
      mylite_ast_expression_view_child_count(expression) == 0 &&
      mylite_ast_expression_view_value(expression) != NULL) {
    (*leaf_values)++;
  }

  for (size_t i = 0; i < mylite_ast_expression_view_child_count(expression);
       i++) {
    count_expression_tree(mylite_ast_expression_view_child_at(expression, i),
                          nodes, operators, leaf_values);
  }
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
