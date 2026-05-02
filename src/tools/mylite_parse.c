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
static void dump_create_table_view_handles(
    const MyliteAstCreateTable *create_table);
static const char *node_symbol_or_none(const MyliteAstNode *node);
static void print_escaped_bytes(const char *value, size_t length);
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
    size_t target_count = mylite_ast_statement_target_count(ast, i);
    size_t column_count = mylite_ast_create_table_column_count(ast, i);
    size_t key_count = mylite_ast_create_table_key_count(ast, i);
    size_t option_count = mylite_ast_create_table_option_count(ast, i);
    const MyliteAstCreateTable *create_table =
        mylite_ast_create_table_view(ast, i);
    const MyliteAstCreateIndex *create_index =
        mylite_ast_create_index_view(ast, i);
    const MyliteAstDropTable *drop_table = mylite_ast_drop_table_view(ast, i);
    const MyliteAstRenameTable *rename_table =
        mylite_ast_rename_table_view(ast, i);
    printf("statement[%zu] kind=%s symbol=%s span=%zu..%zu targets=%zu "
           "columns=%zu keys=%zu options=%zu target=%s:%zu..%zu "
           "schema=%zu..%zu name=%zu..%zu\n",
           i,
           mylite_statement_kind_name(mylite_ast_statement_kind(ast, i)),
           mylite_ast_statement_symbol_name(ast, i),
           mylite_ast_statement_start(ast, i), mylite_ast_statement_end(ast, i),
           target_count, column_count, key_count, option_count,
           mylite_statement_target_kind_name(mylite_ast_statement_target_kind(ast, i)),
           mylite_ast_statement_target_start(ast, i),
           mylite_ast_statement_target_end(ast, i),
           mylite_ast_statement_target_schema_start(ast, i),
           mylite_ast_statement_target_schema_end(ast, i),
           mylite_ast_statement_target_name_start(ast, i),
           mylite_ast_statement_target_name_end(ast, i));
    for (size_t j = 0; j < target_count; j++) {
      printf("  target[%zu] role=%s kind=%s span=%zu..%zu schema=%zu..%zu "
             "name=%zu..%zu\n",
             j,
             mylite_statement_target_role_name(
                 mylite_ast_statement_target_role_at(ast, i, j)),
             mylite_statement_target_kind_name(
                 mylite_ast_statement_target_kind_at(ast, i, j)),
             mylite_ast_statement_target_start_at(ast, i, j),
             mylite_ast_statement_target_end_at(ast, i, j),
             mylite_ast_statement_target_schema_start_at(ast, i, j),
             mylite_ast_statement_target_schema_end_at(ast, i, j),
             mylite_ast_statement_target_name_start_at(ast, i, j),
             mylite_ast_statement_target_name_end_at(ast, i, j));
      const char *schema_value =
          mylite_ast_statement_target_schema_value_at(ast, i, j);
      size_t schema_value_length =
          mylite_ast_statement_target_schema_value_length_at(ast, i, j);
      const char *name_value =
          mylite_ast_statement_target_name_value_at(ast, i, j);
      size_t name_value_length =
          mylite_ast_statement_target_name_value_length_at(ast, i, j);
      printf("    target[%zu].schema_value len=%zu value=", j,
             schema_value_length);
      if (schema_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(schema_value, schema_value_length);
      }
      printf(" name_value len=%zu value=", name_value_length);
      if (name_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(name_value, name_value_length);
      }
      fputc('\n', stdout);
    }
    if (create_table != NULL) {
      printf("  create_table span=%zu..%zu target=%zu..%zu "
             "schema=%zu..%zu name=%zu..%zu columns=%zu keys=%zu "
             "options=%zu node=%s\n",
             mylite_ast_create_table_view_start(create_table),
             mylite_ast_create_table_view_end(create_table),
             mylite_ast_create_table_view_target_start(create_table),
             mylite_ast_create_table_view_target_end(create_table),
             mylite_ast_create_table_view_schema_start(create_table),
             mylite_ast_create_table_view_schema_end(create_table),
             mylite_ast_create_table_view_name_start(create_table),
             mylite_ast_create_table_view_name_end(create_table),
             mylite_ast_create_table_view_column_count(create_table),
             mylite_ast_create_table_view_key_count(create_table),
             mylite_ast_create_table_view_option_count(create_table),
             node_symbol_or_none(
                 mylite_ast_create_table_view_node(create_table)));
      const char *schema_value =
          mylite_ast_create_table_view_schema_value(create_table);
      size_t schema_value_length =
          mylite_ast_create_table_view_schema_value_length(create_table);
      const char *name_value =
          mylite_ast_create_table_view_name_value(create_table);
      size_t name_value_length =
          mylite_ast_create_table_view_name_value_length(create_table);
      printf("    create_table.schema_value len=%zu value=",
             schema_value_length);
      if (schema_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(schema_value, schema_value_length);
      }
      printf(" name_value len=%zu value=", name_value_length);
      if (name_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(name_value, name_value_length);
      }
      fputc('\n', stdout);
      dump_create_table_view_handles(create_table);
    }
    if (create_index != NULL) {
      printf("  create_index span=%zu..%zu kind=%s index_type=%s "
             "visibility=%s columns=%zu options=%zu name_len=%zu table=",
             mylite_ast_create_index_view_start(create_index),
             mylite_ast_create_index_view_end(create_index),
             mylite_create_table_key_kind_name(
                 mylite_ast_create_index_view_key_kind(create_index)),
             mylite_create_table_index_type_name(
                 mylite_ast_create_index_view_index_type_kind(create_index)),
             mylite_create_table_key_visibility_name(
                 mylite_ast_create_index_view_visibility(create_index)),
             mylite_ast_create_index_view_column_count(create_index),
             mylite_ast_create_index_view_option_count(create_index),
             mylite_ast_create_index_view_name_value_length(create_index));
      const char *table_schema =
          mylite_ast_create_index_view_table_schema_value(create_index);
      size_t table_schema_length =
          mylite_ast_create_index_view_table_schema_value_length(create_index);
      if (table_schema != NULL) {
        print_escaped_bytes(table_schema, table_schema_length);
        fputc('.', stdout);
      }
      const char *table_name =
          mylite_ast_create_index_view_table_name_value(create_index);
      size_t table_name_length =
          mylite_ast_create_index_view_table_name_value_length(create_index);
      if (table_name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(table_name, table_name_length);
      }
      fputs(" comment=", stdout);
      const char *comment =
          mylite_ast_create_index_view_comment_value(create_index);
      size_t comment_length =
          mylite_ast_create_index_view_comment_value_length(create_index);
      if (comment == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(comment, comment_length);
      }
      printf(" key_block_size=%d:%llu\n",
             mylite_ast_create_index_view_has_key_block_size_value(
                 create_index),
             mylite_ast_create_index_view_key_block_size_value(create_index));
    }
    if (drop_table != NULL) {
      printf("  drop_table span=%zu..%zu temporary=%d if_exists=%d tables=%zu\n",
             mylite_ast_drop_table_view_start(drop_table),
             mylite_ast_drop_table_view_end(drop_table),
             mylite_ast_drop_table_view_is_temporary(drop_table),
             mylite_ast_drop_table_view_has_if_exists(drop_table),
             mylite_ast_drop_table_view_table_count(drop_table));
    }
    if (rename_table != NULL) {
      printf("  rename_table span=%zu..%zu pairs=%zu\n",
             mylite_ast_rename_table_view_start(rename_table),
             mylite_ast_rename_table_view_end(rename_table),
             mylite_ast_rename_table_view_pair_count(rename_table));
    }
    for (size_t j = 0; j < column_count; j++) {
      const MyliteAstCreateTableColumn *column_view =
          create_table == NULL
              ? NULL
              : mylite_ast_create_table_view_column_at(create_table, j);
      printf("  column[%zu] family=%s kind=%s storage=%s flags=0x%x "
             "span=%zu..%zu "
             "name=%zu..%zu type=%zu..%zu type_name=%zu..%zu "
             "type_params=%zu..%zu type_numeric_params=%zu:%llu,%llu "
             "type_elements=%zu "
             "type_length=%d:%llu type_precision=%d:%llu "
             "type_scale=%d:%llu type_fsp=%d:%llu "
             "type_attrs=%zu..%zu type_unsigned=%zu..%zu "
             "type_zerofill=%zu..%zu type_binary=%zu..%zu "
             "type_charset=%zu..%zu type_charset_value=%zu..%zu "
             "type_collation=%zu..%zu type_collation_value=%zu..%zu "
             "options=%zu..%zu default=%zu..%zu "
             "default_value=%zu..%zu on_update=%zu..%zu "
             "on_update_value=%zu..%zu generated=%zu..%zu "
             "generated_expr=%zu..%zu generated_storage=%zu..%zu "
             "comment=%zu..%zu comment_value=%zu..%zu check=%zu..%zu "
             "check_expr=%zu..%zu check_enforced=%s:%zu..%zu "
             "reference=%zu..%zu type_node=%s options_node=%s "
             "default_node=%s default_value_node=%s generated_expr_node=%s "
             "check_expr_node=%s reference_node=%s\n",
             j,
             mylite_create_table_column_type_family_name(
                 mylite_ast_create_table_column_type_family(ast, i, j)),
             mylite_create_table_column_type_kind_name(
                 mylite_ast_create_table_column_type_kind(ast, i, j)),
             mylite_create_table_column_storage_class_name(
                 mylite_ast_create_table_column_storage_class(ast, i, j)),
             mylite_ast_create_table_column_flags(ast, i, j),
             mylite_ast_create_table_column_start(ast, i, j),
             mylite_ast_create_table_column_end(ast, i, j),
             mylite_ast_create_table_column_name_start(ast, i, j),
             mylite_ast_create_table_column_name_end(ast, i, j),
             mylite_ast_create_table_column_type_start(ast, i, j),
             mylite_ast_create_table_column_type_end(ast, i, j),
             mylite_ast_create_table_column_type_name_start(ast, i, j),
             mylite_ast_create_table_column_type_name_end(ast, i, j),
             mylite_ast_create_table_column_type_parameters_start(ast, i, j),
             mylite_ast_create_table_column_type_parameters_end(ast, i, j),
             mylite_ast_create_table_column_type_numeric_parameter_count(ast, i,
                                                                         j),
             mylite_ast_create_table_column_type_numeric_parameter_at(ast, i, j,
                                                                      0),
             mylite_ast_create_table_column_type_numeric_parameter_at(ast, i, j,
                                                                      1),
             mylite_ast_create_table_column_type_element_count(ast, i, j),
             mylite_ast_create_table_column_type_has_length(ast, i, j),
             mylite_ast_create_table_column_type_length(ast, i, j),
             mylite_ast_create_table_column_type_has_precision(ast, i, j),
             mylite_ast_create_table_column_type_precision(ast, i, j),
             mylite_ast_create_table_column_type_has_scale(ast, i, j),
             mylite_ast_create_table_column_type_scale(ast, i, j),
             mylite_ast_create_table_column_type_has_fractional_seconds_precision(
                 ast, i, j),
             mylite_ast_create_table_column_type_fractional_seconds_precision(
                 ast, i, j),
             mylite_ast_create_table_column_type_attributes_start(ast, i, j),
             mylite_ast_create_table_column_type_attributes_end(ast, i, j),
             mylite_ast_create_table_column_type_unsigned_start(ast, i, j),
             mylite_ast_create_table_column_type_unsigned_end(ast, i, j),
             mylite_ast_create_table_column_type_zerofill_start(ast, i, j),
             mylite_ast_create_table_column_type_zerofill_end(ast, i, j),
             mylite_ast_create_table_column_type_binary_start(ast, i, j),
             mylite_ast_create_table_column_type_binary_end(ast, i, j),
             mylite_ast_create_table_column_type_charset_start(ast, i, j),
             mylite_ast_create_table_column_type_charset_end(ast, i, j),
             mylite_ast_create_table_column_type_charset_value_start(ast, i, j),
             mylite_ast_create_table_column_type_charset_value_end(ast, i, j),
             mylite_ast_create_table_column_type_collation_start(ast, i, j),
             mylite_ast_create_table_column_type_collation_end(ast, i, j),
             mylite_ast_create_table_column_type_collation_value_start(ast, i, j),
             mylite_ast_create_table_column_type_collation_value_end(ast, i, j),
             mylite_ast_create_table_column_options_start(ast, i, j),
             mylite_ast_create_table_column_options_end(ast, i, j),
             mylite_ast_create_table_column_default_start(ast, i, j),
             mylite_ast_create_table_column_default_end(ast, i, j),
             mylite_ast_create_table_column_default_value_start(ast, i, j),
             mylite_ast_create_table_column_default_value_end(ast, i, j),
             mylite_ast_create_table_column_on_update_start(ast, i, j),
             mylite_ast_create_table_column_on_update_end(ast, i, j),
             mylite_ast_create_table_column_on_update_value_start(ast, i, j),
             mylite_ast_create_table_column_on_update_value_end(ast, i, j),
             mylite_ast_create_table_column_generated_start(ast, i, j),
             mylite_ast_create_table_column_generated_end(ast, i, j),
             mylite_ast_create_table_column_generated_expression_start(ast, i, j),
             mylite_ast_create_table_column_generated_expression_end(ast, i, j),
             mylite_ast_create_table_column_generated_storage_start(ast, i, j),
             mylite_ast_create_table_column_generated_storage_end(ast, i, j),
             mylite_ast_create_table_column_comment_start(ast, i, j),
             mylite_ast_create_table_column_comment_end(ast, i, j),
             mylite_ast_create_table_column_comment_value_start(ast, i, j),
             mylite_ast_create_table_column_comment_value_end(ast, i, j),
             mylite_ast_create_table_column_check_start(ast, i, j),
             mylite_ast_create_table_column_check_end(ast, i, j),
             mylite_ast_create_table_column_check_expression_start(ast, i, j),
             mylite_ast_create_table_column_check_expression_end(ast, i, j),
             mylite_create_table_check_enforcement_name(
                 mylite_ast_create_table_column_check_enforcement(ast, i, j)),
             mylite_ast_create_table_column_check_enforcement_start(ast, i, j),
             mylite_ast_create_table_column_check_enforcement_end(ast, i, j),
             mylite_ast_create_table_column_reference_start(ast, i, j),
             mylite_ast_create_table_column_reference_end(ast, i, j),
             node_symbol_or_none(
                 mylite_ast_create_table_column_type_node(ast, i, j)),
             node_symbol_or_none(
                 mylite_ast_create_table_column_options_node(ast, i, j)),
             node_symbol_or_none(
                 mylite_ast_create_table_column_default_node(ast, i, j)),
             node_symbol_or_none(
                 mylite_ast_create_table_column_default_value_node(ast, i, j)),
             node_symbol_or_none(
                 mylite_ast_create_table_column_generated_expression_node(ast, i, j)),
             node_symbol_or_none(
                 mylite_ast_create_table_column_check_expression_node(ast, i, j)),
             node_symbol_or_none(
                 mylite_ast_create_table_column_reference_node(ast, i, j)));
      const char *column_name_value =
          mylite_ast_create_table_column_name_value(ast, i, j);
      size_t column_name_value_length =
          mylite_ast_create_table_column_name_value_length(ast, i, j);
      printf("    column[%zu].name_value len=%zu value=", j,
             column_name_value_length);
      if (column_name_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(column_name_value, column_name_value_length);
      }
      fputc('\n', stdout);
      printf("    column[%zu].summary nullability=%s generated_storage=%s "
             "default_kind=%s default_unsigned=%d:%llu on_update_kind=%s\n",
             j,
             mylite_create_table_column_nullability_name(
                 mylite_ast_create_table_column_view_nullability(column_view)),
             mylite_create_table_column_generated_storage_name(
                 mylite_ast_create_table_column_view_generated_storage_kind(
                     column_view)),
             mylite_create_table_column_value_kind_name(
                 mylite_ast_create_table_column_view_default_value_kind(
                     column_view)),
             mylite_ast_create_table_column_view_has_default_unsigned_integer(
                 column_view),
             mylite_ast_create_table_column_view_default_unsigned_integer_value(
                 column_view),
             mylite_create_table_column_value_kind_name(
                 mylite_ast_create_table_column_view_on_update_value_kind(
                     column_view)));
      printf("    column[%zu].values charset=", j);
      const char *charset_value =
          mylite_ast_create_table_column_view_type_charset_value(column_view);
      size_t charset_value_length =
          mylite_ast_create_table_column_view_type_charset_value_length(
              column_view);
      if (charset_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(charset_value, charset_value_length);
      }
      fputs(" collation=", stdout);
      const char *collation_value =
          mylite_ast_create_table_column_view_type_collation_value(column_view);
      size_t collation_value_length =
          mylite_ast_create_table_column_view_type_collation_value_length(
              column_view);
      if (collation_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(collation_value, collation_value_length);
      }
      fputs(" default=", stdout);
      const char *default_value =
          mylite_ast_create_table_column_view_default_value(column_view);
      size_t default_value_length =
          mylite_ast_create_table_column_view_default_value_length(column_view);
      if (default_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(default_value, default_value_length);
      }
      fputs(" on_update=", stdout);
      const char *on_update_value =
          mylite_ast_create_table_column_view_on_update_value(column_view);
      size_t on_update_value_length =
          mylite_ast_create_table_column_view_on_update_value_length(
              column_view);
      if (on_update_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(on_update_value, on_update_value_length);
      }
      fputs(" comment=", stdout);
      const char *comment_value =
          mylite_ast_create_table_column_view_comment_value(column_view);
      size_t comment_value_length =
          mylite_ast_create_table_column_view_comment_value_length(column_view);
      if (comment_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(comment_value, comment_value_length);
      }
      fputc('\n', stdout);
      size_t type_element_count =
          mylite_ast_create_table_column_type_element_count(ast, i, j);
      for (size_t k = 0; k < type_element_count; k++) {
        const char *value =
            mylite_ast_create_table_column_type_element_value(ast, i, j, k);
        size_t value_length =
            mylite_ast_create_table_column_type_element_value_length(ast, i, j,
                                                                     k);
        printf("    column[%zu].type_element[%zu] span=%zu..%zu "
               "value_len=%zu value=",
               j, k,
               mylite_ast_create_table_column_type_element_start(ast, i, j, k),
               mylite_ast_create_table_column_type_element_end(ast, i, j, k),
               value_length);
        if (value == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(value, value_length);
        }
        fputc('\n', stdout);
      }
    }
    for (size_t j = 0; j < key_count; j++) {
      const MyliteAstCreateTableKey *key_view =
          create_table == NULL ? NULL : mylite_ast_create_table_view_key_at(
                                           create_table, j);
      size_t column_count_for_key =
          mylite_ast_create_table_key_column_count(ast, i, j);
      size_t referenced_column_count =
          mylite_ast_create_table_key_referenced_column_count(ast, i, j);
      size_t key_option_count =
          mylite_ast_create_table_key_option_count(ast, i, j);
      printf("  key[%zu] kind=%s span=%zu..%zu constraint=%zu..%zu "
             "name=%zu..%zu index_type=%zu..%zu columns=%zu "
             "ref_table=%zu..%zu ref_schema=%zu..%zu ref_name=%zu..%zu "
             "ref_columns=%zu match=%s:%zu..%zu on_delete=%s:%zu..%zu "
             "on_update=%s:%zu..%zu check_expr=%zu..%zu "
             "check_enforced=%s:%zu..%zu key_options=%zu\n",
             j,
             mylite_create_table_key_kind_name(
                 mylite_ast_create_table_key_kind(ast, i, j)),
             mylite_ast_create_table_key_start(ast, i, j),
             mylite_ast_create_table_key_end(ast, i, j),
             mylite_ast_create_table_key_constraint_name_start(ast, i, j),
             mylite_ast_create_table_key_constraint_name_end(ast, i, j),
             mylite_ast_create_table_key_name_start(ast, i, j),
             mylite_ast_create_table_key_name_end(ast, i, j),
             mylite_ast_create_table_key_index_type_start(ast, i, j),
             mylite_ast_create_table_key_index_type_end(ast, i, j),
             column_count_for_key,
             mylite_ast_create_table_key_referenced_table_start(ast, i, j),
             mylite_ast_create_table_key_referenced_table_end(ast, i, j),
             mylite_ast_create_table_key_referenced_table_schema_start(ast, i, j),
             mylite_ast_create_table_key_referenced_table_schema_end(ast, i, j),
             mylite_ast_create_table_key_referenced_table_name_start(ast, i, j),
             mylite_ast_create_table_key_referenced_table_name_end(ast, i, j),
             referenced_column_count,
             mylite_create_table_foreign_match_kind_name(
                 mylite_ast_create_table_key_foreign_match_kind(ast, i, j)),
             mylite_ast_create_table_key_foreign_match_start(ast, i, j),
             mylite_ast_create_table_key_foreign_match_end(ast, i, j),
             mylite_create_table_foreign_action_name(
                 mylite_ast_create_table_key_foreign_on_delete_action(ast, i, j)),
             mylite_ast_create_table_key_foreign_on_delete_start(ast, i, j),
             mylite_ast_create_table_key_foreign_on_delete_end(ast, i, j),
             mylite_create_table_foreign_action_name(
                 mylite_ast_create_table_key_foreign_on_update_action(ast, i, j)),
             mylite_ast_create_table_key_foreign_on_update_start(ast, i, j),
             mylite_ast_create_table_key_foreign_on_update_end(ast, i, j),
             mylite_ast_create_table_key_check_expression_start(ast, i, j),
             mylite_ast_create_table_key_check_expression_end(ast, i, j),
             mylite_create_table_check_enforcement_name(
                 mylite_ast_create_table_key_check_enforcement(ast, i, j)),
             mylite_ast_create_table_key_check_enforcement_start(ast, i, j),
             mylite_ast_create_table_key_check_enforcement_end(ast, i, j),
             key_option_count);
      const char *constraint_name_value =
          mylite_ast_create_table_key_constraint_name_value(ast, i, j);
      size_t constraint_name_value_length =
          mylite_ast_create_table_key_constraint_name_value_length(ast, i, j);
      const char *key_name_value =
          mylite_ast_create_table_key_name_value(ast, i, j);
      size_t key_name_value_length =
          mylite_ast_create_table_key_name_value_length(ast, i, j);
      printf("    key[%zu].constraint_name_value len=%zu value=", j,
             constraint_name_value_length);
      if (constraint_name_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(constraint_name_value,
                            constraint_name_value_length);
      }
      printf(" name_value len=%zu value=", key_name_value_length);
      if (key_name_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(key_name_value, key_name_value_length);
      }
      fputc('\n', stdout);
      printf("    key[%zu].summary index_type=%s visibility=%s "
             "key_block_size=%d:%llu comment=",
             j,
             mylite_create_table_index_type_name(
                 mylite_ast_create_table_key_view_index_type_kind(key_view)),
             mylite_create_table_key_visibility_name(
                 mylite_ast_create_table_key_view_visibility(key_view)),
             mylite_ast_create_table_key_view_has_key_block_size_value(key_view),
             mylite_ast_create_table_key_view_key_block_size_value(key_view));
      const char *key_comment_value =
          mylite_ast_create_table_key_view_comment_value(key_view);
      size_t key_comment_value_length =
          mylite_ast_create_table_key_view_comment_value_length(key_view);
      if (key_comment_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(key_comment_value, key_comment_value_length);
      }
      fputs(" parser=", stdout);
      const char *parser_value =
          mylite_ast_create_table_key_view_parser_value(key_view);
      size_t parser_value_length =
          mylite_ast_create_table_key_view_parser_value_length(key_view);
      if (parser_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(parser_value, parser_value_length);
      }
      fputc('\n', stdout);
      const char *referenced_schema_value =
          mylite_ast_create_table_key_referenced_table_schema_value(ast, i, j);
      size_t referenced_schema_value_length =
          mylite_ast_create_table_key_referenced_table_schema_value_length(ast, i,
                                                                           j);
      const char *referenced_name_value =
          mylite_ast_create_table_key_referenced_table_name_value(ast, i, j);
      size_t referenced_name_value_length =
          mylite_ast_create_table_key_referenced_table_name_value_length(ast, i,
                                                                         j);
      printf("    key[%zu].referenced_table_schema_value len=%zu value=", j,
             referenced_schema_value_length);
      if (referenced_schema_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(referenced_schema_value,
                            referenced_schema_value_length);
      }
      printf(" name_value len=%zu value=", referenced_name_value_length);
      if (referenced_name_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(referenced_name_value, referenced_name_value_length);
      }
      fputc('\n', stdout);
      for (size_t k = 0; k < column_count_for_key; k++) {
        printf("    key_column[%zu] kind=%s span=%zu..%zu name=%zu..%zu "
               "expr=%zu..%zu prefix=%zu..%zu prefix_value=%zu..%zu "
               "order=%s:%zu..%zu\n",
               k,
               mylite_create_table_key_part_kind_name(
                   mylite_ast_create_table_key_column_kind(ast, i, j, k)),
               mylite_ast_create_table_key_column_start(ast, i, j, k),
               mylite_ast_create_table_key_column_end(ast, i, j, k),
               mylite_ast_create_table_key_column_name_start(ast, i, j, k),
               mylite_ast_create_table_key_column_name_end(ast, i, j, k),
               mylite_ast_create_table_key_column_expression_start(ast, i, j, k),
               mylite_ast_create_table_key_column_expression_end(ast, i, j, k),
               mylite_ast_create_table_key_column_prefix_start(ast, i, j, k),
               mylite_ast_create_table_key_column_prefix_end(ast, i, j, k),
               mylite_ast_create_table_key_column_prefix_value_start(ast, i, j,
                                                                     k),
               mylite_ast_create_table_key_column_prefix_value_end(ast, i, j,
                                                                   k),
               mylite_create_table_key_part_order_name(
                   mylite_ast_create_table_key_column_order(ast, i, j, k)),
               mylite_ast_create_table_key_column_order_start(ast, i, j, k),
               mylite_ast_create_table_key_column_order_end(ast, i, j, k));
        const char *name_value =
            mylite_ast_create_table_key_column_name_value(ast, i, j, k);
        size_t name_value_length =
            mylite_ast_create_table_key_column_name_value_length(ast, i, j, k);
        printf("      key_column[%zu].name_value len=%zu value=", k,
               name_value_length);
        if (name_value == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(name_value, name_value_length);
        }
        fputc('\n', stdout);
      }
      for (size_t k = 0; k < referenced_column_count; k++) {
        printf("    ref_column[%zu] kind=%s span=%zu..%zu name=%zu..%zu "
               "expr=%zu..%zu order=%s\n",
               k,
               mylite_create_table_key_part_kind_name(
                   mylite_ast_create_table_key_referenced_column_kind(ast, i, j,
                                                                      k)),
               mylite_ast_create_table_key_referenced_column_start(ast, i, j, k),
               mylite_ast_create_table_key_referenced_column_end(ast, i, j, k),
               mylite_ast_create_table_key_referenced_column_name_start(ast, i, j,
                                                                        k),
               mylite_ast_create_table_key_referenced_column_name_end(ast, i, j,
                                                                      k),
               mylite_ast_create_table_key_referenced_column_expression_start(
                   ast, i, j, k),
               mylite_ast_create_table_key_referenced_column_expression_end(
                   ast, i, j, k),
               mylite_create_table_key_part_order_name(
                   mylite_ast_create_table_key_referenced_column_order(ast, i, j,
                                                                       k)));
        const char *name_value =
            mylite_ast_create_table_key_referenced_column_name_value(ast, i, j,
                                                                     k);
        size_t name_value_length =
            mylite_ast_create_table_key_referenced_column_name_value_length(
                ast, i, j, k);
        printf("      ref_column[%zu].name_value len=%zu value=", k,
               name_value_length);
        if (name_value == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(name_value, name_value_length);
        }
        fputc('\n', stdout);
      }
      for (size_t k = 0; k < key_option_count; k++) {
        const MyliteAstCreateTableKeyOption *option_view =
            key_view == NULL ? NULL
                             : mylite_ast_create_table_key_view_option_at(
                                   key_view, k);
        printf("    key_option[%zu] kind=%s span=%zu..%zu name=%zu..%zu "
               "value=%zu..%zu value_kind=%s index_type=%s unsigned=%d:%llu\n",
               k,
               mylite_create_table_key_option_kind_name(
                   mylite_ast_create_table_key_option_kind(ast, i, j, k)),
               mylite_ast_create_table_key_option_start(ast, i, j, k),
               mylite_ast_create_table_key_option_end(ast, i, j, k),
               mylite_ast_create_table_key_option_name_start(ast, i, j, k),
               mylite_ast_create_table_key_option_name_end(ast, i, j, k),
               mylite_ast_create_table_key_option_value_start(ast, i, j, k),
               mylite_ast_create_table_key_option_value_end(ast, i, j, k),
               mylite_create_table_key_option_value_kind_name(
                   mylite_ast_create_table_key_option_view_value_kind(
                       option_view)),
               mylite_create_table_index_type_name(
                   mylite_ast_create_table_key_option_view_index_type_kind(
                       option_view)),
               mylite_ast_create_table_key_option_view_has_unsigned_integer(
                   option_view),
               mylite_ast_create_table_key_option_view_unsigned_integer_value(
                   option_view));
        const char *value =
            mylite_ast_create_table_key_option_view_value(option_view);
        size_t value_length =
            mylite_ast_create_table_key_option_view_value_length(option_view);
        printf("      key_option[%zu].value len=%zu value=", k, value_length);
        if (value == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(value, value_length);
        }
        fputc('\n', stdout);
      }
    }
    for (size_t j = 0; j < option_count; j++) {
      printf("  option[%zu] kind=%s span=%zu..%zu name=%zu..%zu "
             "value=%zu..%zu\n",
             j,
             mylite_create_table_option_kind_name(
                 mylite_ast_create_table_option_kind(ast, i, j)),
             mylite_ast_create_table_option_start(ast, i, j),
             mylite_ast_create_table_option_end(ast, i, j),
             mylite_ast_create_table_option_name_start(ast, i, j),
             mylite_ast_create_table_option_name_end(ast, i, j),
             mylite_ast_create_table_option_value_start(ast, i, j),
             mylite_ast_create_table_option_value_end(ast, i, j));
    }
  }
}

