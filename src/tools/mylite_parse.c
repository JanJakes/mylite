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
static void dump_expression_tree(const MyliteAstExpression *expression,
                                 unsigned depth);
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
    const MyliteAstAlterTable *alter_table =
        mylite_ast_alter_table_view(ast, i);
    const MyliteAstCreateDatabase *create_database =
        mylite_ast_create_database_view(ast, i);
    const MyliteAstCreateTable *create_table =
        mylite_ast_create_table_view(ast, i);
    const MyliteAstCreateIndex *create_index =
        mylite_ast_create_index_view(ast, i);
    const MyliteAstCreateView *create_view =
        mylite_ast_create_view_view(ast, i);
    const MyliteAstDropDatabase *drop_database =
        mylite_ast_drop_database_view(ast, i);
    const MyliteAstDropIndex *drop_index = mylite_ast_drop_index_view(ast, i);
    const MyliteAstDropTable *drop_table = mylite_ast_drop_table_view(ast, i);
    const MyliteAstDropView *drop_view = mylite_ast_drop_view_view(ast, i);
    const MyliteAstCallStatement *call_statement =
        mylite_ast_call_statement_view(ast, i);
    const MyliteAstDoStatement *do_statement =
        mylite_ast_do_statement_view(ast, i);
    const MyliteAstPrepareStatement *prepare_statement =
        mylite_ast_prepare_statement_view(ast, i);
    const MyliteAstExecuteStatement *execute_statement =
        mylite_ast_execute_statement_view(ast, i);
    const MyliteAstDeallocateStatement *deallocate_statement =
        mylite_ast_deallocate_statement_view(ast, i);
    const MyliteAstExplainStatement *explain_statement =
        mylite_ast_explain_statement_view(ast, i);
    const MyliteAstShowStatement *show_statement =
        mylite_ast_show_statement_view(ast, i);
    const MyliteAstLockStatement *lock_statement =
        mylite_ast_lock_statement_view(ast, i);
    const MyliteAstTableMaintenanceStatement *maintenance_statement =
        mylite_ast_table_maintenance_statement_view(ast, i);
    const MyliteAstKillStatement *kill_statement =
        mylite_ast_kill_statement_view(ast, i);
    const MyliteAstFlushStatement *flush_statement =
        mylite_ast_flush_statement_view(ast, i);
    const MyliteAstLoadStatement *load_statement =
        mylite_ast_load_statement_view(ast, i);
    const MyliteAstAccountStatement *account_statement =
        mylite_ast_account_statement_view(ast, i);
    const MyliteAstPrivilegeStatement *privilege_statement =
        mylite_ast_privilege_statement_view(ast, i);
    const MyliteAstRoleStatement *role_statement =
        mylite_ast_role_statement_view(ast, i);
    const MyliteAstDeleteStatement *delete_statement =
        mylite_ast_delete_statement_view(ast, i);
    const MyliteAstInsertStatement *insert_statement =
        mylite_ast_insert_statement_view(ast, i);
    const MyliteAstReplaceStatement *replace_statement =
        mylite_ast_replace_statement_view(ast, i);
    const MyliteAstRenameTable *rename_table =
        mylite_ast_rename_table_view(ast, i);
    const MyliteAstSelectStatement *select_statement =
        mylite_ast_select_statement_view(ast, i);
    const MyliteAstValuesStatement *values_statement =
        mylite_ast_values_statement_view(ast, i);
    const MyliteAstSetStatement *set_statement =
        mylite_ast_set_statement_view(ast, i);
    const MyliteAstTruncateTable *truncate_table =
        mylite_ast_truncate_table_view(ast, i);
    const MyliteAstTransactionStatement *transaction_statement =
        mylite_ast_transaction_statement_view(ast, i);
    const MyliteAstUpdateStatement *update_statement =
        mylite_ast_update_statement_view(ast, i);
    const MyliteAstUseDatabase *use_database =
        mylite_ast_use_database_view(ast, i);
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
    if (alter_table != NULL) {
      printf("  alter_table span=%zu..%zu target=%zu..%zu specs=%zu "
             "options=%zu table=",
             mylite_ast_alter_table_view_start(alter_table),
             mylite_ast_alter_table_view_end(alter_table),
             mylite_ast_alter_table_view_target_start(alter_table),
             mylite_ast_alter_table_view_target_end(alter_table),
             mylite_ast_alter_table_view_spec_count(alter_table),
             mylite_ast_alter_table_view_option_count(alter_table));
      const char *schema = mylite_ast_alter_table_view_schema_value(alter_table);
      size_t schema_length =
          mylite_ast_alter_table_view_schema_value_length(alter_table);
      if (schema != NULL) {
        print_escaped_bytes(schema, schema_length);
        fputc('.', stdout);
      }
      const char *name = mylite_ast_alter_table_view_name_value(alter_table);
      size_t name_length =
          mylite_ast_alter_table_view_name_value_length(alter_table);
      if (name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(name, name_length);
      }
      fputc('\n', stdout);
      for (size_t j = 0;
           j < mylite_ast_alter_table_view_spec_count(alter_table); j++) {
        const MyliteAstAlterTableSpec *spec =
            mylite_ast_alter_table_view_spec_at(alter_table, j);
        const MyliteAstCreateTableColumn *column =
            mylite_ast_alter_table_spec_view_column(spec);
        const MyliteAstCreateTableKey *key =
            mylite_ast_alter_table_spec_view_key(spec);
        printf("    alter_spec[%zu] kind=%s span=%zu..%zu if_exists=%d "
               "if_not_exists=%d columns=%zu keys=%zu name=",
               j,
               mylite_alter_table_spec_kind_name(
                   mylite_ast_alter_table_spec_view_kind(spec)),
               mylite_ast_alter_table_spec_view_start(spec),
               mylite_ast_alter_table_spec_view_end(spec),
               mylite_ast_alter_table_spec_view_has_if_exists(spec),
               mylite_ast_alter_table_spec_view_has_if_not_exists(spec),
               mylite_ast_alter_table_spec_view_column_count(spec),
               mylite_ast_alter_table_spec_view_key_count(spec));
        const char *spec_name =
            mylite_ast_alter_table_spec_view_name_value(spec);
        size_t spec_name_length =
            mylite_ast_alter_table_spec_view_name_value_length(spec);
        if (spec_name == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(spec_name, spec_name_length);
        }
        fputs(" secondary=", stdout);
        const char *secondary =
            mylite_ast_alter_table_spec_view_secondary_name_value(spec);
        size_t secondary_length =
            mylite_ast_alter_table_spec_view_secondary_name_value_length(spec);
        if (secondary == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(secondary, secondary_length);
        }
        fputs(" table=", stdout);
        const char *spec_schema =
            mylite_ast_alter_table_spec_view_table_schema_value(spec);
        size_t spec_schema_length =
            mylite_ast_alter_table_spec_view_table_schema_value_length(spec);
        if (spec_schema != NULL) {
          print_escaped_bytes(spec_schema, spec_schema_length);
          fputc('.', stdout);
        }
        const char *spec_table =
            mylite_ast_alter_table_spec_view_table_name_value(spec);
        size_t spec_table_length =
            mylite_ast_alter_table_spec_view_table_name_value_length(spec);
        if (spec_table == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(spec_table, spec_table_length);
        }
        if (column != NULL) {
          fputs(" column_name=", stdout);
          const char *column_name =
              mylite_ast_create_table_column_view_name_value(column);
          size_t column_name_length =
              mylite_ast_create_table_column_view_name_value_length(column);
          if (column_name == NULL) {
            fputs("none", stdout);
          } else {
            print_escaped_bytes(column_name, column_name_length);
          }
        }
        if (key != NULL) {
          printf(" key_kind=%s key_name=",
                 mylite_create_table_key_kind_name(
                     mylite_ast_create_table_key_view_kind(key)));
          const char *key_name = mylite_ast_create_table_key_view_name_value(key);
          size_t key_name_length =
              mylite_ast_create_table_key_view_name_value_length(key);
          if (key_name == NULL) {
            fputs("none", stdout);
          } else {
            print_escaped_bytes(key_name, key_name_length);
          }
        }
        fputc('\n', stdout);
      }
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
    if (create_database != NULL) {
      printf("  create_database span=%zu..%zu if_not_exists=%d "
             "schema_keyword=%d options=%zu name_len=%zu name=",
             mylite_ast_create_database_view_start(create_database),
             mylite_ast_create_database_view_end(create_database),
             mylite_ast_create_database_view_has_if_not_exists(create_database),
             mylite_ast_create_database_view_uses_schema_keyword(
                 create_database),
             mylite_ast_create_database_view_option_count(create_database),
             mylite_ast_create_database_view_name_value_length(
                 create_database));
      const char *database_name =
          mylite_ast_create_database_view_name_value(create_database);
      size_t database_name_length =
          mylite_ast_create_database_view_name_value_length(create_database);
      if (database_name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(database_name, database_name_length);
      }
      fputs(" charset=", stdout);
      const char *charset =
          mylite_ast_create_database_view_charset_value(create_database);
      size_t charset_length =
          mylite_ast_create_database_view_charset_value_length(create_database);
      if (charset == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(charset, charset_length);
      }
      fputs(" collation=", stdout);
      const char *collation =
          mylite_ast_create_database_view_collation_value(create_database);
      size_t collation_length =
          mylite_ast_create_database_view_collation_value_length(
              create_database);
      if (collation == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(collation, collation_length);
      }
      fputs(" encryption=", stdout);
      const char *encryption =
          mylite_ast_create_database_view_encryption_value(create_database);
      size_t encryption_length =
          mylite_ast_create_database_view_encryption_value_length(
              create_database);
      if (encryption == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(encryption, encryption_length);
      }
      fputc('\n', stdout);
      for (size_t j = 0;
           j < mylite_ast_create_database_view_option_count(create_database);
           j++) {
        const MyliteAstDatabaseOption *option =
            mylite_ast_create_database_view_option_at(create_database, j);
        printf("    database_option[%zu] kind=%s value_kind=%s "
               "span=%zu..%zu name=%zu..%zu value=%zu..%zu value_len=%zu "
               "value=",
               j,
               mylite_database_option_kind_name(
                   mylite_ast_database_option_view_kind(option)),
               mylite_database_option_value_kind_name(
                   mylite_ast_database_option_view_value_kind(option)),
               mylite_ast_database_option_view_start(option),
               mylite_ast_database_option_view_end(option),
               mylite_ast_database_option_view_name_start(option),
               mylite_ast_database_option_view_name_end(option),
               mylite_ast_database_option_view_value_start(option),
               mylite_ast_database_option_view_value_end(option),
               mylite_ast_database_option_view_value_length(option));
        const char *value = mylite_ast_database_option_view_value(option);
        size_t value_length =
            mylite_ast_database_option_view_value_length(option);
        if (value == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(value, value_length);
        }
        fputc('\n', stdout);
      }
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
    if (create_view != NULL) {
      printf("  create_view span=%zu..%zu or_replace=%d algorithm=%s "
             "security=%s check_option=%s columns=%zu select=%zu..%zu "
             "view=",
             mylite_ast_create_view_view_start(create_view),
             mylite_ast_create_view_view_end(create_view),
             mylite_ast_create_view_view_has_or_replace(create_view),
             mylite_create_view_algorithm_name(
                 mylite_ast_create_view_view_algorithm(create_view)),
             mylite_view_sql_security_name(
                 mylite_ast_create_view_view_sql_security(create_view)),
             mylite_view_check_option_name(
                 mylite_ast_create_view_view_check_option(create_view)),
             mylite_ast_create_view_view_column_count(create_view),
             mylite_ast_create_view_view_select_start(create_view),
             mylite_ast_create_view_view_select_end(create_view));
      const char *schema =
          mylite_ast_create_view_view_schema_value(create_view);
      size_t schema_length =
          mylite_ast_create_view_view_schema_value_length(create_view);
      if (schema != NULL) {
        print_escaped_bytes(schema, schema_length);
        fputc('.', stdout);
      }
      const char *name = mylite_ast_create_view_view_name_value(create_view);
      size_t name_length =
          mylite_ast_create_view_view_name_value_length(create_view);
      if (name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(name, name_length);
      }
      fputc('\n', stdout);
      for (size_t j = 0;
           j < mylite_ast_create_view_view_column_count(create_view); j++) {
        const MyliteAstViewColumn *column =
            mylite_ast_create_view_view_column_at(create_view, j);
        printf("    view_column[%zu] span=%zu..%zu name_len=%zu name=", j,
               mylite_ast_view_column_view_start(column),
               mylite_ast_view_column_view_end(column),
               mylite_ast_view_column_view_name_value_length(column));
        const char *column_name = mylite_ast_view_column_view_name_value(column);
        size_t column_name_length =
            mylite_ast_view_column_view_name_value_length(column);
        if (column_name == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(column_name, column_name_length);
        }
        fputc('\n', stdout);
      }
    }
    if (drop_database != NULL) {
      printf("  drop_database span=%zu..%zu if_exists=%d schema_keyword=%d "
             "name_len=%zu name=",
             mylite_ast_drop_database_view_start(drop_database),
             mylite_ast_drop_database_view_end(drop_database),
             mylite_ast_drop_database_view_has_if_exists(drop_database),
             mylite_ast_drop_database_view_uses_schema_keyword(drop_database),
             mylite_ast_drop_database_view_name_value_length(drop_database));
      const char *database_name =
          mylite_ast_drop_database_view_name_value(drop_database);
      size_t database_name_length =
          mylite_ast_drop_database_view_name_value_length(drop_database);
      if (database_name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(database_name, database_name_length);
      }
      fputc('\n', stdout);
    }
    if (drop_index != NULL) {
      printf("  drop_index span=%zu..%zu if_exists=%d hypothetical=%d "
             "name_len=%zu table=",
             mylite_ast_drop_index_view_start(drop_index),
             mylite_ast_drop_index_view_end(drop_index),
             mylite_ast_drop_index_view_has_if_exists(drop_index),
             mylite_ast_drop_index_view_is_hypothetical(drop_index),
             mylite_ast_drop_index_view_name_value_length(drop_index));
      const char *table_schema =
          mylite_ast_drop_index_view_table_schema_value(drop_index);
      size_t table_schema_length =
          mylite_ast_drop_index_view_table_schema_value_length(drop_index);
      if (table_schema != NULL) {
        print_escaped_bytes(table_schema, table_schema_length);
        fputc('.', stdout);
      }
      const char *table_name =
          mylite_ast_drop_index_view_table_name_value(drop_index);
      size_t table_name_length =
          mylite_ast_drop_index_view_table_name_value_length(drop_index);
      if (table_name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(table_name, table_name_length);
      }
      fputc('\n', stdout);
    }
    if (drop_table != NULL) {
      printf("  drop_table span=%zu..%zu temporary=%d if_exists=%d tables=%zu\n",
             mylite_ast_drop_table_view_start(drop_table),
             mylite_ast_drop_table_view_end(drop_table),
             mylite_ast_drop_table_view_is_temporary(drop_table),
             mylite_ast_drop_table_view_has_if_exists(drop_table),
             mylite_ast_drop_table_view_table_count(drop_table));
    }
    if (drop_view != NULL) {
      printf("  drop_view span=%zu..%zu if_exists=%d mode=%s views=%zu\n",
             mylite_ast_drop_view_view_start(drop_view),
             mylite_ast_drop_view_view_end(drop_view),
             mylite_ast_drop_view_view_has_if_exists(drop_view),
             mylite_drop_view_mode_name(
                 mylite_ast_drop_view_view_mode(drop_view)),
             mylite_ast_drop_view_view_view_count(drop_view));
    }
    if (prepare_statement != NULL) {
      printf("  prepare_statement span=%zu..%zu name=%zu..%zu "
             "source=%s:%zu..%zu name_len=%zu name=",
             mylite_ast_prepare_statement_view_start(prepare_statement),
             mylite_ast_prepare_statement_view_end(prepare_statement),
             mylite_ast_prepare_statement_view_name_start(prepare_statement),
             mylite_ast_prepare_statement_view_name_end(prepare_statement),
             mylite_prepare_statement_source_kind_name(
                 mylite_ast_prepare_statement_view_source_kind(
                     prepare_statement)),
             mylite_ast_prepare_statement_view_source_start(prepare_statement),
             mylite_ast_prepare_statement_view_source_end(prepare_statement),
             mylite_ast_prepare_statement_view_name_value_length(
                 prepare_statement));
      const char *name =
          mylite_ast_prepare_statement_view_name_value(prepare_statement);
      size_t name_length =
          mylite_ast_prepare_statement_view_name_value_length(
              prepare_statement);
      if (name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(name, name_length);
      }
      fputs(" source_len=", stdout);
      printf("%zu source=",
             mylite_ast_prepare_statement_view_source_value_length(
                 prepare_statement));
      const char *source =
          mylite_ast_prepare_statement_view_source_value(prepare_statement);
      size_t source_length =
          mylite_ast_prepare_statement_view_source_value_length(
              prepare_statement);
      if (source == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(source, source_length);
      }
      fputc('\n', stdout);
    }
    if (execute_statement != NULL) {
      printf("  execute_statement span=%zu..%zu name=%zu..%zu "
             "using=%zu name_len=%zu name=",
             mylite_ast_execute_statement_view_start(execute_statement),
             mylite_ast_execute_statement_view_end(execute_statement),
             mylite_ast_execute_statement_view_name_start(execute_statement),
             mylite_ast_execute_statement_view_name_end(execute_statement),
             mylite_ast_execute_statement_view_using_count(execute_statement),
             mylite_ast_execute_statement_view_name_value_length(
                 execute_statement));
      const char *name =
          mylite_ast_execute_statement_view_name_value(execute_statement);
      size_t name_length =
          mylite_ast_execute_statement_view_name_value_length(
              execute_statement);
      if (name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(name, name_length);
      }
      fputc('\n', stdout);
      for (size_t j = 0;
           j < mylite_ast_execute_statement_view_using_count(execute_statement);
           j++) {
        const MyliteAstPreparedStatementVariable *variable =
            mylite_ast_execute_statement_view_using_variable_at(
                execute_statement, j);
        printf("    execute_using[%zu] span=%zu..%zu name=%zu..%zu "
               "name_len=%zu name=",
               j,
               mylite_ast_prepared_statement_variable_view_start(variable),
               mylite_ast_prepared_statement_variable_view_end(variable),
               mylite_ast_prepared_statement_variable_view_name_start(variable),
               mylite_ast_prepared_statement_variable_view_name_end(variable),
               mylite_ast_prepared_statement_variable_view_name_value_length(
                   variable));
        const char *variable_name =
            mylite_ast_prepared_statement_variable_view_name_value(variable);
        size_t variable_name_length =
            mylite_ast_prepared_statement_variable_view_name_value_length(
                variable);
        if (variable_name == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(variable_name, variable_name_length);
        }
        fputc('\n', stdout);
      }
    }
    if (deallocate_statement != NULL) {
      printf("  deallocate_statement span=%zu..%zu mode=%s name=%zu..%zu "
             "name_len=%zu name=",
             mylite_ast_deallocate_statement_view_start(deallocate_statement),
             mylite_ast_deallocate_statement_view_end(deallocate_statement),
             mylite_deallocate_statement_mode_name(
                 mylite_ast_deallocate_statement_view_mode(
                     deallocate_statement)),
             mylite_ast_deallocate_statement_view_name_start(
                 deallocate_statement),
             mylite_ast_deallocate_statement_view_name_end(
                 deallocate_statement),
             mylite_ast_deallocate_statement_view_name_value_length(
                 deallocate_statement));
      const char *name =
          mylite_ast_deallocate_statement_view_name_value(deallocate_statement);
      size_t name_length =
          mylite_ast_deallocate_statement_view_name_value_length(
              deallocate_statement);
      if (name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(name, name_length);
      }
      fputc('\n', stdout);
    }
    if (explain_statement != NULL) {
      printf("  explain_statement span=%zu..%zu kind=%s format=%s analyze=%d "
             "statement=%zu..%zu connection_id=%d:%llu id_span=%zu..%zu "
             "table=%zu..%zu column=%zu..%zu node=%s statement_node=%s\n",
             mylite_ast_explain_statement_view_start(explain_statement),
             mylite_ast_explain_statement_view_end(explain_statement),
             mylite_explain_statement_kind_name(
                 mylite_ast_explain_statement_view_kind(explain_statement)),
             mylite_explain_format_kind_name(
                 mylite_ast_explain_statement_view_format_kind(
                     explain_statement)),
             mylite_ast_explain_statement_view_has_analyze(explain_statement),
             mylite_ast_explain_statement_view_statement_start(
                 explain_statement),
             mylite_ast_explain_statement_view_statement_end(
                 explain_statement),
             mylite_ast_explain_statement_view_has_connection_id(
                 explain_statement),
             mylite_ast_explain_statement_view_connection_id(explain_statement),
             mylite_ast_explain_statement_view_connection_id_start(
                 explain_statement),
             mylite_ast_explain_statement_view_connection_id_end(
                 explain_statement),
             mylite_ast_explain_statement_view_table_start(explain_statement),
             mylite_ast_explain_statement_view_table_end(explain_statement),
             mylite_ast_explain_statement_view_column_start(explain_statement),
             mylite_ast_explain_statement_view_column_end(explain_statement),
             node_symbol_or_none(
                 mylite_ast_explain_statement_view_node(explain_statement)),
             node_symbol_or_none(
                 mylite_ast_explain_statement_view_statement_node(
                     explain_statement)));
      printf("    explain_table.schema len=%zu value=",
             mylite_ast_explain_statement_view_table_schema_value_length(
                 explain_statement));
      const char *schema =
          mylite_ast_explain_statement_view_table_schema_value(
              explain_statement);
      if (schema == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            schema,
            mylite_ast_explain_statement_view_table_schema_value_length(
                explain_statement));
      }
      printf(" name_len=%zu value=",
             mylite_ast_explain_statement_view_table_name_value_length(
                 explain_statement));
      const char *name =
          mylite_ast_explain_statement_view_table_name_value(explain_statement);
      if (name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            name, mylite_ast_explain_statement_view_table_name_value_length(
                      explain_statement));
      }
      printf(" column_len=%zu value=",
             mylite_ast_explain_statement_view_column_value_length(
                 explain_statement));
      const char *column =
          mylite_ast_explain_statement_view_column_value(explain_statement);
      if (column == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            column,
            mylite_ast_explain_statement_view_column_value_length(
                explain_statement));
      }
      fputc('\n', stdout);
    }
    if (show_statement != NULL) {
      printf("  show_statement span=%zu..%zu kind=%s scope=%s full=%d "
             "extended=%d count=%d target=%zu..%zu database=%zu..%zu "
             "table=%zu..%zu like=%zu..%zu where=%zu..%zu limit=%zu..%zu "
             "node=%s target_node=%s\n",
             mylite_ast_show_statement_view_start(show_statement),
             mylite_ast_show_statement_view_end(show_statement),
             mylite_show_statement_kind_name(
                 mylite_ast_show_statement_view_kind(show_statement)),
             mylite_show_scope_name(
                 mylite_ast_show_statement_view_scope(show_statement)),
             mylite_ast_show_statement_view_has_full(show_statement),
             mylite_ast_show_statement_view_has_extended(show_statement),
             mylite_ast_show_statement_view_has_count(show_statement),
             mylite_ast_show_statement_view_target_start(show_statement),
             mylite_ast_show_statement_view_target_end(show_statement),
             mylite_ast_show_statement_view_database_start(show_statement),
             mylite_ast_show_statement_view_database_end(show_statement),
             mylite_ast_show_statement_view_table_start(show_statement),
             mylite_ast_show_statement_view_table_end(show_statement),
             mylite_ast_show_statement_view_like_start(show_statement),
             mylite_ast_show_statement_view_like_end(show_statement),
             mylite_ast_show_statement_view_where_start(show_statement),
             mylite_ast_show_statement_view_where_end(show_statement),
             mylite_ast_show_statement_view_limit_start(show_statement),
             mylite_ast_show_statement_view_limit_end(show_statement),
             node_symbol_or_none(
                 mylite_ast_show_statement_view_node(show_statement)),
             node_symbol_or_none(
                 mylite_ast_show_statement_view_target_node(show_statement)));
      printf("    show.database len=%zu value=",
             mylite_ast_show_statement_view_database_value_length(
                 show_statement));
      const char *database =
          mylite_ast_show_statement_view_database_value(show_statement);
      if (database == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            database,
            mylite_ast_show_statement_view_database_value_length(
                show_statement));
      }
      printf(" table_schema_len=%zu value=",
             mylite_ast_show_statement_view_table_schema_value_length(
                 show_statement));
      const char *schema =
          mylite_ast_show_statement_view_table_schema_value(show_statement);
      if (schema == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            schema,
            mylite_ast_show_statement_view_table_schema_value_length(
                show_statement));
      }
      printf(" table_name_len=%zu value=",
             mylite_ast_show_statement_view_table_name_value_length(
                 show_statement));
      const char *name =
          mylite_ast_show_statement_view_table_name_value(show_statement);
      if (name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            name,
            mylite_ast_show_statement_view_table_name_value_length(
                show_statement));
      }
      printf(" like_len=%zu value=",
             mylite_ast_show_statement_view_like_value_length(show_statement));
      const char *like =
          mylite_ast_show_statement_view_like_value(show_statement);
      if (like == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            like,
            mylite_ast_show_statement_view_like_value_length(show_statement));
      }
      fputc('\n', stdout);
      if (mylite_ast_show_statement_view_like_expression(show_statement) !=
          NULL) {
        fputs("    show.like_expression\n", stdout);
        dump_expression_tree(
            mylite_ast_show_statement_view_like_expression(show_statement), 3);
      }
      if (mylite_ast_show_statement_view_where_expression(show_statement) !=
          NULL) {
        fputs("    show.where_expression\n", stdout);
        dump_expression_tree(
            mylite_ast_show_statement_view_where_expression(show_statement), 3);
      }
    }
    if (lock_statement != NULL) {
      printf("  lock_statement span=%zu..%zu kind=%s table_locks=%zu "
             "node=%s\n",
             mylite_ast_lock_statement_view_start(lock_statement),
             mylite_ast_lock_statement_view_end(lock_statement),
             mylite_lock_statement_kind_name(
                 mylite_ast_lock_statement_view_kind(lock_statement)),
             mylite_ast_lock_statement_view_table_lock_count(lock_statement),
             node_symbol_or_none(
                 mylite_ast_lock_statement_view_node(lock_statement)));
      for (size_t j = 0;
           j < mylite_ast_lock_statement_view_table_lock_count(lock_statement);
           j++) {
        const MyliteAstTableLock *table_lock =
            mylite_ast_lock_statement_view_table_lock_at(lock_statement, j);
        printf("    table_lock[%zu] span=%zu..%zu mode=%s table=%zu..%zu "
               "alias=%zu..%zu table=",
               j, mylite_ast_table_lock_view_start(table_lock),
               mylite_ast_table_lock_view_end(table_lock),
               mylite_table_lock_mode_name(
                   mylite_ast_table_lock_view_mode(table_lock)),
               mylite_ast_table_lock_view_table_start(table_lock),
               mylite_ast_table_lock_view_table_end(table_lock),
               mylite_ast_table_lock_view_alias_start(table_lock),
               mylite_ast_table_lock_view_alias_end(table_lock));
        const char *schema =
            mylite_ast_table_lock_view_table_schema_value(table_lock);
        size_t schema_length =
            mylite_ast_table_lock_view_table_schema_value_length(table_lock);
        if (schema != NULL) {
          print_escaped_bytes(schema, schema_length);
          fputc('.', stdout);
        }
        const char *name =
            mylite_ast_table_lock_view_table_name_value(table_lock);
        size_t name_length =
            mylite_ast_table_lock_view_table_name_value_length(table_lock);
        if (name == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(name, name_length);
        }
        fputs(" alias=", stdout);
        const char *alias = mylite_ast_table_lock_view_alias_value(table_lock);
        size_t alias_length =
            mylite_ast_table_lock_view_alias_value_length(table_lock);
        if (alias == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(alias, alias_length);
        }
        fputc('\n', stdout);
      }
    }
    if (maintenance_statement != NULL) {
      printf("  table_maintenance span=%zu..%zu kind=%s targets=%zu "
             "no_write_to_binlog=%d quick=%d extended=%d changed=%d fast=%d "
             "medium=%d use_frm=%d node=%s\n",
             mylite_ast_table_maintenance_statement_view_start(
                 maintenance_statement),
             mylite_ast_table_maintenance_statement_view_end(
                 maintenance_statement),
             mylite_table_maintenance_kind_name(
                 mylite_ast_table_maintenance_statement_view_kind(
                     maintenance_statement)),
             mylite_ast_table_maintenance_statement_view_target_count(
                 maintenance_statement),
             mylite_ast_table_maintenance_statement_view_has_no_write_to_binlog(
                 maintenance_statement),
             mylite_ast_table_maintenance_statement_view_has_quick(
                 maintenance_statement),
             mylite_ast_table_maintenance_statement_view_has_extended(
                 maintenance_statement),
             mylite_ast_table_maintenance_statement_view_has_changed(
                 maintenance_statement),
             mylite_ast_table_maintenance_statement_view_has_fast(
                 maintenance_statement),
             mylite_ast_table_maintenance_statement_view_has_medium(
                 maintenance_statement),
             mylite_ast_table_maintenance_statement_view_has_use_frm(
                 maintenance_statement),
             node_symbol_or_none(
                 mylite_ast_table_maintenance_statement_view_node(
                     maintenance_statement)));
      for (size_t j = 0;
           j < mylite_ast_table_maintenance_statement_view_target_count(
                   maintenance_statement);
           j++) {
        const MyliteAstTableMaintenanceTarget *target =
            mylite_ast_table_maintenance_statement_view_target_at(
                maintenance_statement, j);
        printf("    maintenance_target[%zu] span=%zu..%zu table=", j,
               mylite_ast_table_maintenance_target_view_start(target),
               mylite_ast_table_maintenance_target_view_end(target));
        const char *schema =
            mylite_ast_table_maintenance_target_view_schema_value(target);
        size_t schema_length =
            mylite_ast_table_maintenance_target_view_schema_value_length(
                target);
        if (schema != NULL) {
          print_escaped_bytes(schema, schema_length);
          fputc('.', stdout);
        }
        const char *name =
            mylite_ast_table_maintenance_target_view_name_value(target);
        size_t name_length =
            mylite_ast_table_maintenance_target_view_name_value_length(target);
        if (name == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(name, name_length);
        }
        fputc('\n', stdout);
      }
    }
    if (kill_statement != NULL) {
      printf("  kill_statement span=%zu..%zu kind=%s target_kind=%s "
             "tidb=%d target=%zu..%zu has_connection_id=%d "
             "connection_id=%llu node=%s value_len=%zu value=",
             mylite_ast_kill_statement_view_start(kill_statement),
             mylite_ast_kill_statement_view_end(kill_statement),
             mylite_kill_statement_kind_name(
                 mylite_ast_kill_statement_view_kind(kill_statement)),
             mylite_kill_target_kind_name(
                 mylite_ast_kill_statement_view_target_kind(kill_statement)),
             mylite_ast_kill_statement_view_has_tidb_extension(
                 kill_statement),
             mylite_ast_kill_statement_view_target_start(kill_statement),
             mylite_ast_kill_statement_view_target_end(kill_statement),
             mylite_ast_kill_statement_view_has_connection_id(kill_statement),
             mylite_ast_kill_statement_view_connection_id(kill_statement),
             node_symbol_or_none(
                 mylite_ast_kill_statement_view_node(kill_statement)),
             mylite_ast_kill_statement_view_target_value_length(
                 kill_statement));
      const char *target_value =
          mylite_ast_kill_statement_view_target_value(kill_statement);
      if (target_value == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            target_value,
            mylite_ast_kill_statement_view_target_value_length(
                kill_statement));
      }
      fputc('\n', stdout);
      const MyliteAstExpression *target_expression =
          mylite_ast_kill_statement_view_target_expression(kill_statement);
      if (target_expression != NULL) {
        dump_expression_tree(target_expression, 3);
      }
    }
    if (flush_statement != NULL) {
      printf("  flush_statement span=%zu..%zu kind=%s log=%s "
             "no_write_to_binlog=%d local_alias=%d read_lock=%d "
             "for_export=%d cluster=%d targets=%zu plugins=%zu node=%s\n",
             mylite_ast_flush_statement_view_start(flush_statement),
             mylite_ast_flush_statement_view_end(flush_statement),
             mylite_flush_statement_kind_name(
                 mylite_ast_flush_statement_view_kind(flush_statement)),
             mylite_flush_log_kind_name(
                 mylite_ast_flush_statement_view_log_kind(flush_statement)),
             mylite_ast_flush_statement_view_has_no_write_to_binlog(
                 flush_statement),
             mylite_ast_flush_statement_view_uses_local_alias(flush_statement),
             mylite_ast_flush_statement_view_has_read_lock(flush_statement),
             mylite_ast_flush_statement_view_has_for_export(flush_statement),
             mylite_ast_flush_statement_view_is_cluster(flush_statement),
             mylite_ast_flush_statement_view_target_count(flush_statement),
             mylite_ast_flush_statement_view_plugin_count(flush_statement),
             node_symbol_or_none(
                 mylite_ast_flush_statement_view_node(flush_statement)));
      for (size_t j = 0;
           j < mylite_ast_flush_statement_view_target_count(flush_statement);
           j++) {
        const MyliteAstFlushTarget *target =
            mylite_ast_flush_statement_view_target_at(flush_statement, j);
        printf("    flush_target[%zu] span=%zu..%zu kind=%s wildcard=%d "
               "target=",
               j, mylite_ast_flush_target_view_start(target),
               mylite_ast_flush_target_view_end(target),
               mylite_flush_target_kind_name(
                   mylite_ast_flush_target_view_kind(target)),
               mylite_ast_flush_target_view_has_wildcard(target));
        const char *schema = mylite_ast_flush_target_view_schema_value(target);
        size_t schema_length =
            mylite_ast_flush_target_view_schema_value_length(target);
        if (schema != NULL) {
          print_escaped_bytes(schema, schema_length);
          fputc('.', stdout);
        }
        const char *name = mylite_ast_flush_target_view_name_value(target);
        size_t name_length =
            mylite_ast_flush_target_view_name_value_length(target);
        if (name == NULL) {
          fputs(mylite_ast_flush_target_view_has_wildcard(target) ? "*" :
                                                               "none",
                stdout);
        } else {
          print_escaped_bytes(name, name_length);
        }
        fputc('\n', stdout);
      }
      for (size_t j = 0;
           j < mylite_ast_flush_statement_view_plugin_count(flush_statement);
           j++) {
        const MyliteAstFlushPlugin *plugin =
            mylite_ast_flush_statement_view_plugin_at(flush_statement, j);
        printf("    flush_plugin[%zu] span=%zu..%zu name_len=%zu name=", j,
               mylite_ast_flush_plugin_view_start(plugin),
               mylite_ast_flush_plugin_view_end(plugin),
               mylite_ast_flush_plugin_view_name_value_length(plugin));
        const char *name = mylite_ast_flush_plugin_view_name_value(plugin);
        if (name == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(
              name, mylite_ast_flush_plugin_view_name_value_length(plugin));
        }
        fputc('\n', stdout);
      }
    }
    if (load_statement != NULL) {
      printf("  load_statement span=%zu..%zu kind=%s duplicate=%s "
             "low_priority=%d local=%d partition=%d fields=%d columns_kw=%d "
             "lines=%d ignore_rows=%d:%llu items=%zu assignments=%zu "
             "options=%zu node=%s file=",
             mylite_ast_load_statement_view_start(load_statement),
             mylite_ast_load_statement_view_end(load_statement),
             mylite_load_statement_kind_name(
                 mylite_ast_load_statement_view_kind(load_statement)),
             mylite_load_duplicate_kind_name(
                 mylite_ast_load_statement_view_duplicate_kind(
                     load_statement)),
             mylite_ast_load_statement_view_has_low_priority(load_statement),
             mylite_ast_load_statement_view_has_local(load_statement),
             mylite_ast_load_statement_view_has_partition(load_statement),
             mylite_ast_load_statement_view_has_fields_clause(load_statement),
             mylite_ast_load_statement_view_uses_columns_keyword(
                 load_statement),
             mylite_ast_load_statement_view_has_lines_clause(load_statement),
             mylite_ast_load_statement_view_has_ignore_rows(load_statement),
             mylite_ast_load_statement_view_ignore_rows(load_statement),
             mylite_ast_load_statement_view_item_count(load_statement),
             mylite_ast_load_statement_view_assignment_count(load_statement),
             mylite_ast_load_statement_view_option_count(load_statement),
             node_symbol_or_none(
                 mylite_ast_load_statement_view_node(load_statement)));
      const char *file =
          mylite_ast_load_statement_view_file_value(load_statement);
      if (file == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            file,
            mylite_ast_load_statement_view_file_value_length(load_statement));
      }
      fputs(" table=", stdout);
      const char *schema =
          mylite_ast_load_statement_view_table_schema_value(load_statement);
      if (schema != NULL) {
        print_escaped_bytes(
            schema,
            mylite_ast_load_statement_view_table_schema_value_length(
                load_statement));
        fputc('.', stdout);
      }
      const char *table =
          mylite_ast_load_statement_view_table_name_value(load_statement);
      if (table == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            table,
            mylite_ast_load_statement_view_table_name_value_length(
                load_statement));
      }
      fputs(" charset=", stdout);
      const char *charset =
          mylite_ast_load_statement_view_charset_value(load_statement);
      if (charset == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            charset,
            mylite_ast_load_statement_view_charset_value_length(
                load_statement));
      }
      fputs(" format=", stdout);
      const char *format =
          mylite_ast_load_statement_view_format_value(load_statement);
      if (format == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            format,
            mylite_ast_load_statement_view_format_value_length(
                load_statement));
      }
      fputs(" row_tag=", stdout);
      const char *row_tag =
          mylite_ast_load_statement_view_row_tag_value(load_statement);
      if (row_tag == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            row_tag,
            mylite_ast_load_statement_view_row_tag_value_length(
                load_statement));
      }
      fputc('\n', stdout);

      printf("    load_fields terminated_len=%zu enclosed_len=%zu "
             "enclosed_optional=%d escaped_len=%zu defined_null_len=%zu "
             "defined_null_optional=%d line_starting_len=%zu "
             "line_terminated_len=%zu\n",
             mylite_ast_load_statement_view_field_terminated_value_length(
                 load_statement),
             mylite_ast_load_statement_view_field_enclosed_value_length(
                 load_statement),
             mylite_ast_load_statement_view_field_enclosed_is_optional(
                 load_statement),
             mylite_ast_load_statement_view_field_escaped_value_length(
                 load_statement),
             mylite_ast_load_statement_view_field_defined_null_value_length(
                 load_statement),
             mylite_ast_load_statement_view_field_defined_null_is_optionally_enclosed(
                 load_statement),
             mylite_ast_load_statement_view_line_starting_value_length(
                 load_statement),
             mylite_ast_load_statement_view_line_terminated_value_length(
                 load_statement));

      for (size_t j = 0;
           j < mylite_ast_load_statement_view_item_count(load_statement);
           j++) {
        const MyliteAstLoadListItem *item =
            mylite_ast_load_statement_view_item_at(load_statement, j);
        printf("    load_item[%zu] span=%zu..%zu kind=%s value_len=%zu "
               "value=",
               j, mylite_ast_load_list_item_view_start(item),
               mylite_ast_load_list_item_view_end(item),
               mylite_load_list_item_kind_name(
                   mylite_ast_load_list_item_view_kind(item)),
               mylite_ast_load_list_item_view_value_length(item));
        const char *value = mylite_ast_load_list_item_view_value(item);
        if (value == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(
              value, mylite_ast_load_list_item_view_value_length(item));
        }
        fputc('\n', stdout);
      }
      for (size_t j = 0;
           j < mylite_ast_load_statement_view_assignment_count(load_statement);
           j++) {
        const MyliteAstLoadAssignment *assignment =
            mylite_ast_load_statement_view_assignment_at(load_statement, j);
        printf("    load_assignment[%zu] span=%zu..%zu column_len=%zu "
               "column=",
               j, mylite_ast_load_assignment_view_start(assignment),
               mylite_ast_load_assignment_view_end(assignment),
               mylite_ast_load_assignment_view_column_value_length(
                   assignment));
        const char *column =
            mylite_ast_load_assignment_view_column_value(assignment);
        if (column == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(
              column,
              mylite_ast_load_assignment_view_column_value_length(
                  assignment));
        }
        fputc('\n', stdout);
        const MyliteAstExpression *expression =
            mylite_ast_load_assignment_view_expression(assignment);
        if (expression != NULL) {
          dump_expression_tree(expression, 3);
        }
      }
      for (size_t j = 0;
           j < mylite_ast_load_statement_view_option_count(load_statement);
           j++) {
        const MyliteAstLoadOption *option =
            mylite_ast_load_statement_view_option_at(load_statement, j);
        printf("    load_option[%zu] span=%zu..%zu value_kind=%s name_len=%zu "
               "name=",
               j, mylite_ast_load_option_view_start(option),
               mylite_ast_load_option_view_end(option),
               mylite_load_option_value_kind_name(
                   mylite_ast_load_option_view_value_kind(option)),
               mylite_ast_load_option_view_name_value_length(option));
        const char *name = mylite_ast_load_option_view_name_value(option);
        if (name == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(
              name, mylite_ast_load_option_view_name_value_length(option));
        }
        fputc('\n', stdout);
        const MyliteAstExpression *expression =
            mylite_ast_load_option_view_value_expression(option);
        if (expression != NULL) {
          dump_expression_tree(expression, 3);
        }
      }
    }
    if (account_statement != NULL) {
      printf("  account_statement span=%zu..%zu kind=%s if_exists=%d "
             "if_not_exists=%d require=%d connection_options=%d "
             "password_or_lock=%d comment_or_attribute=%d resource_group=%d "
             "for_user=%d random_password=%d accounts=%zu node=%s "
             "password_len=%zu replacement_len=%zu\n",
             mylite_ast_account_statement_view_start(account_statement),
             mylite_ast_account_statement_view_end(account_statement),
             mylite_account_statement_kind_name(
                 mylite_ast_account_statement_view_kind(account_statement)),
             mylite_ast_account_statement_view_has_if_exists(
                 account_statement),
             mylite_ast_account_statement_view_has_if_not_exists(
                 account_statement),
             mylite_ast_account_statement_view_has_require_clause(
                 account_statement),
             mylite_ast_account_statement_view_has_connection_options(
                 account_statement),
             mylite_ast_account_statement_view_has_password_or_lock_options(
                 account_statement),
             mylite_ast_account_statement_view_has_comment_or_attribute(
                 account_statement),
             mylite_ast_account_statement_view_has_resource_group(
                 account_statement),
             mylite_ast_account_statement_view_has_for_user(account_statement),
             mylite_ast_account_statement_view_uses_random_password(
                 account_statement),
             mylite_ast_account_statement_view_account_count(
                 account_statement),
             node_symbol_or_none(
                 mylite_ast_account_statement_view_node(account_statement)),
             mylite_ast_account_statement_view_password_value_length(
                 account_statement),
             mylite_ast_account_statement_view_replacement_password_value_length(
                 account_statement));
      for (size_t j = 0;
           j < mylite_ast_account_statement_view_account_count(
                   account_statement);
           j++) {
        const MyliteAstAccount *account =
            mylite_ast_account_statement_view_account_at(account_statement, j);
        printf("    account[%zu] span=%zu..%zu current_user=%d host=%d "
               "auth=%s user=",
               j, mylite_ast_account_view_start(account),
               mylite_ast_account_view_end(account),
               mylite_ast_account_view_is_current_user(account),
               mylite_ast_account_view_has_explicit_host(account),
               mylite_account_auth_kind_name(
                   mylite_ast_account_view_auth_kind(account)));
        const char *user = mylite_ast_account_view_user_value(account);
        if (user == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(user,
                              mylite_ast_account_view_user_value_length(
                                  account));
        }
        fputs(" host=", stdout);
        const char *host = mylite_ast_account_view_host_value(account);
        if (host == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(host,
                              mylite_ast_account_view_host_value_length(
                                  account));
        }
        fputs(" plugin=", stdout);
        const char *plugin =
            mylite_ast_account_view_auth_plugin_value(account);
        if (plugin == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(
              plugin,
              mylite_ast_account_view_auth_plugin_value_length(account));
        }
        fputs(" auth_string_len=", stdout);
        printf("%zu hash_len=%zu replacement_len=%zu\n",
               mylite_ast_account_view_auth_string_value_length(account),
               mylite_ast_account_view_hash_string_value_length(account),
               mylite_ast_account_view_replacement_auth_string_value_length(
                   account));
      }
    }
    if (privilege_statement != NULL) {
      printf("  privilege_statement span=%zu..%zu kind=%s object=%s "
             "level=%s with_grant=%d resource_limits=%d require=%d "
             "items=%zu users=%zu node=%s level_schema=",
             mylite_ast_privilege_statement_view_start(privilege_statement),
             mylite_ast_privilege_statement_view_end(privilege_statement),
             mylite_privilege_statement_kind_name(
                 mylite_ast_privilege_statement_view_kind(privilege_statement)),
             mylite_privilege_object_type_name(
                 mylite_ast_privilege_statement_view_object_type(
                     privilege_statement)),
             mylite_privilege_level_kind_name(
                 mylite_ast_privilege_statement_view_level_kind(
                     privilege_statement)),
             mylite_ast_privilege_statement_view_has_with_grant_option(
                 privilege_statement),
             mylite_ast_privilege_statement_view_has_resource_limits(
                 privilege_statement),
             mylite_ast_privilege_statement_view_has_require_clause(
                 privilege_statement),
             mylite_ast_privilege_statement_view_item_count(
                 privilege_statement),
             mylite_ast_privilege_statement_view_user_count(
                 privilege_statement),
             node_symbol_or_none(
                 mylite_ast_privilege_statement_view_node(privilege_statement)));
      const char *level_schema =
          mylite_ast_privilege_statement_view_level_schema_value(
              privilege_statement);
      if (level_schema == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            level_schema,
            mylite_ast_privilege_statement_view_level_schema_value_length(
                privilege_statement));
      }
      fputs(" level_name=", stdout);
      const char *level_name =
          mylite_ast_privilege_statement_view_level_name_value(
              privilege_statement);
      if (level_name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            level_name,
            mylite_ast_privilege_statement_view_level_name_value_length(
                privilege_statement));
      }
      fputc('\n', stdout);
      for (size_t j = 0;
           j < mylite_ast_privilege_statement_view_item_count(
                   privilege_statement);
           j++) {
        const MyliteAstPrivilegeItem *item =
            mylite_ast_privilege_statement_view_item_at(privilege_statement, j);
        printf("    privilege_item[%zu] span=%zu..%zu kind=%s columns=%zu "
               "value=",
               j, mylite_ast_privilege_item_view_start(item),
               mylite_ast_privilege_item_view_end(item),
               mylite_privilege_item_kind_name(
                   mylite_ast_privilege_item_view_kind(item)),
               mylite_ast_privilege_item_view_column_count(item));
        const char *value = mylite_ast_privilege_item_view_value(item);
        if (value == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(value,
                              mylite_ast_privilege_item_view_value_length(item));
        }
        fputc('\n', stdout);
      }
      const MyliteAstAccount *proxy_user =
          mylite_ast_privilege_statement_view_proxy_user(privilege_statement);
      if (proxy_user != NULL) {
        printf("    proxy_user span=%zu..%zu user=",
               mylite_ast_account_view_start(proxy_user),
               mylite_ast_account_view_end(proxy_user));
        const char *user = mylite_ast_account_view_user_value(proxy_user);
        if (user == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(user,
                              mylite_ast_account_view_user_value_length(
                                  proxy_user));
        }
        fputs(" host=", stdout);
        const char *host = mylite_ast_account_view_host_value(proxy_user);
        if (host == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(host,
                              mylite_ast_account_view_host_value_length(
                                  proxy_user));
        }
        fputc('\n', stdout);
      }
      for (size_t j = 0;
           j < mylite_ast_privilege_statement_view_user_count(
                   privilege_statement);
           j++) {
        const MyliteAstAccount *user_account =
            mylite_ast_privilege_statement_view_user_at(privilege_statement, j);
        printf("    privilege_user[%zu] span=%zu..%zu auth=%s user=", j,
               mylite_ast_account_view_start(user_account),
               mylite_ast_account_view_end(user_account),
               mylite_account_auth_kind_name(
                   mylite_ast_account_view_auth_kind(user_account)));
        const char *user = mylite_ast_account_view_user_value(user_account);
        if (user == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(user,
                              mylite_ast_account_view_user_value_length(
                                  user_account));
        }
        fputs(" host=", stdout);
        const char *host = mylite_ast_account_view_host_value(user_account);
        if (host == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(host,
                              mylite_ast_account_view_host_value_length(
                                  user_account));
        }
        fputc('\n', stdout);
      }
    }
    if (role_statement != NULL) {
      printf("  role_statement span=%zu..%zu kind=%s option=%s for_user=%d "
             "using_roles=%d roles=%zu users=%zu node=%s\n",
             mylite_ast_role_statement_view_start(role_statement),
             mylite_ast_role_statement_view_end(role_statement),
             mylite_role_statement_kind_name(
                 mylite_ast_role_statement_view_kind(role_statement)),
             mylite_role_option_kind_name(
                 mylite_ast_role_statement_view_option_kind(role_statement)),
             mylite_ast_role_statement_view_has_for_user(role_statement),
             mylite_ast_role_statement_view_has_using_roles(role_statement),
             mylite_ast_role_statement_view_role_count(role_statement),
             mylite_ast_role_statement_view_user_count(role_statement),
             node_symbol_or_none(
                 mylite_ast_role_statement_view_node(role_statement)));
      for (size_t j = 0;
           j < mylite_ast_role_statement_view_role_count(role_statement); j++) {
        const MyliteAstRoleName *role =
            mylite_ast_role_statement_view_role_at(role_statement, j);
        printf("    role[%zu] span=%zu..%zu host=%d name=", j,
               mylite_ast_role_name_view_start(role),
               mylite_ast_role_name_view_end(role),
               mylite_ast_role_name_view_has_explicit_host(role));
        const char *name = mylite_ast_role_name_view_name_value(role);
        if (name == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(name,
                              mylite_ast_role_name_view_name_value_length(role));
        }
        fputs(" host=", stdout);
        const char *host = mylite_ast_role_name_view_host_value(role);
        if (host == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(host,
                              mylite_ast_role_name_view_host_value_length(role));
        }
        fputc('\n', stdout);
      }
      for (size_t j = 0;
           j < mylite_ast_role_statement_view_user_count(role_statement); j++) {
        const MyliteAstAccount *user_account =
            mylite_ast_role_statement_view_user_at(role_statement, j);
        printf("    role_user[%zu] span=%zu..%zu user=", j,
               mylite_ast_account_view_start(user_account),
               mylite_ast_account_view_end(user_account));
        const char *user = mylite_ast_account_view_user_value(user_account);
        if (user == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(user,
                              mylite_ast_account_view_user_value_length(
                                  user_account));
        }
        fputs(" host=", stdout);
        const char *host = mylite_ast_account_view_host_value(user_account);
        if (host == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(host,
                              mylite_ast_account_view_host_value_length(
                                  user_account));
        }
        fputc('\n', stdout);
      }
    }
    if (insert_statement != NULL) {
      printf("  insert_statement span=%zu..%zu source=%s priority=%s "
             "ignore=%d into=%d partition=%d odku=%d target=%zu..%zu "
             "schema=%zu..%zu name=%zu..%zu partition=%zu..%zu "
             "source_span=%zu..%zu columns=%zu rows=%zu values=%zu "
             "set_assignments=%zu duplicate_assignments=%zu node=%s "
             "source_node=%s select_source=%s table=",
             mylite_ast_insert_statement_view_start(insert_statement),
             mylite_ast_insert_statement_view_end(insert_statement),
             mylite_insert_source_kind_name(
                 mylite_ast_insert_statement_view_source_kind(
                     insert_statement)),
             mylite_insert_priority_name(
                 mylite_ast_insert_statement_view_priority(insert_statement)),
             mylite_ast_insert_statement_view_has_ignore(insert_statement),
             mylite_ast_insert_statement_view_has_into(insert_statement),
             mylite_ast_insert_statement_view_has_partition_clause(
                 insert_statement),
             mylite_ast_insert_statement_view_has_on_duplicate_key_update(
                 insert_statement),
             mylite_ast_insert_statement_view_target_start(insert_statement),
             mylite_ast_insert_statement_view_target_end(insert_statement),
             mylite_ast_insert_statement_view_target_schema_start(
                 insert_statement),
             mylite_ast_insert_statement_view_target_schema_end(
                 insert_statement),
             mylite_ast_insert_statement_view_target_name_start(
                 insert_statement),
             mylite_ast_insert_statement_view_target_name_end(
                 insert_statement),
             mylite_ast_insert_statement_view_partition_start(insert_statement),
             mylite_ast_insert_statement_view_partition_end(insert_statement),
             mylite_ast_insert_statement_view_source_start(insert_statement),
             mylite_ast_insert_statement_view_source_end(insert_statement),
             mylite_ast_insert_statement_view_column_count(insert_statement),
             mylite_ast_insert_statement_view_value_row_count(
                 insert_statement),
             mylite_ast_insert_statement_view_value_count(insert_statement),
             mylite_ast_insert_statement_view_set_assignment_count(
                 insert_statement),
             mylite_ast_insert_statement_view_duplicate_assignment_count(
                 insert_statement),
             node_symbol_or_none(
                 mylite_ast_insert_statement_view_node(insert_statement)),
             node_symbol_or_none(
                 mylite_ast_insert_statement_view_source_node(
                     insert_statement)),
             node_symbol_or_none(
                 mylite_ast_insert_statement_view_select_source_node(
                     insert_statement)));
      const char *schema =
          mylite_ast_insert_statement_view_target_schema_value(
              insert_statement);
      size_t schema_length =
          mylite_ast_insert_statement_view_target_schema_value_length(
              insert_statement);
      if (schema != NULL) {
        print_escaped_bytes(schema, schema_length);
        fputc('.', stdout);
      }
      const char *name =
          mylite_ast_insert_statement_view_target_name_value(insert_statement);
      size_t name_length =
          mylite_ast_insert_statement_view_target_name_value_length(
              insert_statement);
      if (name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(name, name_length);
      }
      fputc('\n', stdout);

      for (size_t j = 0;
           j < mylite_ast_insert_statement_view_column_count(insert_statement);
           j++) {
        const MyliteAstInsertColumn *column =
            mylite_ast_insert_statement_view_column_at(insert_statement, j);
        printf("    insert_column[%zu] span=%zu..%zu name_len=%zu name=", j,
               mylite_ast_insert_column_view_start(column),
               mylite_ast_insert_column_view_end(column),
               mylite_ast_insert_column_view_name_value_length(column));
        const char *column_name =
            mylite_ast_insert_column_view_name_value(column);
        if (column_name == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(
              column_name,
              mylite_ast_insert_column_view_name_value_length(column));
        }
        fputc('\n', stdout);
      }

      for (size_t j = 0;
           j < mylite_ast_insert_statement_view_value_count(insert_statement);
           j++) {
        const MyliteAstInsertValue *value =
            mylite_ast_insert_statement_view_value_at(insert_statement, j);
        printf("    insert_value[%zu] row=%zu index=%zu span=%zu..%zu "
               "default=%d node=%s\n",
               j, mylite_ast_insert_value_view_row_index(value),
               mylite_ast_insert_value_view_value_index(value),
               mylite_ast_insert_value_view_start(value),
               mylite_ast_insert_value_view_end(value),
               mylite_ast_insert_value_view_is_default(value),
               node_symbol_or_none(mylite_ast_insert_value_view_node(value)));
        const MyliteAstExpression *expression =
            mylite_ast_insert_value_view_expression(value);
        if (expression != NULL) {
          printf("      insert_value[%zu].expression\n", j);
          dump_expression_tree(expression, 4);
        }
      }

      for (size_t j = 0;
           j < mylite_ast_insert_statement_view_set_assignment_count(
                   insert_statement);
           j++) {
        const MyliteAstInsertAssignment *assignment =
            mylite_ast_insert_statement_view_set_assignment_at(
                insert_statement, j);
        printf("    insert_set_assignment[%zu] span=%zu..%zu name=%zu..%zu "
               "value=%zu..%zu name_len=%zu name=",
               j, mylite_ast_insert_assignment_view_start(assignment),
               mylite_ast_insert_assignment_view_end(assignment),
               mylite_ast_insert_assignment_view_name_start(assignment),
               mylite_ast_insert_assignment_view_name_end(assignment),
               mylite_ast_insert_assignment_view_value_start(assignment),
               mylite_ast_insert_assignment_view_value_end(assignment),
               mylite_ast_insert_assignment_view_name_value_length(
                   assignment));
        const char *assignment_name =
            mylite_ast_insert_assignment_view_name_value(assignment);
        if (assignment_name == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(
              assignment_name,
              mylite_ast_insert_assignment_view_name_value_length(
                  assignment));
        }
        fputc('\n', stdout);
        const MyliteAstExpression *expression =
            mylite_ast_insert_assignment_view_value_expression(assignment);
        if (expression != NULL) {
          printf("      insert_set_assignment[%zu].expression\n", j);
          dump_expression_tree(expression, 4);
        }
      }

      for (size_t j = 0;
           j < mylite_ast_insert_statement_view_duplicate_assignment_count(
                   insert_statement);
           j++) {
        const MyliteAstInsertAssignment *assignment =
            mylite_ast_insert_statement_view_duplicate_assignment_at(
                insert_statement, j);
        printf("    insert_duplicate_assignment[%zu] span=%zu..%zu "
               "name=%zu..%zu value=%zu..%zu name_len=%zu name=",
               j, mylite_ast_insert_assignment_view_start(assignment),
               mylite_ast_insert_assignment_view_end(assignment),
               mylite_ast_insert_assignment_view_name_start(assignment),
               mylite_ast_insert_assignment_view_name_end(assignment),
               mylite_ast_insert_assignment_view_value_start(assignment),
               mylite_ast_insert_assignment_view_value_end(assignment),
               mylite_ast_insert_assignment_view_name_value_length(
                   assignment));
        const char *assignment_name =
            mylite_ast_insert_assignment_view_name_value(assignment);
        if (assignment_name == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(
              assignment_name,
              mylite_ast_insert_assignment_view_name_value_length(
                  assignment));
        }
        fputc('\n', stdout);
        const MyliteAstExpression *expression =
            mylite_ast_insert_assignment_view_value_expression(assignment);
        if (expression != NULL) {
          printf("      insert_duplicate_assignment[%zu].expression\n", j);
          dump_expression_tree(expression, 4);
        }
      }
    }
    if (call_statement != NULL) {
      printf("  call_statement span=%zu..%zu routine=%zu..%zu "
             "schema=%zu..%zu name=%zu..%zu parentheses=%d "
             "argument_list=%zu..%zu arguments=%zu node=%s\n",
             mylite_ast_call_statement_view_start(call_statement),
             mylite_ast_call_statement_view_end(call_statement),
             mylite_ast_call_statement_view_routine_start(call_statement),
             mylite_ast_call_statement_view_routine_end(call_statement),
             mylite_ast_call_statement_view_routine_schema_start(
                 call_statement),
             mylite_ast_call_statement_view_routine_schema_end(call_statement),
             mylite_ast_call_statement_view_routine_name_start(call_statement),
             mylite_ast_call_statement_view_routine_name_end(call_statement),
             mylite_ast_call_statement_view_has_parentheses(call_statement),
             mylite_ast_call_statement_view_argument_list_start(
                 call_statement),
             mylite_ast_call_statement_view_argument_list_end(call_statement),
             mylite_ast_call_statement_view_argument_count(call_statement),
             node_symbol_or_none(
                 mylite_ast_call_statement_view_node(call_statement)));
      printf("    call_routine.schema len=%zu value=",
             mylite_ast_call_statement_view_routine_schema_value_length(
                 call_statement));
      const char *schema =
          mylite_ast_call_statement_view_routine_schema_value(call_statement);
      if (schema == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            schema,
            mylite_ast_call_statement_view_routine_schema_value_length(
                call_statement));
      }
      printf(" name_len=%zu value=",
             mylite_ast_call_statement_view_routine_name_value_length(
                 call_statement));
      const char *name =
          mylite_ast_call_statement_view_routine_name_value(call_statement);
      if (name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            name, mylite_ast_call_statement_view_routine_name_value_length(
                      call_statement));
      }
      fputc('\n', stdout);
      for (size_t j = 0;
           j < mylite_ast_call_statement_view_argument_count(call_statement);
           j++) {
        const MyliteAstCallArgument *argument =
            mylite_ast_call_statement_view_argument_at(call_statement, j);
        printf("    call_argument[%zu] span=%zu..%zu\n", j,
               mylite_ast_call_argument_view_start(argument),
               mylite_ast_call_argument_view_end(argument));
        const MyliteAstExpression *expression =
            mylite_ast_call_argument_view_expression(argument);
        if (expression != NULL) {
          printf("      call_argument[%zu].expression\n", j);
          dump_expression_tree(expression, 4);
        }
      }
    }
    if (do_statement != NULL) {
      printf("  do_statement span=%zu..%zu expression_list=%zu..%zu "
             "expressions=%zu node=%s\n",
             mylite_ast_do_statement_view_start(do_statement),
             mylite_ast_do_statement_view_end(do_statement),
             mylite_ast_do_statement_view_expression_list_start(do_statement),
             mylite_ast_do_statement_view_expression_list_end(do_statement),
             mylite_ast_do_statement_view_expression_count(do_statement),
             node_symbol_or_none(
                 mylite_ast_do_statement_view_node(do_statement)));
      for (size_t j = 0;
           j < mylite_ast_do_statement_view_expression_count(do_statement);
           j++) {
        const MyliteAstDoExpression *expression =
            mylite_ast_do_statement_view_expression_at(do_statement, j);
        printf("    do_expression[%zu] span=%zu..%zu\n", j,
               mylite_ast_do_expression_view_start(expression),
               mylite_ast_do_expression_view_end(expression));
        const MyliteAstExpression *expression_view =
            mylite_ast_do_expression_view_expression(expression);
        if (expression_view != NULL) {
          printf("      do_expression[%zu].expression\n", j);
          dump_expression_tree(expression_view, 4);
        }
      }
    }
    if (replace_statement != NULL) {
      printf("  replace_statement span=%zu..%zu source=%s priority=%s "
             "into=%d target=%zu..%zu schema=%zu..%zu name=%zu..%zu "
             "partition=%zu..%zu source_span=%zu..%zu columns=%zu rows=%zu "
             "values=%zu set_assignments=%zu node=%s\n",
             mylite_ast_replace_statement_view_start(replace_statement),
             mylite_ast_replace_statement_view_end(replace_statement),
             mylite_replace_source_kind_name(
                 mylite_ast_replace_statement_view_source_kind(
                     replace_statement)),
             mylite_replace_priority_name(
                 mylite_ast_replace_statement_view_priority(
                     replace_statement)),
             mylite_ast_replace_statement_view_has_into(replace_statement),
             mylite_ast_replace_statement_view_target_start(replace_statement),
             mylite_ast_replace_statement_view_target_end(replace_statement),
             mylite_ast_replace_statement_view_target_schema_start(
                 replace_statement),
             mylite_ast_replace_statement_view_target_schema_end(
                 replace_statement),
             mylite_ast_replace_statement_view_target_name_start(
                 replace_statement),
             mylite_ast_replace_statement_view_target_name_end(
                 replace_statement),
             mylite_ast_replace_statement_view_partition_start(
                 replace_statement),
             mylite_ast_replace_statement_view_partition_end(
                 replace_statement),
             mylite_ast_replace_statement_view_source_start(
                 replace_statement),
             mylite_ast_replace_statement_view_source_end(replace_statement),
             mylite_ast_replace_statement_view_column_count(replace_statement),
             mylite_ast_replace_statement_view_value_row_count(
                 replace_statement),
             mylite_ast_replace_statement_view_value_count(replace_statement),
             mylite_ast_replace_statement_view_set_assignment_count(
                 replace_statement),
             node_symbol_or_none(
                 mylite_ast_replace_statement_view_node(replace_statement)));
      printf("    replace_target.schema len=%zu value=",
             mylite_ast_replace_statement_view_target_schema_value_length(
                 replace_statement));
      const char *schema =
          mylite_ast_replace_statement_view_target_schema_value(
              replace_statement);
      if (schema == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            schema,
            mylite_ast_replace_statement_view_target_schema_value_length(
                replace_statement));
      }
      printf(" name_len=%zu value=",
             mylite_ast_replace_statement_view_target_name_value_length(
                 replace_statement));
      const char *name =
          mylite_ast_replace_statement_view_target_name_value(
              replace_statement);
      if (name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(
            name, mylite_ast_replace_statement_view_target_name_value_length(
                      replace_statement));
      }
      fputc('\n', stdout);

      for (size_t j = 0;
           j < mylite_ast_replace_statement_view_column_count(
                   replace_statement);
           j++) {
        const MyliteAstReplaceColumn *column =
            mylite_ast_replace_statement_view_column_at(replace_statement, j);
        printf("    replace_column[%zu] span=%zu..%zu name_len=%zu name=",
               j, mylite_ast_replace_column_view_start(column),
               mylite_ast_replace_column_view_end(column),
               mylite_ast_replace_column_view_name_value_length(column));
        const char *column_name =
            mylite_ast_replace_column_view_name_value(column);
        if (column_name == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(
              column_name,
              mylite_ast_replace_column_view_name_value_length(column));
        }
        fputc('\n', stdout);
      }

      for (size_t j = 0;
           j < mylite_ast_replace_statement_view_value_count(
                   replace_statement);
           j++) {
        const MyliteAstReplaceValue *value =
            mylite_ast_replace_statement_view_value_at(replace_statement, j);
        printf("    replace_value[%zu] row=%zu index=%zu span=%zu..%zu "
               "default=%d\n",
               j, mylite_ast_replace_value_view_row_index(value),
               mylite_ast_replace_value_view_value_index(value),
               mylite_ast_replace_value_view_start(value),
               mylite_ast_replace_value_view_end(value),
               mylite_ast_replace_value_view_is_default(value));
        const MyliteAstExpression *expression =
            mylite_ast_replace_value_view_expression(value);
        if (expression != NULL) {
          printf("      replace_value[%zu].expression\n", j);
          dump_expression_tree(expression, 4);
        }
      }

      for (size_t j = 0;
           j < mylite_ast_replace_statement_view_set_assignment_count(
                   replace_statement);
           j++) {
        const MyliteAstReplaceAssignment *assignment =
            mylite_ast_replace_statement_view_set_assignment_at(
                replace_statement, j);
        printf("    replace_set_assignment[%zu] span=%zu..%zu name=%zu..%zu "
               "value=%zu..%zu name_len=%zu name=",
               j, mylite_ast_replace_assignment_view_start(assignment),
               mylite_ast_replace_assignment_view_end(assignment),
               mylite_ast_replace_assignment_view_name_start(assignment),
               mylite_ast_replace_assignment_view_name_end(assignment),
               mylite_ast_replace_assignment_view_value_start(assignment),
               mylite_ast_replace_assignment_view_value_end(assignment),
               mylite_ast_replace_assignment_view_name_value_length(
                   assignment));
        const char *assignment_name =
            mylite_ast_replace_assignment_view_name_value(assignment);
        if (assignment_name == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(
              assignment_name,
              mylite_ast_replace_assignment_view_name_value_length(
                  assignment));
        }
        fputc('\n', stdout);
        const MyliteAstExpression *expression =
            mylite_ast_replace_assignment_view_value_expression(assignment);
        if (expression != NULL) {
          printf("      replace_set_assignment[%zu].expression\n", j);
          dump_expression_tree(expression, 4);
        }
      }
    }
    if (delete_statement != NULL) {
      printf("  delete_statement span=%zu..%zu with=%d kind=%s "
             "multi_table=%d priority=%s quick=%d ignore=%d "
             "targets=%zu target_list=%zu..%zu table_refs=%zu..%zu "
             "where=%zu..%zu order=%zu..%zu limit=%zu..%zu node=%s\n",
             mylite_ast_delete_statement_view_start(delete_statement),
             mylite_ast_delete_statement_view_end(delete_statement),
             mylite_ast_delete_statement_view_has_with_clause(
                 delete_statement),
             mylite_delete_statement_kind_name(
                 mylite_ast_delete_statement_view_kind(delete_statement)),
             mylite_ast_delete_statement_view_is_multi_table(
                 delete_statement),
             mylite_delete_priority_name(
                 mylite_ast_delete_statement_view_priority(delete_statement)),
             mylite_ast_delete_statement_view_has_quick(delete_statement),
             mylite_ast_delete_statement_view_has_ignore(delete_statement),
             mylite_ast_delete_statement_view_target_count(delete_statement),
             mylite_ast_delete_statement_view_target_list_start(
                 delete_statement),
             mylite_ast_delete_statement_view_target_list_end(
                 delete_statement),
             mylite_ast_delete_statement_view_table_reference_start(
                 delete_statement),
             mylite_ast_delete_statement_view_table_reference_end(
                 delete_statement),
             mylite_ast_delete_statement_view_where_start(delete_statement),
             mylite_ast_delete_statement_view_where_end(delete_statement),
             mylite_ast_delete_statement_view_order_by_start(delete_statement),
             mylite_ast_delete_statement_view_order_by_end(delete_statement),
             mylite_ast_delete_statement_view_limit_start(delete_statement),
             mylite_ast_delete_statement_view_limit_end(delete_statement),
             node_symbol_or_none(
                 mylite_ast_delete_statement_view_node(delete_statement)));
      const MyliteAstExpression *where_expression =
          mylite_ast_delete_statement_view_where_expression(delete_statement);
      if (where_expression != NULL) {
        fputs("    delete_statement.where_expression\n", stdout);
        dump_expression_tree(where_expression, 3);
      }
      for (size_t j = 0;
           j < mylite_ast_delete_statement_view_target_count(delete_statement);
           j++) {
        const MyliteAstDeleteTarget *target =
            mylite_ast_delete_statement_view_target_at(delete_statement, j);
        printf("    delete_target[%zu] span=%zu..%zu schema=%zu..%zu "
               "name=%zu..%zu wildcard=%d schema_len=%zu schema=",
               j, mylite_ast_delete_target_view_start(target),
               mylite_ast_delete_target_view_end(target),
               mylite_ast_delete_target_view_schema_start(target),
               mylite_ast_delete_target_view_schema_end(target),
               mylite_ast_delete_target_view_name_start(target),
               mylite_ast_delete_target_view_name_end(target),
               mylite_ast_delete_target_view_has_wildcard(target),
               mylite_ast_delete_target_view_schema_value_length(target));
        const char *schema =
            mylite_ast_delete_target_view_schema_value(target);
        if (schema == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(
              schema,
              mylite_ast_delete_target_view_schema_value_length(target));
        }
        printf(" name_len=%zu name=",
               mylite_ast_delete_target_view_name_value_length(target));
        const char *name = mylite_ast_delete_target_view_name_value(target);
        if (name == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(
              name, mylite_ast_delete_target_view_name_value_length(target));
        }
        fputc('\n', stdout);
      }
    }
    if (update_statement != NULL) {
      printf("  update_statement span=%zu..%zu with=%d multi_table=%d "
             "priority=%s ignore=%d table_refs=%zu..%zu assignments=%zu "
             "where=%zu..%zu order=%zu..%zu limit=%zu..%zu node=%s\n",
             mylite_ast_update_statement_view_start(update_statement),
             mylite_ast_update_statement_view_end(update_statement),
             mylite_ast_update_statement_view_has_with_clause(
                 update_statement),
             mylite_ast_update_statement_view_is_multi_table(
                 update_statement),
             mylite_update_priority_name(
                 mylite_ast_update_statement_view_priority(update_statement)),
             mylite_ast_update_statement_view_has_ignore(update_statement),
             mylite_ast_update_statement_view_table_reference_start(
                 update_statement),
             mylite_ast_update_statement_view_table_reference_end(
                 update_statement),
             mylite_ast_update_statement_view_assignment_count(
                 update_statement),
             mylite_ast_update_statement_view_where_start(update_statement),
             mylite_ast_update_statement_view_where_end(update_statement),
             mylite_ast_update_statement_view_order_by_start(update_statement),
             mylite_ast_update_statement_view_order_by_end(update_statement),
             mylite_ast_update_statement_view_limit_start(update_statement),
             mylite_ast_update_statement_view_limit_end(update_statement),
             node_symbol_or_none(
                 mylite_ast_update_statement_view_node(update_statement)));
      const MyliteAstExpression *where_expression =
          mylite_ast_update_statement_view_where_expression(update_statement);
      if (where_expression != NULL) {
        fputs("    update_statement.where_expression\n", stdout);
        dump_expression_tree(where_expression, 3);
      }
      for (size_t j = 0;
           j < mylite_ast_update_statement_view_assignment_count(
                   update_statement);
           j++) {
        const MyliteAstUpdateAssignment *assignment =
            mylite_ast_update_statement_view_assignment_at(update_statement, j);
        printf("    update_assignment[%zu] span=%zu..%zu name=%zu..%zu "
               "value=%zu..%zu name_len=%zu name=",
               j, mylite_ast_update_assignment_view_start(assignment),
               mylite_ast_update_assignment_view_end(assignment),
               mylite_ast_update_assignment_view_name_start(assignment),
               mylite_ast_update_assignment_view_name_end(assignment),
               mylite_ast_update_assignment_view_value_start(assignment),
               mylite_ast_update_assignment_view_value_end(assignment),
               mylite_ast_update_assignment_view_name_value_length(
                   assignment));
        const char *assignment_name =
            mylite_ast_update_assignment_view_name_value(assignment);
        if (assignment_name == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(
              assignment_name,
              mylite_ast_update_assignment_view_name_value_length(assignment));
        }
        fputc('\n', stdout);
        const MyliteAstExpression *expression =
            mylite_ast_update_assignment_view_value_expression(assignment);
        if (expression != NULL) {
          printf("      update_assignment[%zu].expression\n", j);
          dump_expression_tree(expression, 4);
        }
      }
    }
    if (select_statement != NULL) {
      printf("  select_statement span=%zu..%zu with=%d set_op=%d "
             "query_blocks=%zu projections=%zu from=%zu..%zu "
             "where=%zu..%zu group=%zu..%zu having=%zu..%zu "
             "order=%zu..%zu limit=%zu..%zu into=%zu..%zu lock=%zu..%zu "
             "node=%s\n",
             mylite_ast_select_statement_view_start(select_statement),
             mylite_ast_select_statement_view_end(select_statement),
             mylite_ast_select_statement_view_has_with_clause(
                 select_statement),
             mylite_ast_select_statement_view_has_set_operation(
                 select_statement),
             mylite_ast_select_statement_view_query_block_count(
                 select_statement),
             mylite_ast_select_statement_view_projection_count(
                 select_statement),
             mylite_ast_select_statement_view_from_start(select_statement),
             mylite_ast_select_statement_view_from_end(select_statement),
             mylite_ast_select_statement_view_where_start(select_statement),
             mylite_ast_select_statement_view_where_end(select_statement),
             mylite_ast_select_statement_view_group_by_start(select_statement),
             mylite_ast_select_statement_view_group_by_end(select_statement),
             mylite_ast_select_statement_view_having_start(select_statement),
             mylite_ast_select_statement_view_having_end(select_statement),
             mylite_ast_select_statement_view_order_by_start(select_statement),
             mylite_ast_select_statement_view_order_by_end(select_statement),
             mylite_ast_select_statement_view_limit_start(select_statement),
             mylite_ast_select_statement_view_limit_end(select_statement),
             mylite_ast_select_statement_view_into_start(select_statement),
             mylite_ast_select_statement_view_into_end(select_statement),
             mylite_ast_select_statement_view_lock_start(select_statement),
             mylite_ast_select_statement_view_lock_end(select_statement),
             node_symbol_or_none(
                 mylite_ast_select_statement_view_node(select_statement)));
      const MyliteAstExpression *where_expression =
          mylite_ast_select_statement_view_where_expression(select_statement);
      if (where_expression != NULL) {
        fputs("    select_statement.where_expression\n", stdout);
        dump_expression_tree(where_expression, 3);
      }
      const MyliteAstExpression *having_expression =
          mylite_ast_select_statement_view_having_expression(select_statement);
      if (having_expression != NULL) {
        fputs("    select_statement.having_expression\n", stdout);
        dump_expression_tree(having_expression, 3);
      }
      for (size_t j = 0;
           j < mylite_ast_select_statement_view_projection_count(
                   select_statement);
           j++) {
        const MyliteAstSelectProjection *projection =
            mylite_ast_select_statement_view_projection_at(select_statement, j);
        printf("    projection[%zu] kind=%s span=%zu..%zu expr=%zu..%zu "
               "alias=%zu..%zu qualifier=%zu..%zu alias_len=%zu value=",
               j,
               mylite_select_projection_kind_name(
                   mylite_ast_select_projection_view_kind(projection)),
               mylite_ast_select_projection_view_start(projection),
               mylite_ast_select_projection_view_end(projection),
               mylite_ast_select_projection_view_expression_start(projection),
               mylite_ast_select_projection_view_expression_end(projection),
               mylite_ast_select_projection_view_alias_start(projection),
               mylite_ast_select_projection_view_alias_end(projection),
               mylite_ast_select_projection_view_qualifier_start(projection),
               mylite_ast_select_projection_view_qualifier_end(projection),
               mylite_ast_select_projection_view_alias_value_length(
                   projection));
        const char *alias =
            mylite_ast_select_projection_view_alias_value(projection);
        if (alias == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(
              alias,
              mylite_ast_select_projection_view_alias_value_length(projection));
        }
        size_t qualifier_length =
            mylite_ast_select_projection_view_qualifier_value_length(
                projection);
        printf(" qualifier_len=%zu value=", qualifier_length);
        const char *qualifier =
            mylite_ast_select_projection_view_qualifier_value(projection);
        if (qualifier == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(qualifier, qualifier_length);
        }
        fputc('\n', stdout);
        const MyliteAstExpression *projection_expression =
            mylite_ast_select_projection_view_expression(projection);
        if (projection_expression != NULL) {
          printf("      projection[%zu].expression\n", j);
          dump_expression_tree(projection_expression, 4);
        }
      }
    }
    if (values_statement != NULL) {
      printf("  values_statement span=%zu..%zu row_list=%zu..%zu "
             "rows=%zu values=%zu order=%zu..%zu limit=%zu..%zu "
             "into=%zu..%zu lock=%zu..%zu node=%s\n",
             mylite_ast_values_statement_view_start(values_statement),
             mylite_ast_values_statement_view_end(values_statement),
             mylite_ast_values_statement_view_row_list_start(values_statement),
             mylite_ast_values_statement_view_row_list_end(values_statement),
             mylite_ast_values_statement_view_row_count(values_statement),
             mylite_ast_values_statement_view_value_count(values_statement),
             mylite_ast_values_statement_view_order_by_start(values_statement),
             mylite_ast_values_statement_view_order_by_end(values_statement),
             mylite_ast_values_statement_view_limit_start(values_statement),
             mylite_ast_values_statement_view_limit_end(values_statement),
             mylite_ast_values_statement_view_into_start(values_statement),
             mylite_ast_values_statement_view_into_end(values_statement),
             mylite_ast_values_statement_view_lock_start(values_statement),
             mylite_ast_values_statement_view_lock_end(values_statement),
             node_symbol_or_none(
                 mylite_ast_values_statement_view_node(values_statement)));
      for (size_t j = 0;
           j < mylite_ast_values_statement_view_value_count(values_statement);
           j++) {
        const MyliteAstValuesValue *value =
            mylite_ast_values_statement_view_value_at(values_statement, j);
        printf("    values_value[%zu] row=%zu value=%zu span=%zu..%zu "
               "default=%d\n",
               j, mylite_ast_values_value_view_row_index(value),
               mylite_ast_values_value_view_value_index(value),
               mylite_ast_values_value_view_start(value),
               mylite_ast_values_value_view_end(value),
               mylite_ast_values_value_view_is_default(value));
        const MyliteAstExpression *expression =
            mylite_ast_values_value_view_expression(value);
        if (expression != NULL) {
          printf("      values_value[%zu].expression\n", j);
          dump_expression_tree(expression, 4);
        }
      }
    }
    if (set_statement != NULL) {
      printf("  set_statement span=%zu..%zu form=%s assignments=%zu\n",
             mylite_ast_set_statement_view_start(set_statement),
             mylite_ast_set_statement_view_end(set_statement),
             mylite_set_statement_form_name(
                 mylite_ast_set_statement_view_form(set_statement)),
             mylite_ast_set_statement_view_assignment_count(set_statement));
      for (size_t j = 0;
           j < mylite_ast_set_statement_view_assignment_count(set_statement);
           j++) {
        const MyliteAstSetAssignment *assignment =
            mylite_ast_set_statement_view_assignment_at(set_statement, j);
        printf("    set_assignment[%zu] kind=%s scope=%s operator=%s "
               "span=%zu..%zu name=%zu..%zu value=%zu..%zu "
               "extend_value=%zu..%zu name_len=%zu name=",
               j,
               mylite_set_assignment_kind_name(
                   mylite_ast_set_assignment_view_kind(assignment)),
               mylite_set_variable_scope_name(
                   mylite_ast_set_assignment_view_scope(assignment)),
               mylite_set_assignment_operator_name(
                   mylite_ast_set_assignment_view_operator(assignment)),
               mylite_ast_set_assignment_view_start(assignment),
               mylite_ast_set_assignment_view_end(assignment),
               mylite_ast_set_assignment_view_name_start(assignment),
               mylite_ast_set_assignment_view_name_end(assignment),
               mylite_ast_set_assignment_view_value_start(assignment),
               mylite_ast_set_assignment_view_value_end(assignment),
               mylite_ast_set_assignment_view_extend_value_start(assignment),
               mylite_ast_set_assignment_view_extend_value_end(assignment),
               mylite_ast_set_assignment_view_name_value_length(assignment));
        const char *name =
            mylite_ast_set_assignment_view_name_value(assignment);
        size_t name_length =
            mylite_ast_set_assignment_view_name_value_length(assignment);
        if (name == NULL) {
          fputs("none", stdout);
        } else {
          print_escaped_bytes(name, name_length);
        }
        const MyliteAstExpression *expression =
            mylite_ast_set_assignment_view_value_expression(assignment);
        if (expression != NULL) {
          printf(" expr=%s literal=%s operator=%s "
                 "expr_span=%zu..%zu expr_operator=%zu..%zu "
                 "expr_children=%zu expr_value=%zu..%zu "
                 "expr_value_len=%zu expr_value=",
                 mylite_expression_kind_name(
                     mylite_ast_expression_view_kind(expression)),
                 mylite_expression_literal_kind_name(
                     mylite_ast_expression_view_literal_kind(expression)),
                 mylite_expression_operator_kind_name(
                     mylite_ast_expression_view_operator_kind(expression)),
                 mylite_ast_expression_view_start(expression),
                 mylite_ast_expression_view_end(expression),
                 mylite_ast_expression_view_operator_start(expression),
                 mylite_ast_expression_view_operator_end(expression),
                 mylite_ast_expression_view_child_count(expression),
                 mylite_ast_expression_view_value_start(expression),
                 mylite_ast_expression_view_value_end(expression),
                 mylite_ast_expression_view_value_length(expression));
          const char *expression_value =
              mylite_ast_expression_view_value(expression);
          size_t expression_value_length =
              mylite_ast_expression_view_value_length(expression);
          if (expression_value == NULL) {
            fputs("none", stdout);
          } else {
            print_escaped_bytes(expression_value, expression_value_length);
          }
        }
        fputc('\n', stdout);
        if (expression != NULL &&
            mylite_ast_expression_view_child_count(expression) > 0) {
          dump_expression_tree(expression, 3);
        }
      }
    }
    if (rename_table != NULL) {
      printf("  rename_table span=%zu..%zu pairs=%zu\n",
             mylite_ast_rename_table_view_start(rename_table),
             mylite_ast_rename_table_view_end(rename_table),
             mylite_ast_rename_table_view_pair_count(rename_table));
    }
    if (truncate_table != NULL) {
      printf("  truncate_table span=%zu..%zu table_keyword=%d table=",
             mylite_ast_truncate_table_view_start(truncate_table),
             mylite_ast_truncate_table_view_end(truncate_table),
             mylite_ast_truncate_table_view_has_table_keyword(truncate_table));
      const char *table_schema =
          mylite_ast_truncate_table_view_schema_value(truncate_table);
      size_t table_schema_length =
          mylite_ast_truncate_table_view_schema_value_length(truncate_table);
      if (table_schema != NULL) {
        print_escaped_bytes(table_schema, table_schema_length);
        fputc('.', stdout);
      }
      const char *table_name =
          mylite_ast_truncate_table_view_name_value(truncate_table);
      size_t table_name_length =
          mylite_ast_truncate_table_view_name_value_length(truncate_table);
      if (table_name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(table_name, table_name_length);
      }
      fputc('\n', stdout);
    }
    if (transaction_statement != NULL) {
      printf("  transaction_statement span=%zu..%zu kind=%s begin_form=%s "
             "begin_mode=%s access=%s consistent_snapshot=%d "
             "causal_consistency=%d work=%d chain=%d no_chain=%d "
             "release=%d no_release=%d savepoint_keyword=%d "
             "savepoint=%zu..%zu savepoint_len=%zu savepoint=",
             mylite_ast_transaction_statement_view_start(
                 transaction_statement),
             mylite_ast_transaction_statement_view_end(transaction_statement),
             mylite_transaction_statement_kind_name(
                 mylite_ast_transaction_statement_view_kind(
                     transaction_statement)),
             mylite_transaction_begin_form_name(
                 mylite_ast_transaction_statement_view_begin_form(
                     transaction_statement)),
             mylite_transaction_begin_mode_name(
                 mylite_ast_transaction_statement_view_begin_mode(
                     transaction_statement)),
             mylite_transaction_access_mode_name(
                 mylite_ast_transaction_statement_view_access_mode(
                     transaction_statement)),
             mylite_ast_transaction_statement_view_has_consistent_snapshot(
                 transaction_statement),
             mylite_ast_transaction_statement_view_has_causal_consistency(
                 transaction_statement),
             mylite_ast_transaction_statement_view_has_work_keyword(
                 transaction_statement),
             mylite_ast_transaction_statement_view_has_chain(
                 transaction_statement),
             mylite_ast_transaction_statement_view_has_no_chain(
                 transaction_statement),
             mylite_ast_transaction_statement_view_has_release(
                 transaction_statement),
             mylite_ast_transaction_statement_view_has_no_release(
                 transaction_statement),
             mylite_ast_transaction_statement_view_has_savepoint_keyword(
                 transaction_statement),
             mylite_ast_transaction_statement_view_savepoint_name_start(
                 transaction_statement),
             mylite_ast_transaction_statement_view_savepoint_name_end(
                 transaction_statement),
             mylite_ast_transaction_statement_view_savepoint_name_value_length(
                 transaction_statement));
      const char *savepoint =
          mylite_ast_transaction_statement_view_savepoint_name_value(
              transaction_statement);
      size_t savepoint_length =
          mylite_ast_transaction_statement_view_savepoint_name_value_length(
              transaction_statement);
      if (savepoint == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(savepoint, savepoint_length);
      }
      fputc('\n', stdout);
    }
    if (use_database != NULL) {
      printf("  use_database span=%zu..%zu name=%zu..%zu name_len=%zu name=",
             mylite_ast_use_database_view_start(use_database),
             mylite_ast_use_database_view_end(use_database),
             mylite_ast_use_database_view_name_start(use_database),
             mylite_ast_use_database_view_name_end(use_database),
             mylite_ast_use_database_view_name_value_length(use_database));
      const char *database_name =
          mylite_ast_use_database_view_name_value(use_database);
      size_t database_name_length =
          mylite_ast_use_database_view_name_value_length(use_database);
      if (database_name == NULL) {
        fputs("none", stdout);
      } else {
        print_escaped_bytes(database_name, database_name_length);
      }
      fputc('\n', stdout);
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

static void dump_expression_tree(const MyliteAstExpression *expression,
                                 unsigned depth) {
  if (expression == NULL) {
    return;
  }

  for (unsigned i = 0; i < depth; i++) {
    fputs("  ", stdout);
  }
  printf("expr_tree kind=%s literal=%s operator=%s span=%zu..%zu "
         "operator_span=%zu..%zu children=%zu value_len=%zu value=",
         mylite_expression_kind_name(
             mylite_ast_expression_view_kind(expression)),
         mylite_expression_literal_kind_name(
             mylite_ast_expression_view_literal_kind(expression)),
         mylite_expression_operator_kind_name(
             mylite_ast_expression_view_operator_kind(expression)),
         mylite_ast_expression_view_start(expression),
         mylite_ast_expression_view_end(expression),
         mylite_ast_expression_view_operator_start(expression),
         mylite_ast_expression_view_operator_end(expression),
         mylite_ast_expression_view_child_count(expression),
         mylite_ast_expression_view_value_length(expression));
  const char *value = mylite_ast_expression_view_value(expression);
  if (value == NULL) {
    fputs("none", stdout);
  } else {
    print_escaped_bytes(value, mylite_ast_expression_view_value_length(expression));
  }
  fputc('\n', stdout);

  for (size_t i = 0; i < mylite_ast_expression_view_child_count(expression);
       i++) {
    dump_expression_tree(mylite_ast_expression_view_child_at(expression, i),
                         depth + 1);
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
    const MyliteAstExpression *default_expression =
        mylite_ast_create_table_column_view_default_value_expression(column);
    if (default_expression != NULL) {
      printf("      create_table.column[%zu].default_expression\n", i);
      dump_expression_tree(default_expression, 4);
    }
    const MyliteAstExpression *on_update_expression =
        mylite_ast_create_table_column_view_on_update_value_expression(column);
    if (on_update_expression != NULL) {
      printf("      create_table.column[%zu].on_update_expression\n", i);
      dump_expression_tree(on_update_expression, 4);
    }
    const MyliteAstExpression *generated_expression =
        mylite_ast_create_table_column_view_generated_expression(column);
    if (generated_expression != NULL) {
      printf("      create_table.column[%zu].generated_expression\n", i);
      dump_expression_tree(generated_expression, 4);
    }
    const MyliteAstExpression *check_expression =
        mylite_ast_create_table_column_view_check_expression(column);
    if (check_expression != NULL) {
      printf("      create_table.column[%zu].check_expression\n", i);
      dump_expression_tree(check_expression, 4);
    }
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
    const MyliteAstExpression *check_expression =
        mylite_ast_create_table_key_view_check_expression(key);
    if (check_expression != NULL) {
      printf("      create_table.key[%zu].check_expression\n", i);
      dump_expression_tree(check_expression, 4);
    }
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
      const MyliteAstExpression *part_expression =
          mylite_ast_create_table_key_part_view_expression(part);
      if (part_expression != NULL) {
        printf("        create_table.key[%zu].column[%zu].expression\n", i, j);
        dump_expression_tree(part_expression, 5);
      }
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