static const char *node_symbol_or_none(const MyliteAstNode *node) {
  const char *symbol = mylite_ast_node_symbol_name(node);
  return symbol == NULL ? "none" : symbol;
}

static void dump_create_table_view_handles(
    const MyliteAstCreateTable *create_table) {
  const char *engine_value =
      mylite_ast_create_table_view_engine_value(create_table);
  const char *charset_value =
      mylite_ast_create_table_view_charset_value(create_table);
  const char *collation_value =
      mylite_ast_create_table_view_collation_value(create_table);
  const char *comment_value =
      mylite_ast_create_table_view_comment_value(create_table);
  printf("    create_table.option_summary engine=");
  if (engine_value == NULL) {
    fputs("none", stdout);
  } else {
    print_escaped_bytes(
        engine_value,
        mylite_ast_create_table_view_engine_value_length(create_table));
  }
  fputs(" charset=", stdout);
  if (charset_value == NULL) {
    fputs("none", stdout);
  } else {
    print_escaped_bytes(
        charset_value,
        mylite_ast_create_table_view_charset_value_length(create_table));
  }
  fputs(" collation=", stdout);
  if (collation_value == NULL) {
    fputs("none", stdout);
  } else {
    print_escaped_bytes(
        collation_value,
        mylite_ast_create_table_view_collation_value_length(create_table));
  }
  fputs(" comment=", stdout);
  if (comment_value == NULL) {
    fputs("none", stdout);
  } else {
    print_escaped_bytes(
        comment_value,
        mylite_ast_create_table_view_comment_value_length(create_table));
  }
  printf(" auto_increment=%d:%llu\n",
         mylite_ast_create_table_view_has_auto_increment_value(create_table),
         mylite_ast_create_table_view_auto_increment_value(create_table));

  for (size_t i = 0; i < mylite_ast_create_table_view_column_count(create_table);
       i++) {
    const MyliteAstCreateTableColumn *column =
        mylite_ast_create_table_view_column_at(create_table, i);
    const char *name_value =
        mylite_ast_create_table_column_view_name_value(column);
    size_t name_value_length =
        mylite_ast_create_table_column_view_name_value_length(column);
    printf("    create_table.column[%zu] span=%zu..%zu name=%zu..%zu "
           "type=%zu..%zu type_name=%zu..%zu type_params=%zu..%zu "
           "numeric_params=%zu type_elements=%zu type_length=%d:%llu "
           "family=%s kind=%s storage=%s flags=0x%x type_attrs=%zu..%zu "
           "type_unsigned=%zu..%zu options=%zu..%zu default=%zu..%zu "
           "default_value=%zu..%zu generated=%zu..%zu generated_expr=%zu..%zu "
           "comment=%zu..%zu comment_value=%zu..%zu check=%zu..%zu "
           "check_expr=%zu..%zu check_enforced=%s:%zu..%zu reference=%zu..%zu "
           "type_node=%s options_node=%s name_value_len=%zu value=",
           i, mylite_ast_create_table_column_view_start(column),
           mylite_ast_create_table_column_view_end(column),
           mylite_ast_create_table_column_view_name_start(column),
           mylite_ast_create_table_column_view_name_end(column),
           mylite_ast_create_table_column_view_type_start(column),
           mylite_ast_create_table_column_view_type_end(column),
           mylite_ast_create_table_column_view_type_name_start(column),
           mylite_ast_create_table_column_view_type_name_end(column),
           mylite_ast_create_table_column_view_type_parameters_start(column),
           mylite_ast_create_table_column_view_type_parameters_end(column),
           mylite_ast_create_table_column_view_type_numeric_parameter_count(
               column),
           mylite_ast_create_table_column_view_type_element_count(column),
           mylite_ast_create_table_column_view_type_has_length(column),
           mylite_ast_create_table_column_view_type_length(column),
           mylite_create_table_column_type_family_name(
               mylite_ast_create_table_column_view_type_family(column)),
           mylite_create_table_column_type_kind_name(
               mylite_ast_create_table_column_view_type_kind(column)),
           mylite_create_table_column_storage_class_name(
               mylite_ast_create_table_column_view_storage_class(column)),
           mylite_ast_create_table_column_view_flags(column),
           mylite_ast_create_table_column_view_type_attributes_start(column),
           mylite_ast_create_table_column_view_type_attributes_end(column),
           mylite_ast_create_table_column_view_type_unsigned_start(column),
           mylite_ast_create_table_column_view_type_unsigned_end(column),
           mylite_ast_create_table_column_view_options_start(column),
           mylite_ast_create_table_column_view_options_end(column),
           mylite_ast_create_table_column_view_default_start(column),
           mylite_ast_create_table_column_view_default_end(column),
           mylite_ast_create_table_column_view_default_value_start(column),
           mylite_ast_create_table_column_view_default_value_end(column),
           mylite_ast_create_table_column_view_generated_start(column),
           mylite_ast_create_table_column_view_generated_end(column),
           mylite_ast_create_table_column_view_generated_expression_start(
               column),
           mylite_ast_create_table_column_view_generated_expression_end(column),
           mylite_ast_create_table_column_view_comment_start(column),
           mylite_ast_create_table_column_view_comment_end(column),
           mylite_ast_create_table_column_view_comment_value_start(column),
           mylite_ast_create_table_column_view_comment_value_end(column),
           mylite_ast_create_table_column_view_check_start(column),
           mylite_ast_create_table_column_view_check_end(column),
           mylite_ast_create_table_column_view_check_expression_start(column),
           mylite_ast_create_table_column_view_check_expression_end(column),
           mylite_create_table_check_enforcement_name(
               mylite_ast_create_table_column_view_check_enforcement(column)),
           mylite_ast_create_table_column_view_check_enforcement_start(column),
           mylite_ast_create_table_column_view_check_enforcement_end(column),
           mylite_ast_create_table_column_view_reference_start(column),
           mylite_ast_create_table_column_view_reference_end(column),
           node_symbol_or_none(
               mylite_ast_create_table_column_view_type_node(column)),
           node_symbol_or_none(
               mylite_ast_create_table_column_view_options_node(column)),
           name_value_length);
    if (name_value == NULL) {
      fputs("none", stdout);
    } else {
      print_escaped_bytes(name_value, name_value_length);
    }
    fputc('\n', stdout);
    for (size_t j = 0;
         j < mylite_ast_create_table_column_view_type_element_count(column);
         j++) {
      const MyliteAstCreateTableColumnTypeElement *element =
          mylite_ast_create_table_column_view_type_element_at(column, j);
      const char *element_value =
          mylite_ast_create_table_column_type_element_view_value(element);
      size_t element_value_length =
          mylite_ast_create_table_column_type_element_view_value_length(
              element);
      printf("      create_table.column[%zu].type_element[%zu] span=%zu..%zu "
             "value_len=%zu value=",
             i, j,
             mylite_ast_create_table_column_type_element_view_start(element),
             mylite_ast_create_table_column_type_element_view_end(element),
             element_value_length);
      if (element_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(element_value, element_value_length);
      }
      fputc('\n', stdout);
    }
  }
  for (size_t i = 0; i < mylite_ast_create_table_view_key_count(create_table);
       i++) {
    const MyliteAstCreateTableKey *key =
        mylite_ast_create_table_view_key_at(create_table, i);
    const char *constraint_name_value =
        mylite_ast_create_table_key_view_constraint_name_value(key);
    size_t constraint_name_value_length =
        mylite_ast_create_table_key_view_constraint_name_value_length(key);
    const char *name_value = mylite_ast_create_table_key_view_name_value(key);
    size_t name_value_length =
        mylite_ast_create_table_key_view_name_value_length(key);
    printf("    create_table.key[%zu] kind=%s span=%zu..%zu columns=%zu "
           "ref_columns=%zu options=%zu constraint_name_value_len=%zu value=",
           i,
           mylite_create_table_key_kind_name(
               mylite_ast_create_table_key_view_kind(key)),
           mylite_ast_create_table_key_view_start(key),
           mylite_ast_create_table_key_view_end(key),
           mylite_ast_create_table_key_view_column_count(key),
           mylite_ast_create_table_key_view_referenced_column_count(key),
           mylite_ast_create_table_key_view_option_count(key),
           constraint_name_value_length);
    if (constraint_name_value == NULL) {
      fputs("none", stdout);
    } else {
      print_escaped_bytes(constraint_name_value,
                          constraint_name_value_length);
    }
    printf(" name_value_len=%zu value=", name_value_length);
    if (name_value == NULL) {
      fputs("none", stdout);
    } else {
      print_escaped_bytes(name_value, name_value_length);
    }
    fputc('\n', stdout);
    for (size_t j = 0; j < mylite_ast_create_table_key_view_column_count(key);
         j++) {
      const MyliteAstCreateTableKeyPart *part =
          mylite_ast_create_table_key_view_column_at(key, j);
      const char *part_name_value =
          mylite_ast_create_table_key_part_view_name_value(part);
      size_t part_name_value_length =
          mylite_ast_create_table_key_part_view_name_value_length(part);
      printf("      create_table.key[%zu].column[%zu] kind=%s span=%zu..%zu "
             "name=%zu..%zu expr=%zu..%zu prefix=%zu..%zu "
             "prefix_value=%zu..%zu order=%s order_span=%zu..%zu "
             "name_value_len=%zu value=",
             i, j,
             mylite_create_table_key_part_kind_name(
                 mylite_ast_create_table_key_part_view_kind(part)),
             mylite_ast_create_table_key_part_view_start(part),
             mylite_ast_create_table_key_part_view_end(part),
             mylite_ast_create_table_key_part_view_name_start(part),
             mylite_ast_create_table_key_part_view_name_end(part),
             mylite_ast_create_table_key_part_view_expression_start(part),
             mylite_ast_create_table_key_part_view_expression_end(part),
             mylite_ast_create_table_key_part_view_prefix_start(part),
             mylite_ast_create_table_key_part_view_prefix_end(part),
             mylite_ast_create_table_key_part_view_prefix_value_start(part),
             mylite_ast_create_table_key_part_view_prefix_value_end(part),
             mylite_create_table_key_part_order_name(
                 mylite_ast_create_table_key_part_view_order(part)),
             mylite_ast_create_table_key_part_view_order_start(part),
             mylite_ast_create_table_key_part_view_order_end(part),
             part_name_value_length);
      if (part_name_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(part_name_value, part_name_value_length);
      }
      fputc('\n', stdout);
    }
    for (size_t j = 0;
         j < mylite_ast_create_table_key_view_referenced_column_count(key);
         j++) {
      const MyliteAstCreateTableKeyPart *part =
          mylite_ast_create_table_key_view_referenced_column_at(key, j);
      const char *part_name_value =
          mylite_ast_create_table_key_part_view_name_value(part);
      size_t part_name_value_length =
          mylite_ast_create_table_key_part_view_name_value_length(part);
      printf("      create_table.key[%zu].referenced_column[%zu] kind=%s "
             "span=%zu..%zu name=%zu..%zu expr=%zu..%zu order=%s "
             "order_span=%zu..%zu name_value_len=%zu value=",
             i, j,
             mylite_create_table_key_part_kind_name(
                 mylite_ast_create_table_key_part_view_kind(part)),
             mylite_ast_create_table_key_part_view_start(part),
             mylite_ast_create_table_key_part_view_end(part),
             mylite_ast_create_table_key_part_view_name_start(part),
             mylite_ast_create_table_key_part_view_name_end(part),
             mylite_ast_create_table_key_part_view_expression_start(part),
             mylite_ast_create_table_key_part_view_expression_end(part),
             mylite_create_table_key_part_order_name(
                 mylite_ast_create_table_key_part_view_order(part)),
             mylite_ast_create_table_key_part_view_order_start(part),
             mylite_ast_create_table_key_part_view_order_end(part),
             part_name_value_length);
      if (part_name_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(part_name_value, part_name_value_length);
      }
      fputc('\n', stdout);
    }
    for (size_t j = 0; j < mylite_ast_create_table_key_view_option_count(key);
         j++) {
      const MyliteAstCreateTableKeyOption *option =
          mylite_ast_create_table_key_view_option_at(key, j);
      printf("      create_table.key[%zu].option[%zu] kind=%s span=%zu..%zu "
             "name=%zu..%zu value=%zu..%zu\n",
             i, j,
             mylite_create_table_key_option_kind_name(
                 mylite_ast_create_table_key_option_view_kind(option)),
             mylite_ast_create_table_key_option_view_start(option),
             mylite_ast_create_table_key_option_view_end(option),
             mylite_ast_create_table_key_option_view_name_start(option),
             mylite_ast_create_table_key_option_view_name_end(option),
             mylite_ast_create_table_key_option_view_value_start(option),
             mylite_ast_create_table_key_option_view_value_end(option));
    }
  }
  for (size_t i = 0; i < mylite_ast_create_table_view_option_count(create_table);
       i++) {
    const MyliteAstCreateTableOption *option =
        mylite_ast_create_table_view_option_at(create_table, i);
    const char *value = mylite_ast_create_table_option_view_value(option);
    size_t value_length =
        mylite_ast_create_table_option_view_value_length(option);
    printf("    create_table.option[%zu] kind=%s span=%zu..%zu name=%zu..%zu "
           "value=%zu..%zu value_kind=%s unsigned=%d:%llu value_len=%zu "
           "decoded_value=",
           i,
           mylite_create_table_option_kind_name(
               mylite_ast_create_table_option_view_kind(option)),
           mylite_ast_create_table_option_view_start(option),
           mylite_ast_create_table_option_view_end(option),
           mylite_ast_create_table_option_view_name_start(option),
           mylite_ast_create_table_option_view_name_end(option),
           mylite_ast_create_table_option_view_value_start(option),
           mylite_ast_create_table_option_view_value_end(option),
           mylite_create_table_option_value_kind_name(
               mylite_ast_create_table_option_view_value_kind(option)),
           mylite_ast_create_table_option_view_has_unsigned_integer(option),
           mylite_ast_create_table_option_view_unsigned_integer_value(option),
           value_length);
    if (value == NULL) {
      fputs("none", stdout);
    } else {
      print_escaped_bytes(value, value_length);
    }
    fputc('\n', stdout);
  }
}

static void print_escaped_bytes(const char *value, size_t length) {
  fputc('"', stdout);
  for (size_t i = 0; i < length; i++) {
    unsigned char ch = (unsigned char)value[i];
    switch (ch) {
      case '\0':
        fputs("\\0", stdout);
        break;
      case '\b':
        fputs("\\b", stdout);
        break;
      case '\n':
        fputs("\\n", stdout);
        break;
      case '\r':
        fputs("\\r", stdout);
        break;
      case '\t':
        fputs("\\t", stdout);
        break;
      case 26:
        fputs("\\Z", stdout);
        break;
      case '\\':
        fputs("\\\\", stdout);
        break;
      case '"':
        fputs("\\\"", stdout);
        break;
      default:
        if (ch < 0x20 || ch >= 0x7f) {
          printf("\\x%02x", ch);
        } else {
          fputc(ch, stdout);
        }
        break;
    }
  }
  fputc('"', stdout);
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
