#include <mylite/mylite.h>

#include "mylite_file_format.h"
#include "mylite_internal.h"
#include "mylite_vfs.h"
#include "sqlite3.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    schemata_column_count = 6,
    tables_column_count = 21,
    columns_column_count = 22,
    statistics_column_count = 18,
    information_schema_view_count = 7,
    schemata_catalog_column = 0,
    schemata_name_column = 1,
    schemata_character_set_column = 2,
    schemata_collation_column = 3,
    schemata_sql_path_column = 4,
    schemata_encryption_column = 5,
    tables_catalog_column = 0,
    tables_schema_column = 1,
    tables_name_column = 2,
    tables_type_column = 3,
    tables_engine_column = 4,
    tables_version_column = 5,
    tables_rows_column = 7,
    tables_auto_increment_column = 13,
    tables_collation_column = 17,
    tables_comment_column = 20,
    columns_name_column = 3,
    columns_ordinal_column = 4,
    columns_default_column = 5,
    columns_nullable_column = 6,
    columns_data_type_column = 7,
    columns_character_max_length_column = 8,
    columns_character_octet_length_column = 9,
    columns_numeric_precision_column = 10,
    columns_numeric_scale_column = 11,
    columns_datetime_precision_column = 12,
    columns_character_set_column = 13,
    columns_collation_column = 14,
    columns_type_column = 15,
    columns_key_column = 16,
    columns_extra_column = 17,
    columns_comment_column = 19,
    columns_table_name_column = 2,
    show_collation_column_count = 7,
    show_collation_id_column = 2,
    show_collation_sortlen_column = 5,
    statistics_non_unique_column = 3,
    statistics_index_name_column = 5,
    statistics_seq_column = 6,
    statistics_column_name_column = 7,
    statistics_collation_column = 8,
    statistics_sub_part_column = 10,
    statistics_nullable_column = 12,
    statistics_index_type_column = 13,
    statistics_index_comment_column = 15,
    statistics_visible_column = 16,
    statistics_table_name_column = 2,
    information_schema_table_version = 10,
    simple_create_table_version = 10,
    simple_create_auto_increment = 10,
    simple_create_name_length = 20,
    simple_create_name_octet_length = 80,
    simple_create_amount_precision = 10,
    simple_create_column_count = 6,
    simple_create_statistics_count = 3,
    mysql_warning_no_database = 1046,
    mysql_warning_bad_null = 1048,
    mysql_warning_ambiguous_column = 1052,
    mysql_warning_unknown_column = 1054,
    mysql_warning_duplicate_entry = 1062,
    mysql_warning_multiple_primary = 1068,
    mysql_warning_key_column_missing = 1072,
    mysql_warning_invalid_null = 1138,
    mysql_warning_wrong_field_with_group = 1055,
    mysql_warning_nonunique_table = 1066,
    mysql_warning_invalid_group_function = 1111,
    mysql_warning_mix_group_function_fields = 1140,
    mysql_warning_wrong_usage = 1221,
    mysql_warning_wrong_number_of_columns = 1222,
    mysql_warning_not_supported_yet = 1235,
    mysql_warning_unknown = 1105,
    mysql_warning_incorrect_escape_arguments = 1210,
    mysql_warning_operand_columns = 1241,
    mysql_warning_subquery_no_1_row = 1242,
    mysql_warning_table_name_not_allowed = 1250,
    mysql_warning_deprecated_syntax = 1287,
    mysql_warning_truncated_wrong_value = 1292,
    mysql_warning_savepoint_does_not_exist = 1305,
    mysql_warning_no_default = 1364,
    mysql_warning_division_by_zero = 1365,
    mysql_warning_duplicate_index = 1831,
    mysql_warning_legacy_syntax_converted = 3005,
    mysql_warning_field_in_order_not_select = 3065,
    mysql_warning_using_other_handler = 3502,
    mysql_warning_primary_invisible = 3522,
};

struct expected_schemata_row {
    const char *schema_name;
    const char *character_set;
    const char *collation;
    const char *encryption;
};

struct sqlite_table_lookup {
    const char *path;
    const char *table_name;
};

struct show_variable_expectation {
    const char *sql;
    const char *variable_name;
    const char *value;
    const char *context;
};

struct show_status_numeric_expectation {
    const char *sql;
    const char *const *variable_names;
    int row_count;
    const char *context;
};

struct show_status_row_expectation {
    const char *variable_name;
    const char *value;
};

struct show_status_catalog_expectation {
    const char *sql;
    const struct show_status_row_expectation *rows;
    int row_count;
    const char *context;
};

struct sqlite_physical_value_expectation {
    const char *path;
    const char *physical_name;
    const char *expression;
    const char *tail;
    int expected_type;
    int64_t expected_int;
    const char *expected_text;
    const char *context;
};

struct expected_table_collation {
    const char *table_name;
    const char *collation;
};

struct expected_column_metadata {
    const char *name;
    const char *schema_name;
    const char *table_name;
    const char *origin_table_name;
    const char *origin_column_name;
};

struct expected_columns_row {
    const char *table_name;
    const char *column_name;
    int64_t ordinal_position;
    const char *column_default;
    const char *is_nullable;
    const char *data_type;
    const char *column_type;
    const char *column_key;
    const char *extra;
};

struct expected_statistics_row {
    const char *table_name;
    const char *index_name;
    int64_t seq_in_index;
    const char *column_name;
    int64_t non_unique;
    const char *collation;
    const char *sub_part;
    const char *index_comment;
    const char *visible;
};

struct expected_result_metadata {
    const char *name;
    const char *schema_name;
    const char *table_name;
    const char *origin_schema_name;
    const char *origin_table_name;
    const char *origin_column_name;
    uint64_t declared_length;
    int field_type;
    unsigned int decimals;
    unsigned int charset_id;
    unsigned int flags_set;
    unsigned int flags_clear;
    int nullable;
};

static int test_select_integer_literal(void);
static int test_select_integer_literal_with_semicolon(void);
static int test_expression_operator_foundation(void);
static int test_scalar_builtin_functions_execution(void);
static int test_case_expression_execution(void);
static int test_cast_expression_execution(void);
static int test_aggregate_grouping_execution(void);
static int test_schema_lifecycle(void);
static int test_character_set_collation_foundation(void);
static int test_core_metadata_catalog(void);
static int test_information_schema_engines_execution(void);
static int test_information_schema_character_sets_execution(void);
static int test_information_schema_collations_execution(void);
static int test_mylite_file_preamble_and_vfs_payload(void);
static int test_mylite_open_rejects_plain_sqlite(void);
static int test_unsupported_statement(void);
static int test_create_table_base_execution(void);
static int test_create_table_prepare_has_no_side_effects(void);
static int test_drop_table_base_execution(void);
static int test_create_drop_index_execution(void);
static int test_alter_table_column_operations_execution(void);
static int test_alter_table_key_operations_execution(void);
static int test_rename_table_execution(void);
static int test_truncate_table_execution(void);
static int test_show_variables_execution(void);
static int test_show_status_execution(void);
static int test_show_engines_execution(void);
static int test_show_character_set_execution(void);
static int test_show_collation_execution(void);
static int test_show_tables_execution(void);
static int test_show_columns_execution(void);
static int test_show_index_execution(void);
static int test_show_create_table_execution(void);
static int test_show_diagnostics_execution(void);
static int test_describe_table_execution(void);
static int test_insert_values_execution(void);
static int test_insert_set_execution(void);
static int test_replace_execution(void);
static int test_insert_ignore_execution(void);
static int test_insert_on_duplicate_key_update_execution(void);
static int test_select_table_core_execution(void);
static int test_inner_join_execution(void);
static int test_outer_join_execution(void);
static int test_select_distinct_execution(void);
static int test_union_query_expression_execution(void);
static int test_subquery_execution(void);
static int test_select_where_execution(void);
static int test_select_order_limit_offset_execution(void);
static int test_result_metadata_expression_labels_execution(void);
static int test_update_single_table_execution(void);
static int test_delete_single_table_execution(void);
static int test_transaction_statements_execution(void);
static int test_savepoint_execution(void);
static int test_parse_error(void);
static int prepare_sql(mylite_db *database, const char *sql, int expected_status,
                       mylite_stmt **out_stmt);
static int expect_no_stmt_handle(mylite_stmt **stmt, const char *context);
static int execute_sql(mylite_db *database, const char *sql, int expected_step_status);
static int expect_prepare_error(mylite_db *database, const char *sql, int expected_status,
                                const char *error_fragment, const char *context);
static int expect_exec_error(mylite_stmt *stmt, mylite_db *database, const char *error_fragment,
                             const char *context);
static int expect_savepoint_warning(mylite_db *database, const char *error_fragment,
                                    const char *context);
static int execute_sql_expect_done_affected(mylite_db *database, const char *sql,
                                            int64_t expected_affected_rows, const char *context);
static int expect_select_rows(mylite_db *database, const char *sql, const char *const *columns,
                              int column_count, const char *const *values, int row_count,
                              const char *context);
static int expect_show_variables_contains(mylite_db *database,
                                          const struct show_variable_expectation *expected);
static int expect_show_status_catalog_rows(mylite_db *database,
                                           const struct show_status_catalog_expectation *expected);
static int expect_show_status_numeric_rows(mylite_db *database,
                                           const struct show_status_numeric_expectation *expected);
static int expect_show_character_set_maxlen_int64(mylite_db *database, const char *sql,
                                                  int64_t expected, const char *context);
static int expect_show_collation_numeric_columns(mylite_db *database, const char *sql,
                                                 int64_t expected_id, int64_t expected_sortlen,
                                                 const char *context);
static int expect_select_row_count(mylite_db *database, const char *sql, int row_count,
                                   const char *context);
static int expect_information_schema_schemata_row(mylite_db *database,
                                                  const struct expected_schemata_row *expected);
static int expect_no_information_schema_schemata_row(mylite_db *database, const char *schema_name);
static int expect_information_schema_tables_views(mylite_db *database);
static int expect_information_schema_column_row(mylite_db *database,
                                                const struct expected_columns_row *expected);
static int expect_no_information_schema_table_schema_row(mylite_db *database,
                                                         const char *schema_name);
static int expect_no_information_schema_table_name_row(mylite_db *database, const char *table_name);
static int expect_no_information_schema_column_table_name_row(mylite_db *database,
                                                              const char *table_name);
static int expect_no_information_schema_column_row(mylite_db *database, const char *table_name,
                                                   const char *column_name);
static int expect_no_information_schema_statistics_table_name_row(mylite_db *database,
                                                                  const char *table_name);
static int expect_no_information_schema_statistics_column_row(mylite_db *database,
                                                              const char *table_name,
                                                              const char *column_name);
static int expect_information_schema_statistics_row(mylite_db *database,
                                                    const struct expected_statistics_row *expected);
static int expect_no_information_schema_statistics_index_row(mylite_db *database,
                                                             const char *table_name,
                                                             const char *index_name);
static int
expect_information_schema_table_collation(mylite_db *database,
                                          const struct expected_table_collation *expected);
static int expect_simple_create_table_row(mylite_db *database);
static int expect_simple_create_column_rows(mylite_db *database);
static int expect_simple_create_statistics_rows(mylite_db *database);
static int expect_empty_information_schema_table(mylite_db *database, const char *sql,
                                                 const char *const *columns, int column_count);
static int expect_show_database_rows(mylite_db *database, const char *required,
                                     const char *forbidden);
static int expect_connection_state(mylite_db *database, const char *client, const char *connection,
                                   const char *results, const char *collation, const char *context);
static int expect_column_names(const mylite_stmt *stmt, const char *const *expected, int count,
                               const char *context);
static int expect_column_metadata(const mylite_stmt *stmt,
                                  const struct expected_column_metadata *expected, int count,
                                  const char *context);
static int expect_result_metadata(const mylite_stmt *stmt,
                                  const struct expected_result_metadata *expected, int count,
                                  const char *context);
static char *expected_physical_table_name(const char *schema_name, const char *table_name);
static int expect_sqlite_table_exists(const struct sqlite_table_lookup *lookup);
static int expect_sqlite_table_missing(const struct sqlite_table_lookup *lookup);
static int expect_sqlite_physical_int64(const char *path, const char *physical_name,
                                        const char *expression, const char *tail, int64_t expected,
                                        const char *context);
static int expect_sqlite_physical_text(const char *path, const char *physical_name,
                                       const char *expression, const char *tail,
                                       const char *expected, const char *context);
static int expect_sqlite_physical_null(const char *path, const char *physical_name,
                                       const char *expression, const char *tail,
                                       const char *context);
static int expect_sqlite_physical_not_null(const char *path, const char *physical_name,
                                           const char *expression, const char *tail,
                                           const char *context);
static int expect_sqlite_physical_value(const struct sqlite_physical_value_expectation *expected);
static void remove_runtime_test_files(void);
static int read_file_at(const char *path, long offset, unsigned char *buffer, size_t size);
static int exec_sqlite(sqlite3 *database, const char *sql);
static int expect_default_sqlite_rejects_mylite(const char *path);
static int expect_sqlite_status(int actual, int expected, const char *context);
static int expect_status(int actual, int expected, const char *context);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_u16(unsigned int actual, unsigned int expected, const char *context);
static int expect_string(const char *actual, const char *expected, const char *context);
static int expect_unsigned_decimal_text(const char *actual, const char *context);
static int expect_null_text(const char *actual, const char *context);
static int expect_contains(const char *actual, const char *expected_fragment, const char *context);
static int expect_bytes(const unsigned char *actual, const void *expected, size_t size,
                        const char *context);

int main(void)
{
    int failures = 0;

    failures += test_select_integer_literal();
    failures += test_select_integer_literal_with_semicolon();
    failures += test_expression_operator_foundation();
    failures += test_scalar_builtin_functions_execution();
    failures += test_case_expression_execution();
    failures += test_cast_expression_execution();
    failures += test_aggregate_grouping_execution();
    failures += test_schema_lifecycle();
    failures += test_character_set_collation_foundation();
    failures += test_core_metadata_catalog();
    failures += test_information_schema_engines_execution();
    failures += test_information_schema_character_sets_execution();
    failures += test_information_schema_collations_execution();
    failures += test_mylite_file_preamble_and_vfs_payload();
    failures += test_mylite_open_rejects_plain_sqlite();
    failures += test_unsupported_statement();
    failures += test_create_table_base_execution();
    failures += test_create_table_prepare_has_no_side_effects();
    failures += test_drop_table_base_execution();
    failures += test_create_drop_index_execution();
    failures += test_alter_table_column_operations_execution();
    failures += test_alter_table_key_operations_execution();
    failures += test_rename_table_execution();
    failures += test_truncate_table_execution();
    failures += test_show_variables_execution();
    failures += test_show_status_execution();
    failures += test_show_engines_execution();
    failures += test_show_character_set_execution();
    failures += test_show_collation_execution();
    failures += test_show_tables_execution();
    failures += test_show_columns_execution();
    failures += test_show_index_execution();
    failures += test_show_create_table_execution();
    failures += test_show_diagnostics_execution();
    failures += test_describe_table_execution();
    failures += test_insert_values_execution();
    failures += test_insert_set_execution();
    failures += test_replace_execution();
    failures += test_insert_ignore_execution();
    failures += test_insert_on_duplicate_key_update_execution();
    failures += test_select_table_core_execution();
    failures += test_inner_join_execution();
    failures += test_outer_join_execution();
    failures += test_select_distinct_execution();
    failures += test_union_query_expression_execution();
    failures += test_subquery_execution();
    failures += test_select_where_execution();
    failures += test_select_order_limit_offset_execution();
    failures += test_result_metadata_expression_labels_execution();
    failures += test_update_single_table_execution();
    failures += test_delete_single_table_execution();
    failures += test_transaction_statements_execution();
    failures += test_savepoint_execution();
    failures += test_parse_error();

    return failures == 0 ? 0 : 1;
}

static int test_schema_lifecycle(void)
{
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open memory database");

    failures += prepare_sql(database, "SHOW DATABASES", MYLITE_OK, &stmt);
    failures += expect_int(mylite_column_count(stmt), 1, "show databases column count");
    failures += expect_string(mylite_column_name(stmt, 0), "Database", "show databases column");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "show databases first row");
    failures += expect_string(mylite_column_text(stmt, 0), "information_schema",
                              "show databases first schema");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += expect_show_database_rows(database, "mysql", "mylite_schema_lifecycle_a");
    failures += execute_sql(database,
                            "CREATE DATABASE mylite_schema_lifecycle_a DEFAULT CHARACTER SET "
                            "utf8mb4 COLLATE utf8mb4_bin ENCRYPTION='N'",
                            MYLITE_DONE);
    failures += expect_show_database_rows(database, "mylite_schema_lifecycle_a", NULL);

    failures +=
        prepare_sql(database, "CREATE DATABASE mylite_schema_lifecycle_a", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "duplicate create step");
    failures += expect_contains(mylite_error_message(database), "database exists",
                                "duplicate create error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        execute_sql(database, "CREATE SCHEMA IF NOT EXISTS mylite_schema_lifecycle_a", MYLITE_DONE);
    failures +=
        prepare_sql(database, "ALTER DATABASE DEFAULT CHARACTER SET utf8mb4", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "alter no default schema");
    failures += expect_contains(mylite_error_message(database), "No database selected",
                                "alter no default schema error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "USE mylite_schema_lifecycle_a", MYLITE_DONE);
    failures += prepare_sql(database, "USE mylite_schema_lifecycle_missing", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "use missing schema");
    failures += expect_contains(mylite_error_message(database), "Unknown database",
                                "use missing schema error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        execute_sql(database, "ALTER SCHEMA DEFAULT COLLATE utf8mb4_0900_ai_ci", MYLITE_DONE);
    failures += prepare_sql(database,
                            "ALTER DATABASE mylite_schema_lifecycle_missing DEFAULT CHARACTER SET "
                            "utf8mb4",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "alter missing schema");
    failures += expect_contains(mylite_error_message(database), "doesn't exist",
                                "alter missing schema error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "DROP DATABASE mylite_schema_lifecycle_a", MYLITE_DONE);
    failures += expect_show_database_rows(database, NULL, "mylite_schema_lifecycle_a");
    failures +=
        prepare_sql(database, "ALTER DATABASE DEFAULT COLLATE utf8mb4_bin", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "alter after selected schema drop");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        execute_sql(database, "DROP SCHEMA IF EXISTS mylite_schema_lifecycle_missing", MYLITE_DONE);
    failures +=
        prepare_sql(database, "DROP SCHEMA mylite_schema_lifecycle_missing", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "drop missing schema");
    failures += expect_contains(mylite_error_message(database), "database doesn't exist",
                                "drop missing schema error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "DROP DATABASE mysql", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "drop system schema");
    failures += expect_contains(mylite_error_message(database), "system schema",
                                "drop system schema error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "CREATE DATABASE `My``Schema`", MYLITE_DONE);
    failures += expect_show_database_rows(database, "My`Schema", NULL);
    failures += execute_sql(database, "DROP DATABASE `My``Schema`", MYLITE_DONE);

    failures += execute_sql(database,
                            "CREATE DATABASE encryption DEFAULT CHARSET 'utf8mb4' "
                            "COLLATE 'utf8mb4_bin' ENCRYPTION='y'",
                            MYLITE_DONE);
    failures += expect_show_database_rows(database, "encryption", NULL);
    failures += execute_sql(database, "DROP DATABASE encryption", MYLITE_DONE);

    failures += prepare_sql(database, "CREATE DATABASE invalid_encryption ENCRYPTION='X'",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "invalid encryption value");
    failures +=
        expect_contains(mylite_error_message(database), "Y or N", "invalid encryption error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "CREATE DATABASE read_only_value", MYLITE_DONE);
    failures += execute_sql(database, "USE read_only_value", MYLITE_DONE);
    failures += prepare_sql(database, "ALTER DATABASE READ ONLY = 2", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "invalid read only value");
    failures +=
        expect_contains(mylite_error_message(database), "READ ONLY", "invalid read only error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "DROP DATABASE read_only_value", MYLITE_DONE);

    mylite_close(database);
    return failures;
}

static int test_character_set_collation_foundation(void)
{
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open memory database");
    failures += expect_connection_state(database, "utf8mb4", "utf8mb4", "utf8mb4",
                                        "utf8mb4_0900_ai_ci", "initial connection charset");

    failures += execute_sql(database, "SET NAMES utf8mb4", MYLITE_DONE);
    failures += expect_connection_state(database, "utf8mb4", "utf8mb4", "utf8mb4",
                                        "utf8mb4_0900_ai_ci", "set names utf8mb4");
    failures += execute_sql(database, "SET NAMES latin1 COLLATE latin1_bin", MYLITE_DONE);
    failures += expect_connection_state(database, "latin1", "latin1", "latin1", "latin1_bin",
                                        "set names latin1 explicit collation");
    failures += execute_sql(database, "SET NAMES binary", MYLITE_DONE);
    failures += expect_connection_state(database, "binary", "binary", "binary", "binary",
                                        "set names binary");
    failures += execute_sql(database, "SET NAMES UTF8MB4 COLLATE UTF8MB4_BIN", MYLITE_DONE);
    failures += expect_connection_state(database, "utf8mb4", "utf8mb4", "utf8mb4", "utf8mb4_bin",
                                        "set names uppercase normalized");
    failures += execute_sql(database, "SET NAMES 'utf8mb3' COLLATE 'utf8mb3_bin'", MYLITE_DONE);
    failures += expect_connection_state(database, "utf8mb3", "utf8mb3", "utf8mb3", "utf8mb3_bin",
                                        "set names quoted utf8mb3");
    failures += execute_sql(database, "SET NAMES DEFAULT", MYLITE_DONE);
    failures += expect_connection_state(database, "utf8mb4", "utf8mb4", "utf8mb4",
                                        "utf8mb4_0900_ai_ci", "set names default");

    failures += execute_sql(database, "SET CHARACTER SET utf8mb3", MYLITE_DONE);
    failures +=
        expect_connection_state(database, "utf8mb3", "utf8mb4", "utf8mb3", "utf8mb4_0900_ai_ci",
                                "set character set no selected schema");
    failures += execute_sql(database, "SET CHARACTER SET binary", MYLITE_DONE);
    failures +=
        expect_connection_state(database, "binary", "utf8mb4", "binary", "utf8mb4_0900_ai_ci",
                                "set character set binary no selected schema");
    failures += execute_sql(database, "SET CHARSET 'latin1'", MYLITE_DONE);
    failures +=
        expect_connection_state(database, "latin1", "utf8mb4", "latin1", "utf8mb4_0900_ai_ci",
                                "set charset quoted no selected schema");
    failures += execute_sql(database, "SET CHARACTER SET DEFAULT", MYLITE_DONE);
    failures += expect_connection_state(database, "utf8mb4", "utf8mb4", "utf8mb4",
                                        "utf8mb4_0900_ai_ci", "set character set default");

    failures += execute_sql(database,
                            "CREATE DATABASE mylite_charset_session DEFAULT CHARACTER SET latin1 "
                            "COLLATE latin1_bin",
                            MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_charset_session", MYLITE_DONE);
    failures += execute_sql(database, "SET CHARACTER SET utf8mb4", MYLITE_DONE);
    failures += expect_connection_state(database, "utf8mb4", "latin1", "utf8mb4", "latin1_bin",
                                        "set character set selected schema default");
    failures += execute_sql(database, "SET NAMES utf8mb4 COLLATE utf8mb4_bin", MYLITE_DONE);
    failures += execute_sql(database, "SET CHARACTER SET DEFAULT", MYLITE_DONE);
    failures += expect_connection_state(database, "utf8mb4", "latin1", "utf8mb4", "latin1_bin",
                                        "set character set default selected schema");
    failures += execute_sql(database, "DROP DATABASE mylite_charset_session", MYLITE_DONE);

    failures += prepare_sql(database, "SET NAMES nosuchcharset", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "set names unknown charset");
    failures += expect_contains(mylite_error_message(database), "Unknown character set",
                                "set names unknown charset error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "SET NAMES utf8mb4 COLLATE nosuchcollation", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "set names unknown collation");
    failures += expect_contains(mylite_error_message(database), "Unknown collation",
                                "set names unknown collation error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SET NAMES utf8mb4 COLLATE latin1_bin", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "set names incompatible collation");
    failures += expect_contains(mylite_error_message(database), "not valid for CHARACTER SET",
                                "set names incompatible collation error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SET CHARACTER SET nosuchcharset", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "set character set unknown charset");
    failures += expect_contains(mylite_error_message(database), "Unknown character set",
                                "set character set unknown charset error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database,
                            "CREATE DATABASE mylite_charset_upper DEFAULT CHARACTER SET UTF8MB4 "
                            "COLLATE UTF8MB4_BIN",
                            MYLITE_DONE);
    failures +=
        expect_information_schema_schemata_row(database, &(const struct expected_schemata_row){
                                                             .schema_name = "mylite_charset_upper",
                                                             .character_set = "utf8mb4",
                                                             .collation = "utf8mb4_bin",
                                                             .encryption = "NO",
                                                         });
    failures += execute_sql(database, "DROP DATABASE mylite_charset_upper", MYLITE_DONE);

    failures += execute_sql(database, "CREATE DATABASE mylite_charset_collate COLLATE latin1_bin",
                            MYLITE_DONE);
    failures += expect_information_schema_schemata_row(database,
                                                       &(const struct expected_schemata_row){
                                                           .schema_name = "mylite_charset_collate",
                                                           .character_set = "latin1",
                                                           .collation = "latin1_bin",
                                                           .encryption = "NO",
                                                       });
    failures += execute_sql(database, "ALTER DATABASE mylite_charset_collate CHARACTER SET utf8mb3",
                            MYLITE_DONE);
    failures += expect_information_schema_schemata_row(database,
                                                       &(const struct expected_schemata_row){
                                                           .schema_name = "mylite_charset_collate",
                                                           .character_set = "utf8mb3",
                                                           .collation = "utf8mb3_general_ci",
                                                           .encryption = "NO",
                                                       });
    failures += execute_sql(database, "ALTER DATABASE mylite_charset_collate COLLATE latin1_bin",
                            MYLITE_DONE);
    failures += expect_information_schema_schemata_row(database,
                                                       &(const struct expected_schemata_row){
                                                           .schema_name = "mylite_charset_collate",
                                                           .character_set = "latin1",
                                                           .collation = "latin1_bin",
                                                           .encryption = "NO",
                                                       });
    failures += execute_sql(database, "DROP DATABASE mylite_charset_collate", MYLITE_DONE);

    failures += prepare_sql(database,
                            "CREATE DATABASE mylite_charset_bad DEFAULT CHARACTER SET "
                            "nosuchcharset",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "create unknown charset");
    failures += expect_contains(mylite_error_message(database), "Unknown character set",
                                "create unknown charset error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "CREATE DATABASE mylite_charset_bad DEFAULT CHARACTER SET utf8mb4 "
                            "COLLATE latin1_bin",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR,
                              "create incompatible charset collation");
    failures += expect_contains(mylite_error_message(database), "not valid for CHARACTER SET",
                                "create incompatible charset collation error");
    mylite_finalize(stmt);

    mylite_close(database);
    return failures;
}

static int test_core_metadata_catalog(void)
{
    static const char *const schemata_columns[] = {
        "CATALOG_NAME",           "SCHEMA_NAME", "DEFAULT_CHARACTER_SET_NAME",
        "DEFAULT_COLLATION_NAME", "SQL_PATH",    "DEFAULT_ENCRYPTION",
    };
    static const char *const tables_columns[] = {
        "TABLE_CATALOG",   "TABLE_SCHEMA", "TABLE_NAME",      "TABLE_TYPE",     "ENGINE",
        "VERSION",         "ROW_FORMAT",   "TABLE_ROWS",      "AVG_ROW_LENGTH", "DATA_LENGTH",
        "MAX_DATA_LENGTH", "INDEX_LENGTH", "DATA_FREE",       "AUTO_INCREMENT", "CREATE_TIME",
        "UPDATE_TIME",     "CHECK_TIME",   "TABLE_COLLATION", "CHECKSUM",       "CREATE_OPTIONS",
        "TABLE_COMMENT",
    };
    static const char *const columns_columns[] = {
        "TABLE_CATALOG",
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "COLUMN_DEFAULT",
        "IS_NULLABLE",
        "DATA_TYPE",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
        "NUMERIC_PRECISION",
        "NUMERIC_SCALE",
        "DATETIME_PRECISION",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "COLUMN_TYPE",
        "COLUMN_KEY",
        "EXTRA",
        "PRIVILEGES",
        "COLUMN_COMMENT",
        "GENERATION_EXPRESSION",
        "SRS_ID",
    };
    static const char *const statistics_columns[] = {
        "TABLE_CATALOG", "TABLE_SCHEMA", "TABLE_NAME",  "NON_UNIQUE", "INDEX_SCHEMA",
        "INDEX_NAME",    "SEQ_IN_INDEX", "COLUMN_NAME", "COLLATION",  "CARDINALITY",
        "SUB_PART",      "PACKED",       "NULLABLE",    "INDEX_TYPE", "COMMENT",
        "INDEX_COMMENT", "IS_VISIBLE",   "EXPRESSION",
    };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open memory database");

    failures +=
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.SCHEMATA", MYLITE_OK, &stmt);
    failures += expect_column_names(stmt, schemata_columns, schemata_column_count, "schemata");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        expect_information_schema_schemata_row(database, &(const struct expected_schemata_row){
                                                             .schema_name = "information_schema",
                                                             .character_set = "utf8mb3",
                                                             .collation = "utf8mb3_general_ci",
                                                             .encryption = "NO",
                                                         });
    failures +=
        expect_information_schema_schemata_row(database, &(const struct expected_schemata_row){
                                                             .schema_name = "mysql",
                                                             .character_set = "utf8mb4",
                                                             .collation = "utf8mb4_0900_ai_ci",
                                                             .encryption = "NO",
                                                         });
    failures +=
        expect_information_schema_schemata_row(database, &(const struct expected_schemata_row){
                                                             .schema_name = "performance_schema",
                                                             .character_set = "utf8mb4",
                                                             .collation = "utf8mb4_0900_ai_ci",
                                                             .encryption = "NO",
                                                         });
    failures +=
        expect_information_schema_schemata_row(database, &(const struct expected_schemata_row){
                                                             .schema_name = "sys",
                                                             .character_set = "utf8mb4",
                                                             .collation = "utf8mb4_0900_ai_ci",
                                                             .encryption = "NO",
                                                         });

    failures += execute_sql(database,
                            "CREATE DATABASE mylite_metadata_catalog_a DEFAULT CHARACTER SET "
                            "latin1 COLLATE latin1_swedish_ci ENCRYPTION='Y'",
                            MYLITE_DONE);
    failures += expect_information_schema_schemata_row(
        database, &(const struct expected_schemata_row){
                      .schema_name = "mylite_metadata_catalog_a",
                      .character_set = "latin1",
                      .collation = "latin1_swedish_ci",
                      .encryption = "YES",
                  });
    failures += execute_sql(database, "DROP DATABASE mylite_metadata_catalog_a", MYLITE_DONE);
    failures += expect_no_information_schema_schemata_row(database, "mylite_metadata_catalog_a");

    failures +=
        prepare_sql(database, "SELECT * FROM information_schema.schemata", MYLITE_OK, &stmt);
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.schemata", MYLITE_OK, &stmt);
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        prepare_sql(database, "SELECT * FROM `information_schema`.`SCHEMATA`", MYLITE_OK, &stmt);
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.TABLES", MYLITE_OK, &stmt);
    failures += expect_column_names(stmt, tables_columns, tables_column_count, "tables");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_information_schema_tables_views(database);
    failures += execute_sql(database, "CREATE DATABASE mylite_metadata_catalog_empty", MYLITE_DONE);
    failures +=
        expect_no_information_schema_table_schema_row(database, "mylite_metadata_catalog_empty");
    failures +=
        expect_empty_information_schema_table(database, "SELECT * FROM INFORMATION_SCHEMA.COLUMNS",
                                              columns_columns, columns_column_count);
    failures += expect_empty_information_schema_table(database,
                                                      "SELECT * FROM INFORMATION_SCHEMA.STATISTICS",
                                                      statistics_columns, statistics_column_count);
    failures += execute_sql(database, "DROP DATABASE mylite_metadata_catalog_empty", MYLITE_DONE);

    failures += prepare_sql(database, "SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA",
                            MYLITE_UNSUPPORTED, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "unsupported information_schema projection returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.SCHEMATA WHERE TRUE",
                            MYLITE_UNSUPPORTED, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "unsupported information_schema filter returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
    }
    failures += expect_prepare_error(database, "SELECT * FROM SCHEMATA", MYLITE_EXEC_ERROR,
                                     "No database selected", "unqualified table no database");
    failures +=
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.VIEWS", MYLITE_UNSUPPORTED, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "unsupported information_schema table returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
    }

    mylite_close(database);
    return failures;
}

static int test_information_schema_engines_execution(void)
{
    // NOLINTBEGIN(readability-function-size,readability-magic-numbers)
    static const char *const columns[] = {"ENGINE",       "SUPPORT", "COMMENT",
                                          "TRANSACTIONS", "XA",      "SAVEPOINTS"};
    static const char *const show_tables_columns[] = {"Tables_in_information_schema (ENGINES)",
                                                      "Table_type"};
    static const char *const show_tables_values[] = {"ENGINES", "SYSTEM VIEW"};
    static const char *const values[] = {
        "InnoDB",     "DEFAULT", "MyLite SQLite-backed transactional engine facade",
        "YES",        "NO",      "YES",
        "MEMORY",     "NO",      "In-memory tables are not supported by MyLite",
        NULL,         NULL,      NULL,
        "MyISAM",     "NO",      "MyISAM tables are not supported by MyLite",
        NULL,         NULL,      NULL,
        "FEDERATED",  "NO",      "Federated tables are not supported by MyLite",
        NULL,         NULL,      NULL,
        "MRG_MYISAM", "NO",      "Merge MyISAM tables are not supported by MyLite",
        NULL,         NULL,      NULL,
        "BLACKHOLE",  "NO",      "Blackhole tables are not supported by MyLite",
        NULL,         NULL,      NULL,
        "CSV",        "NO",      "CSV-backed tables are not supported by MyLite",
        NULL,         NULL,      NULL,
        "ARCHIVE",    "NO",      "Archive tables are not supported by MyLite",
        NULL,         NULL,      NULL,
    };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures +=
        expect_status(mylite_open_memory(&database), MYLITE_OK, "open information schema engines");

    failures += expect_select_rows(database, "SELECT * FROM INFORMATION_SCHEMA.ENGINES", columns, 6,
                                   values, 8, "information schema engines registry");
    failures += expect_select_rows(database, "SELECT * FROM information_schema.engines", columns, 6,
                                   values, 8, "information schema engines lower-case");
    failures += expect_select_rows(database, "SELECT * FROM Information_Schema.EnGiNeS", columns, 6,
                                   values, 8, "information schema engines mixed-case");
    failures += expect_select_rows(database, "SELECT * FROM `information_schema`.`ENGINES`",
                                   columns, 6, values, 8, "information schema engines quoted");

    failures += expect_information_schema_tables_views(database);
    failures += expect_select_rows(
        database, "SHOW FULL TABLES FROM information_schema LIKE 'engines'", show_tables_columns, 2,
        show_tables_values, 1, "show tables information schema engines");

    failures += prepare_sql(database, "SELECT ENGINE FROM INFORMATION_SCHEMA.ENGINES",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema engines projection");
    failures +=
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.ENGINES WHERE ENGINE = 'InnoDB'",
                    MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema engines where");
    failures += prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.ENGINES ORDER BY ENGINE",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema engines order by");
    failures += prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.ENGINES LIMIT 1",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema engines limit");
    failures += prepare_sql(database, "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ENGINES",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema engines count");
    failures += prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.ENGINES AS e",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema engines AS alias");
    failures += prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.ENGINES e",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema engines bare alias");
    failures +=
        prepare_sql(database, "SELECT INFORMATION_SCHEMA.ENGINES.* FROM INFORMATION_SCHEMA.ENGINES",
                    MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema engines qualified wildcard");
    failures += prepare_sql(database,
                            "SELECT * FROM INFORMATION_SCHEMA.ENGINES JOIN "
                            "INFORMATION_SCHEMA.TABLES",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema engines join");

    mylite_close(database);
    // NOLINTEND(readability-function-size,readability-magic-numbers)
    return failures;
}

static int test_information_schema_character_sets_execution(void)
{
    // NOLINTBEGIN(readability-function-size,readability-magic-numbers)
    static const char *const columns[] = {"CHARACTER_SET_NAME", "DEFAULT_COLLATE_NAME",
                                          "DESCRIPTION", "MAXLEN"};
    static const char *const show_tables_columns[] = {
        "Tables_in_information_schema (CHARACTER_SETS)", "Table_type"};
    static const char *const show_tables_values[] = {"CHARACTER_SETS", "SYSTEM VIEW"};
    static const char *const values[] = {
        "binary",
        "binary",
        "Binary pseudo charset",
        "1",
        "latin1",
        "latin1_swedish_ci",
        "cp1252 West European",
        "1",
        "utf8mb3",
        "utf8mb3_general_ci",
        "UTF-8 Unicode",
        "3",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "UTF-8 Unicode",
        "4",
    };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK,
                              "open information schema character sets");

    failures +=
        expect_select_rows(database, "SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS", columns, 4,
                           values, 4, "information schema character sets registry");
    failures +=
        expect_select_rows(database, "SELECT * FROM information_schema.character_sets", columns, 4,
                           values, 4, "information schema character sets lower-case");
    failures +=
        expect_select_rows(database, "SELECT * FROM Information_Schema.Character_Sets", columns, 4,
                           values, 4, "information schema character sets mixed-case");
    failures +=
        expect_select_rows(database, "SELECT * FROM `information_schema`.`CHARACTER_SETS`", columns,
                           4, values, 4, "information schema character sets quoted");

    failures +=
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS", MYLITE_OK, &stmt);
    failures += expect_column_names(stmt, columns, 4, "information schema character sets columns");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW,
                              "information schema character sets numeric maxlen row");
    failures += expect_int64(mylite_column_int64(stmt, 3), 1,
                             "information schema character sets numeric maxlen");
    failures += expect_int64(mylite_affected_rows(stmt), -1,
                             "information schema character sets affected rows");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += expect_information_schema_tables_views(database);
    failures += expect_select_rows(database,
                                   "SHOW FULL TABLES FROM information_schema LIKE 'character_sets'",
                                   show_tables_columns, 2, show_tables_values, 1,
                                   "show tables information schema character sets");

    failures +=
        prepare_sql(database, "SELECT CHARACTER_SET_NAME FROM INFORMATION_SCHEMA.CHARACTER_SETS",
                    MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema character sets projection");
    failures += prepare_sql(database, "SELECT DISTINCT * FROM INFORMATION_SCHEMA.CHARACTER_SETS",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema character sets distinct");
    failures += prepare_sql(database, "SELECT ALL * FROM INFORMATION_SCHEMA.CHARACTER_SETS",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema character sets explicit all");
    failures +=
        prepare_sql(database,
                    "SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS WHERE CHARACTER_SET_NAME = "
                    "'utf8mb4'",
                    MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema character sets where");
    failures += prepare_sql(database,
                            "SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS ORDER BY "
                            "CHARACTER_SET_NAME",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema character sets order by");
    failures += prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS LIMIT 1",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema character sets limit");
    failures += prepare_sql(database, "SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHARACTER_SETS",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema character sets count");
    failures += prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS AS cs",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema character sets AS alias");
    failures += prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS cs",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema character sets bare alias");
    failures += prepare_sql(database,
                            "SELECT INFORMATION_SCHEMA.CHARACTER_SETS.* FROM "
                            "INFORMATION_SCHEMA.CHARACTER_SETS",
                            MYLITE_UNSUPPORTED, &stmt);
    failures +=
        expect_no_stmt_handle(&stmt, "information schema character sets qualified wildcard");
    failures += prepare_sql(database,
                            "SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS JOIN "
                            "INFORMATION_SCHEMA.TABLES",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema character sets join");

    mylite_close(database);
    mylite_finalize(stmt);
    // NOLINTEND(readability-function-size,readability-magic-numbers)
    return failures;
}

static int test_information_schema_collations_execution(void)
{
    // NOLINTBEGIN(readability-function-size,readability-magic-numbers)
    static const char *const columns[] = {"COLLATION_NAME", "CHARACTER_SET_NAME", "ID",
                                          "IS_DEFAULT",     "IS_COMPILED",        "SORTLEN",
                                          "PAD_ATTRIBUTE"};
    static const char *const show_tables_columns[] = {"Tables_in_information_schema (COLLATIONS)",
                                                      "Table_type"};
    static const char *const show_tables_values[] = {"COLLATIONS", "SYSTEM VIEW"};
    static const char *const values[] = {
        "binary",
        "binary",
        "63",
        "Yes",
        "Yes",
        "1",
        "NO PAD",
        "latin1_bin",
        "latin1",
        "47",
        "",
        "Yes",
        "1",
        "PAD SPACE",
        "latin1_swedish_ci",
        "latin1",
        "8",
        "Yes",
        "Yes",
        "1",
        "PAD SPACE",
        "utf8mb3_bin",
        "utf8mb3",
        "83",
        "",
        "Yes",
        "1",
        "PAD SPACE",
        "utf8mb3_general_ci",
        "utf8mb3",
        "33",
        "Yes",
        "Yes",
        "1",
        "PAD SPACE",
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
        "255",
        "Yes",
        "Yes",
        "0",
        "NO PAD",
        "utf8mb4_bin",
        "utf8mb4",
        "46",
        "",
        "Yes",
        "1",
        "PAD SPACE",
    };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK,
                              "open information schema collations");

    failures += expect_select_rows(database, "SELECT * FROM INFORMATION_SCHEMA.COLLATIONS", columns,
                                   7, values, 7, "information schema collations registry");
    failures += expect_select_rows(database, "SELECT * FROM information_schema.collations", columns,
                                   7, values, 7, "information schema collations lower-case");
    failures += expect_select_rows(database, "SELECT * FROM Information_Schema.Collations", columns,
                                   7, values, 7, "information schema collations mixed-case");
    failures += expect_select_rows(database, "SELECT * FROM `information_schema`.`COLLATIONS`",
                                   columns, 7, values, 7, "information schema collations quoted");

    failures +=
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.COLLATIONS", MYLITE_OK, &stmt);
    failures += expect_column_names(stmt, columns, 7, "information schema collations columns");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW,
                              "information schema collations numeric binary row");
    failures += expect_int64(mylite_column_int64(stmt, show_collation_id_column), 63,
                             "information schema collations numeric id");
    failures += expect_int64(mylite_column_int64(stmt, show_collation_sortlen_column), 1,
                             "information schema collations numeric sortlen");
    failures +=
        expect_int64(mylite_affected_rows(stmt), -1, "information schema collations affected rows");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += expect_information_schema_tables_views(database);
    failures += expect_select_rows(
        database, "SHOW FULL TABLES FROM information_schema LIKE 'collations'", show_tables_columns,
        2, show_tables_values, 1, "show tables information schema collations");

    failures += prepare_sql(database, "SELECT COLLATION_NAME FROM INFORMATION_SCHEMA.COLLATIONS",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema collations projection");
    failures += prepare_sql(database, "SELECT DISTINCT * FROM INFORMATION_SCHEMA.COLLATIONS",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema collations distinct");
    failures += prepare_sql(database, "SELECT ALL * FROM INFORMATION_SCHEMA.COLLATIONS",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema collations explicit all");
    failures += prepare_sql(database,
                            "SELECT * FROM INFORMATION_SCHEMA.COLLATIONS WHERE COLLATION_NAME = "
                            "'utf8mb4_bin'",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema collations where");
    failures +=
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.COLLATIONS ORDER BY COLLATION_NAME",
                    MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema collations order by");
    failures += prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.COLLATIONS LIMIT 1",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema collations limit");
    failures += prepare_sql(database, "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLLATIONS",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema collations count");
    failures += prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.COLLATIONS AS c",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema collations AS alias");
    failures += prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.COLLATIONS c",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema collations bare alias");
    failures += prepare_sql(database,
                            "SELECT INFORMATION_SCHEMA.COLLATIONS.* FROM "
                            "INFORMATION_SCHEMA.COLLATIONS",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema collations qualified wildcard");
    failures += prepare_sql(database,
                            "SELECT * FROM INFORMATION_SCHEMA.COLLATIONS JOIN "
                            "INFORMATION_SCHEMA.TABLES",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "information schema collations join");

    mylite_close(database);
    mylite_finalize(stmt);
    // NOLINTEND(readability-function-size,readability-magic-numbers)
    return failures;
}

static int test_select_integer_literal(void)
{
    enum { expected_value = 123 };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open memory database");
    failures += prepare_sql(database, "SELECT 123", MYLITE_OK, &stmt);
    failures += expect_int(mylite_column_count(stmt), 1, "column count");
    failures += expect_string(mylite_column_name(stmt, 0), "123", "column name");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "first step");
    failures += expect_int64(mylite_column_int64(stmt, 0), expected_value, "integer value");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "second step");

    mylite_finalize(stmt);
    mylite_close(database);
    return failures;
}

static int test_select_integer_literal_with_semicolon(void)
{
    enum { expected_value = 123 };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open memory database");
    failures += prepare_sql(database, "SELECT 123;", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "semicolon first step");
    failures +=
        expect_int64(mylite_column_int64(stmt, 0), expected_value, "semicolon integer value");

    mylite_finalize(stmt);
    mylite_close(database);
    return failures;
}

static int test_expression_operator_foundation(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    static const char *columns[] = {
        "1 + 2 * 3",
        "(1 + 2) * 3",
        "1 | 2 & 0",
        "1 OR 0 AND 0",
        "NOT 1 BETWEEN 0 AND 2",
        "1 + 2 << 1",
        "1 BETWEEN 0 AND 2 AND 0",
        "1 XOR 1 OR 1",
    };
    static const char *values[] = {"7", "9", "1", "1", "0", "6", "0", "1"};
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open memory database");
    failures += expect_select_rows(database,
                                   "SELECT 1 + 2 * 3, (1 + 2) * 3, 1 | 2 & 0, "
                                   "1 OR 0 AND 0, NOT 1 BETWEEN 0 AND 2, "
                                   "1 + 2 << 1, 1 BETWEEN 0 AND 2 AND 0, 1 XOR 1 OR 1",
                                   columns, 8, values, 1, "expression precedence");

    failures += prepare_sql(database,
                            "SELECT NULL = NULL, NULL <=> NULL, 1 <=> NULL, NULL <> 1, "
                            "NULL IS NULL, NULL IS NOT NULL, 0 IS FALSE, 2 IS TRUE, "
                            "NULL IS UNKNOWN",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "null truth row");
    failures += expect_null_text(mylite_column_text(stmt, 0), "null equality");
    failures += expect_string(mylite_column_text(stmt, 1), "1", "null-safe equality");
    failures += expect_string(mylite_column_text(stmt, 2), "0", "null-safe nonmatch");
    failures += expect_null_text(mylite_column_text(stmt, 3), "null not equal");
    failures += expect_string(mylite_column_text(stmt, 4), "1", "is null");
    failures += expect_string(mylite_column_text(stmt, 5), "0", "is not null");
    failures += expect_string(mylite_column_text(stmt, 6), "1", "is false");
    failures += expect_string(mylite_column_text(stmt, 7), "1", "is true");
    failures += expect_string(mylite_column_text(stmt, 8), "1", "is unknown");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "null truth done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT 5 DIV 2, 5 / 2, 5 % 2, 5 MOD 2, ~0, 1 << 63, 1 >> 1",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "arithmetic row");
    failures += expect_string(mylite_column_text(stmt, 0), "2", "div");
    failures += expect_string(mylite_column_text(stmt, 1), "2.5000", "slash division");
    failures += expect_string(mylite_column_text(stmt, 2), "1", "percent");
    failures += expect_string(mylite_column_text(stmt, 3), "1", "mod");
    failures += expect_string(mylite_column_text(stmt, 4), "18446744073709551615", "bitwise not");
    failures += expect_string(mylite_column_text(stmt, 5), "9223372036854775808", "shift left");
    failures += expect_string(mylite_column_text(stmt, 6), "0", "shift right");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "arithmetic done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "SELECT 2 BETWEEN 1 AND 3, 2 NOT BETWEEN 3 AND 1, "
                            "2 BETWEEN 3 AND NULL, 2 BETWEEN NULL AND 1, "
                            "2 NOT BETWEEN 3 AND NULL, 2 NOT BETWEEN NULL AND 1, "
                            "'abc' LIKE 'a%', 'abc' LIKE 'A%', 'abc' LIKE 'a\\_c', "
                            "'a_c' LIKE 'a\\_c', 'abc' LIKE 'a\\%c', "
                            "'a%c' LIKE 'a\\%c', 'abc' NOT LIKE 'a%', 2 IN (1,2,3), "
                            "4 IN (1,2,NULL), 4 NOT IN (1,2,NULL)",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "range pattern row");
    failures += expect_string(mylite_column_text(stmt, 0), "1", "between");
    failures += expect_string(mylite_column_text(stmt, 1), "1", "not between");
    failures += expect_string(mylite_column_text(stmt, 2), "0", "between false low null high");
    failures += expect_string(mylite_column_text(stmt, 3), "0", "between false high null low");
    failures += expect_string(mylite_column_text(stmt, 4), "1", "not between false low null high");
    failures += expect_string(mylite_column_text(stmt, 5), "1", "not between false high null low");
    failures += expect_string(mylite_column_text(stmt, 6), "1", "like percent");
    failures += expect_string(mylite_column_text(stmt, 7), "1", "like case insensitive");
    failures += expect_string(mylite_column_text(stmt, 8), "0", "like escaped underscore miss");
    failures += expect_string(mylite_column_text(stmt, 9), "1", "like escaped underscore match");
    failures += expect_string(mylite_column_text(stmt, 10), "0", "like escaped percent miss");
    failures += expect_string(mylite_column_text(stmt, 11), "1", "like escaped percent match");
    failures += expect_string(mylite_column_text(stmt, 12), "0", "not like");
    failures += expect_string(mylite_column_text(stmt, 13), "1", "in match");
    failures += expect_null_text(mylite_column_text(stmt, 14), "in null miss");
    failures += expect_null_text(mylite_column_text(stmt, 15), "not in null miss");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "range pattern done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT 1/0, 1 DIV 0, 1 % 0", MYLITE_OK, &stmt);
    failures += expect_int(mylite_warning_count(database), 0, "division warning count before step");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "division warning row");
    failures += expect_null_text(mylite_column_text(stmt, 0), "slash zero");
    failures += expect_null_text(mylite_column_text(stmt, 1), "div zero");
    failures += expect_null_text(mylite_column_text(stmt, 2), "mod zero");
    failures += expect_int(mylite_warning_count(database), 3, "division warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), 1365, "division warning code");
    failures += expect_string(mylite_warning_message(database, 0), "Division by 0",
                              "division warning message");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT 1 IN ()", MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "empty in list");
    failures += prepare_sql(database, "SELECT ROW(1,2) IN ((1,2))", MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "row in deferred");
    failures += prepare_sql(database, "SELECT (SELECT 1), EXISTS (SELECT 1)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "simple subquery row");
    failures += expect_string(mylite_column_text(stmt, 0), "1", "simple scalar subquery");
    failures += expect_string(mylite_column_text(stmt, 1), "1", "simple exists subquery");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "simple subquery done");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        prepare_sql(database, "SELECT 1 = ANY (SELECT 1), 2 > ALL (SELECT 1)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "simple quantified subquery row");
    failures += expect_string(mylite_column_text(stmt, 0), "1", "simple any subquery");
    failures += expect_string(mylite_column_text(stmt, 1), "1", "simple all subquery");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "simple quantified subquery done");
    mylite_finalize(stmt);
    stmt = NULL;

    mylite_close(database);
    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_scalar_builtin_functions_execution(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    static const char *const scalar_columns[] = {
        "concat_text", "concat_null",    "byte_len",     "char_len", "lower_text", "upper_text",
        "left_text",   "right_text",     "replace_text", "abs_int",  "sign_neg",   "floor_num",
        "ceil_num",    "ceiling_num",    "mod_num",      "pi_value", "if_false",   "ifnull_value",
        "nullif_null", "coalesce_value", "isnull_value",
    };
    static const struct expected_result_metadata metadata[] = {
        {"concat_text", NULL, NULL, NULL, NULL, NULL, 8U, MYLITE_FIELD_TYPE_VAR_STRING, 31U, 255U,
         MYLITE_FIELD_FLAG_NOT_NULL, MYLITE_FIELD_FLAG_BINARY, 0},
        {"byte_len", NULL, NULL, NULL, NULL, NULL, 10U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
        {"pi_value", NULL, NULL, NULL, NULL, NULL, 8U, MYLITE_FIELD_TYPE_DOUBLE, 6U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
        {"isnull_value", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
    };
    static const char *const edge_columns[] = {
        "left_zero", "left_negative", "right_zero", "right_negative", "replace_empty",
        "octets",    "characters",    "lcase_text", "ucase_text",
    };
    static const char *const edge_values[] = {"", "", "", "", "abc", "6", "2", "abc", "ABC"};
    static const char *const projection_columns[] = {"id", "title"};
    static const char *const projection_values[] = {"1", "Alpha", "2", "Beta"};
    static const char *const id_column[] = {"id"};
    static const char *const n_column[] = {"n"};
    static const char *const id_2[] = {"2"};
    static const char *const id_3[] = {"3"};
    static const char *const n_1[] = {"1"};
    static const char *const id_s_n_columns[] = {"id", "s", "n"};
    static const char *const updated_values[] = {"2", "beta", "2"};
    static const char *const all_id_values[] = {"1", "2", "3"};
    static const char *const remaining_values[] = {"1", "2"};
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures +=
        expect_status(mylite_open_memory(&database), MYLITE_OK, "open scalar functions database");
    failures += execute_sql(database,
                            "CREATE DATABASE mylite_task24_functions "
                            "DEFAULT CHARACTER SET utf8mb4",
                            MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_task24_functions", MYLITE_DONE);

    failures += prepare_sql(database,
                            "SELECT CONCAT('a','b') AS concat_text, "
                            "CONCAT('a',NULL,'b') AS concat_null, "
                            "LENGTH('\xE6\xB5\xB7\xE8\xB1\x9A') AS byte_len, "
                            "CHAR_LENGTH('\xE6\xB5\xB7\xE8\xB1\x9A') AS char_len, "
                            "LOWER('AbC') AS lower_text, UPPER('AbC') AS upper_text, "
                            "LEFT('abcdef',2) AS left_text, RIGHT('abcdef',3) AS right_text, "
                            "REPLACE('banana','na','NA') AS replace_text, ABS(-12) AS abs_int, "
                            "SIGN(-12.5) AS sign_neg, FLOOR(-1.2) AS floor_num, "
                            "CEIL(-1.2) AS ceil_num, CEILING(1.2) AS ceiling_num, "
                            "MOD(7,3) AS mod_num, PI() AS pi_value, "
                            "IF(0,'yes','no') AS if_false, "
                            "IFNULL(NULL,'fallback') AS ifnull_value, "
                            "NULLIF('a','a') AS nullif_null, "
                            "COALESCE(NULL,NULL,'x') AS coalesce_value, "
                            "ISNULL(NULL) AS isnull_value",
                            MYLITE_OK, &stmt);
    failures += expect_column_names(stmt, scalar_columns,
                                    (int)(sizeof(scalar_columns) / sizeof(scalar_columns[0])),
                                    "scalar function columns");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "scalar function row");
    failures += expect_string(mylite_column_text(stmt, 0), "ab", "concat text");
    failures += expect_null_text(mylite_column_text(stmt, 1), "concat null");
    failures += expect_string(mylite_column_text(stmt, 2), "6", "length utf8 bytes");
    failures += expect_string(mylite_column_text(stmt, 3), "2", "char length utf8");
    failures += expect_string(mylite_column_text(stmt, 4), "abc", "lower text");
    failures += expect_string(mylite_column_text(stmt, 5), "ABC", "upper text");
    failures += expect_string(mylite_column_text(stmt, 6), "ab", "left text");
    failures += expect_string(mylite_column_text(stmt, 7), "def", "right text");
    failures += expect_string(mylite_column_text(stmt, 8), "baNANA", "replace text");
    failures += expect_string(mylite_column_text(stmt, 9), "12", "abs int");
    failures += expect_string(mylite_column_text(stmt, 10), "-1", "sign negative");
    failures += expect_string(mylite_column_text(stmt, 11), "-2", "floor negative");
    failures += expect_string(mylite_column_text(stmt, 12), "-1", "ceil negative");
    failures += expect_string(mylite_column_text(stmt, 13), "2", "ceiling positive");
    failures += expect_string(mylite_column_text(stmt, 14), "1", "mod function");
    failures += expect_string(mylite_column_text(stmt, 15), "3.141593", "pi function");
    failures += expect_string(mylite_column_text(stmt, 16), "no", "if false branch");
    failures += expect_string(mylite_column_text(stmt, 17), "fallback", "ifnull fallback");
    failures += expect_null_text(mylite_column_text(stmt, 18), "nullif equal");
    failures += expect_string(mylite_column_text(stmt, 19), "x", "coalesce first nonnull");
    failures += expect_string(mylite_column_text(stmt, 20), "1", "isnull null");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "scalar function done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "SELECT CONCAT('a','b') AS concat_text, "
                            "LENGTH('abc') AS byte_len, PI() AS pi_value, "
                            "ISNULL(NULL) AS isnull_value",
                            MYLITE_OK, &stmt);
    failures += expect_result_metadata(
        stmt, metadata, (int)(sizeof(metadata) / sizeof(metadata[0])), "scalar function metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "scalar metadata row");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "scalar metadata done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        expect_select_rows(database,
                           "SELECT LEFT('abcdef',0) AS left_zero, "
                           "LEFT('abcdef',-1) AS left_negative, "
                           "RIGHT('abcdef',0) AS right_zero, "
                           "RIGHT('abcdef',-1) AS right_negative, "
                           "REPLACE('abc','','x') AS replace_empty, "
                           "OCTET_LENGTH('\xE6\xB5\xB7\xE8\xB1\x9A') AS octets, "
                           "CHARACTER_LENGTH('\xE6\xB5\xB7\xE8\xB1\x9A') AS characters, "
                           "LCASE('AbC') AS lcase_text, UCASE('AbC') AS ucase_text",
                           edge_columns, (int)(sizeof(edge_columns) / sizeof(edge_columns[0])),
                           edge_values, 1, "scalar function edge values");

    failures += prepare_sql(database, "SELECT MOD(7,0) AS mod_zero", MYLITE_OK, &stmt);
    failures += expect_int(mylite_warning_count(database), 0, "mod zero warning before step");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "mod zero row");
    failures += expect_null_text(mylite_column_text(stmt, 0), "mod zero result");
    failures += expect_int(mylite_warning_count(database), 1, "mod zero warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_division_by_zero,
                           "mod zero warning code");
    failures += expect_string(mylite_warning_message(database, 0), "Division by 0",
                              "mod zero warning message");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "mod zero done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database,
                            "CREATE TABLE t ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
                            "s VARCHAR(20), "
                            "n INT)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO t (s,n) VALUES "
                            "('alpha',1),('Beta',-2),(NULL,NULL)",
                            MYLITE_DONE);
    failures += expect_select_rows(database,
                                   "SELECT id, CONCAT(UPPER(LEFT(s,1)), "
                                   "RIGHT(s,LENGTH(s)-1)) AS title "
                                   "FROM t WHERE ISNULL(s)=0 ORDER BY LOWER(s)",
                                   projection_columns, 2, projection_values, 2,
                                   "table function projection");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE ABS(n)=2", id_column, 1, id_2,
                                   1, "table function where");
    failures += expect_select_rows(database, "SELECT id FROM t ORDER BY COALESCE(n,0), id LIMIT 1",
                                   id_column, 1, id_2, 1, "table function order");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE COALESCE(n,0)=0", id_column, 1,
                                   id_3, 1, "coalesce where null row");

    failures += execute_sql_expect_done_affected(
        database,
        "UPDATE t SET s = CONCAT(LOWER(LEFT(s,1)), RIGHT(s,LENGTH(s)-1)), n = ABS(n) "
        "WHERE id = 2",
        1, "update function assignment");
    failures += expect_select_rows(database, "SELECT id, s, n FROM t WHERE id = 2", id_s_n_columns,
                                   3, updated_values, 1, "updated function values");

    failures += prepare_sql(database, "UPDATE t SET n = 5 WHERE MOD(7,0)", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "update function warning promoted");
    failures += expect_contains(mylite_error_message(database), "Division by 0",
                                "update function warning error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT n FROM t WHERE id = 1", n_column, 1, n_1, 1,
                                   "update warning predicate unchanged");

    failures += prepare_sql(database, "UPDATE t SET n = MOD(7,0) WHERE id = 1", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR,
                              "update assignment function warning promoted");
    failures += expect_contains(mylite_error_message(database), "Division by 0",
                                "update assignment function warning error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT n FROM t WHERE id = 1", n_column, 1, n_1, 1,
                                   "update warning assignment unchanged");

    failures += prepare_sql(database, "DELETE FROM t WHERE MOD(7,0)", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "delete function warning promoted");
    failures += expect_contains(mylite_error_message(database), "Division by 0",
                                "delete function warning error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT id FROM t ORDER BY id", id_column, 1,
                                   all_id_values, 3, "delete warning predicate unchanged");

    failures += execute_sql_expect_done_affected(database, "DELETE FROM t WHERE ISNULL(s)", 1,
                                                 "delete function predicate");
    failures += expect_select_rows(database, "SELECT id FROM t ORDER BY id", id_column, 1,
                                   remaining_values, 2, "delete function remaining rows");

    failures += prepare_sql(database, "SELECT SIN(1)", MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "unsupported scalar function");
    failures += prepare_sql(database, "SELECT CONCAT()", MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "unsupported concat arity");
    failures += prepare_sql(database, "SELECT PI(1)", MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "unsupported pi arity");

    mylite_close(database);
    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_case_expression_execution(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    static const char *const rowless_columns[] = {
        "searched_short", "searched_second", "null_condition", "simple_match",
        "simple_null",    "omitted_else",    "nested_case",
    };
    static const struct expected_result_metadata metadata[] = {
        {"case_text", NULL, NULL, NULL, NULL, NULL, 12U, MYLITE_FIELD_TYPE_VAR_STRING, 31U, 255U,
         MYLITE_FIELD_FLAG_NOT_NULL, MYLITE_FIELD_FLAG_BINARY, 0},
        {"case_int", NULL, NULL, NULL, NULL, NULL, 3U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
        {"case_null", NULL, NULL, NULL, NULL, NULL, 0U, MYLITE_FIELD_TYPE_NULL, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"case_decimal", NULL, NULL, NULL, NULL, NULL, 4U, MYLITE_FIELD_TYPE_NEWDECIMAL, 1U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
        {"case_func", NULL, NULL, NULL, NULL, NULL, 2U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
    };
    static const char *const id_column[] = {"id"};
    static const char *const id_label_columns[] = {"id", "label"};
    static const char *const projection_values[] = {
        "1", "n=1", "2", "n=-2", "3", "nil", "4", "n=0",
    };
    static const char *const simple_projection_values[] = {
        "1", "one", "2", "minus", "3", "zero", "4", "zero",
    };
    static const char *const where_values[] = {"1", "3"};
    static const char *const order_values[] = {"3", "2", "1", "4"};
    static const char *const id_s_columns[] = {"id", "s"};
    static const char *const updated_s_values[] = {"2", "beta"};
    static const char *const v_column[] = {"v"};
    static const char *const v_11[] = {"11"};
    static const char *const order_dml_columns[] = {"id", "marker"};
    static const char *const order_dml_after_update[] = {"1", "a", "2", "hit", "3", "c"};
    static const char *const order_dml_after_delete[] = {"1", "a", "3", "c"};
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open CASE database");
    failures += execute_sql(database,
                            "CREATE DATABASE mylite_case_expression "
                            "DEFAULT CHARACTER SET utf8mb4",
                            MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_case_expression", MYLITE_DONE);

    failures += prepare_sql(database,
                            "SELECT CASE WHEN 1 THEN 'yes' ELSE 1/0 END AS searched_short, "
                            "CASE WHEN 0 THEN 1/0 WHEN 2 THEN 'second' ELSE 'else' END "
                            "AS searched_second, "
                            "CASE WHEN NULL THEN 'null-branch' ELSE 'else' END "
                            "AS null_condition, "
                            "CASE 2 WHEN 1 THEN 'one' WHEN 2 THEN 'two' ELSE 'other' END "
                            "AS simple_match, "
                            "CASE NULL WHEN NULL THEN 'matched' ELSE 'else' END AS simple_null, "
                            "CASE WHEN 0 THEN 'no' END AS omitted_else, "
                            "CASE WHEN 1 THEN CASE WHEN 0 THEN 2 ELSE 3 END ELSE 4 END "
                            "AS nested_case",
                            MYLITE_OK, &stmt);
    failures += expect_column_names(stmt, rowless_columns,
                                    (int)(sizeof(rowless_columns) / sizeof(rowless_columns[0])),
                                    "CASE rowless columns");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CASE rowless row");
    failures += expect_string(mylite_column_text(stmt, 0), "yes", "CASE searched short");
    failures += expect_string(mylite_column_text(stmt, 1), "second", "CASE searched second");
    failures += expect_string(mylite_column_text(stmt, 2), "else", "CASE null condition");
    failures += expect_string(mylite_column_text(stmt, 3), "two", "CASE simple match");
    failures += expect_string(mylite_column_text(stmt, 4), "else", "CASE simple null");
    failures += expect_null_text(mylite_column_text(stmt, 5), "CASE omitted else");
    failures += expect_string(mylite_column_text(stmt, 6), "3", "CASE nested");
    failures += expect_int(mylite_warning_count(database), 0, "CASE rowless warning count");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "CASE rowless done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT CASE WHEN 0 THEN 10 ELSE 1/0 END AS selected_warning",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CASE selected warning row");
    failures += expect_null_text(mylite_column_text(stmt, 0), "CASE selected warning result");
    failures += expect_int(mylite_warning_count(database), 1, "CASE selected warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_division_by_zero,
                           "CASE selected warning code");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "SELECT CASE 1 WHEN 1 THEN 10 WHEN 1/0 THEN 20 ELSE 30 END "
                            "AS simple_short",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CASE simple short row");
    failures += expect_string(mylite_column_text(stmt, 0), "10", "CASE simple short result");
    failures += expect_int(mylite_warning_count(database), 0, "CASE simple short warning count");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "SELECT CASE 1 WHEN 0 THEN 10 WHEN 1/0 THEN 20 ELSE 30 END "
                            "AS simple_warning",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CASE simple warning row");
    failures += expect_string(mylite_column_text(stmt, 0), "30", "CASE simple warning result");
    failures += expect_int(mylite_warning_count(database), 1, "CASE simple warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_division_by_zero,
                           "CASE simple warning code");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT CASE 1/0 WHEN 0 THEN 10 ELSE 20 END AS base_warning",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CASE base warning row");
    failures += expect_string(mylite_column_text(stmt, 0), "20", "CASE base warning result");
    failures += expect_int(mylite_warning_count(database), 1, "CASE base warning count");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "SELECT CASE WHEN 1 THEN 'yes' ELSE 'no' END AS case_text, "
                            "CASE WHEN 1 THEN 10 ELSE 20 END AS case_int, "
                            "CASE WHEN 0 THEN NULL END AS case_null, "
                            "CASE 1 WHEN 1 THEN 2.5 ELSE 3 END AS case_decimal, "
                            "CASE WHEN 1 THEN ISNULL(NULL) ELSE 0 END AS case_func",
                            MYLITE_OK, &stmt);
    failures += expect_result_metadata(
        stmt, metadata, (int)(sizeof(metadata) / sizeof(metadata[0])), "CASE metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CASE metadata row");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "CASE metadata done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database,
                            "CREATE TABLE t ("
                            "id INT PRIMARY KEY, "
                            "n INT, "
                            "s VARCHAR(20), "
                            "marker VARCHAR(10))",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO t VALUES "
                            "(1,1,'alpha','a'),"
                            "(2,-2,'Beta','b'),"
                            "(3,NULL,NULL,'c'),"
                            "(4,0,'gamma','d')",
                            MYLITE_DONE);
    failures += expect_select_rows(
        database,
        "SELECT id, CASE WHEN n IS NULL THEN 'nil' ELSE CONCAT('n=', n) END AS label "
        "FROM t ORDER BY id",
        id_label_columns, 2, projection_values, 4, "CASE table projection");
    failures += expect_select_rows(
        database,
        "SELECT id, CASE n WHEN 1 THEN 'one' WHEN -2 THEN 'minus' ELSE 'zero' END AS label "
        "FROM t ORDER BY id",
        id_label_columns, 2, simple_projection_values, 4, "CASE simple table projection");
    failures += expect_select_rows(
        database,
        "SELECT id FROM t WHERE CASE WHEN n = 1 THEN 1 WHEN s IS NULL THEN 1 ELSE 0 END "
        "ORDER BY id",
        id_column, 1, where_values, 2, "CASE where");
    failures += expect_select_rows(
        database, "SELECT id FROM t ORDER BY CASE WHEN s IS NULL THEN 0 ELSE LENGTH(s) END, id",
        id_column, 1, order_values, 4, "CASE order");

    failures += execute_sql_expect_done_affected(
        database, "UPDATE t SET s = CASE WHEN n < 0 THEN LOWER(s) ELSE s END WHERE id = 2", 1,
        "CASE update assignment");
    failures += expect_select_rows(database, "SELECT id, s FROM t WHERE id = 2", id_s_columns, 2,
                                   updated_s_values, 1, "CASE update value");
    failures +=
        expect_prepare_error(database,
                             "SELECT CASE WHEN 1 THEN 1 ELSE missing_col END FROM t "
                             "LIMIT 1",
                             MYLITE_EXEC_ERROR, "missing_col", "CASE binds unselected branch");

    failures += execute_sql(database, "CREATE TABLE d (id INT PRIMARY KEY, v INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO d VALUES (1,10)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database, "UPDATE d SET v = CASE WHEN 1 THEN v + 1 ELSE MOD(7,0) END WHERE id = 1", 1,
        "CASE update unselected warning");
    failures += expect_select_rows(database, "SELECT v FROM d WHERE id = 1", v_column, 1, v_11, 1,
                                   "CASE update unselected warning value");
    failures += prepare_sql(
        database, "UPDATE d SET v = CASE WHEN 0 THEN v + 1 ELSE MOD(7,0) END WHERE id = 1",
        MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Division by 0", "CASE update selected warning");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT v FROM d WHERE id = 1", v_column, 1, v_11, 1,
                                   "CASE update selected warning unchanged");

    failures += execute_sql_expect_done_affected(
        database, "DELETE FROM d WHERE CASE WHEN id = 1 THEN 0 ELSE MOD(7,0) END", 0,
        "CASE delete unselected warning");
    failures += expect_select_row_count(database, "SELECT id FROM d WHERE id = 1", 1,
                                        "CASE delete unselected warning unchanged");
    failures +=
        prepare_sql(database, "DELETE FROM d WHERE CASE WHEN id = 1 THEN MOD(7,0) ELSE 0 END",
                    MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Division by 0", "CASE delete selected warning");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_row_count(database, "SELECT id FROM d WHERE id = 1", 1,
                                        "CASE delete selected warning unchanged");

    failures += execute_sql(database, "CREATE TABLE d2 (id INT PRIMARY KEY, v INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO d2 VALUES (1,10),(2,20)", MYLITE_DONE);
    failures +=
        prepare_sql(database, "DELETE FROM d2 WHERE CASE WHEN id = 1 THEN 0 ELSE MOD(7,0) END",
                    MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Division by 0", "CASE delete later row warning");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_row_count(database, "SELECT id FROM d2", 2,
                                        "CASE delete later row unchanged");

    failures += execute_sql(
        database, "CREATE TABLE order_dml_case (id INT PRIMARY KEY, v INT, marker VARCHAR(10))",
        MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO order_dml_case VALUES (1,30,'a'), (2,10,'b'), (3,20,'c')",
                            MYLITE_DONE);
    failures +=
        execute_sql_expect_done_affected(database,
                                         "UPDATE order_dml_case "
                                         "SET marker = CASE WHEN id = 2 THEN 'hit' ELSE 'miss' END "
                                         "WHERE CASE WHEN v >= 10 THEN 1 ELSE 0 END "
                                         "ORDER BY CASE WHEN id = 2 THEN 0 ELSE 1 END, id "
                                         "LIMIT 1",
                                         1, "CASE update order");
    failures += expect_select_rows(database, "SELECT id, marker FROM order_dml_case ORDER BY id",
                                   order_dml_columns, 2, order_dml_after_update, 3,
                                   "CASE update order rows");
    failures +=
        execute_sql_expect_done_affected(database,
                                         "DELETE FROM order_dml_case "
                                         "WHERE CASE WHEN v >= 10 THEN 1 ELSE 0 END "
                                         "ORDER BY CASE WHEN marker = 'hit' THEN 0 ELSE 1 END, id "
                                         "LIMIT 1",
                                         1, "CASE delete order");
    failures += expect_select_rows(database, "SELECT id, marker FROM order_dml_case ORDER BY id",
                                   order_dml_columns, 2, order_dml_after_delete, 2,
                                   "CASE delete order rows");

    mylite_close(database);
    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_cast_expression_execution(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    static const char *const rowless_columns[] = {
        "null_signed",   "round_signed", "unsigned_wrap",
        "decimal_value", "char_value",   "binary_value",
    };
    static const struct expected_result_metadata metadata[] = {
        {"signed_value", NULL, NULL, NULL, NULL, NULL, 21U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
         MYLITE_FIELD_FLAG_UNSIGNED, 0},
        {"unsigned_value", NULL, NULL, NULL, NULL, NULL, 21U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_BINARY |
             MYLITE_FIELD_FLAG_NUM,
         0U, 0},
        {"decimal_value", NULL, NULL, NULL, NULL, NULL, 8U, MYLITE_FIELD_TYPE_NEWDECIMAL, 2U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
        {"char_value", NULL, NULL, NULL, NULL, NULL, 12U, MYLITE_FIELD_TYPE_VAR_STRING, 31U, 255U,
         MYLITE_FIELD_FLAG_NOT_NULL, MYLITE_FIELD_FLAG_BINARY, 0},
        {"binary_value", NULL, NULL, NULL, NULL, NULL, 3U, MYLITE_FIELD_TYPE_VAR_STRING, 31U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY, MYLITE_FIELD_FLAG_NUM, 0},
        {"null_char", NULL, NULL, NULL, NULL, NULL, 0U, MYLITE_FIELD_TYPE_VAR_STRING, 31U, 255U, 0U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY, 1},
        {"quoted_binary_char", NULL, NULL, NULL, NULL, NULL, 3U, MYLITE_FIELD_TYPE_VAR_STRING, 31U,
         63U, MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY, MYLITE_FIELD_FLAG_NUM, 0},
    };
    static const char *const id_column[] = {"id"};
    static const char *const id_text_columns[] = {"id", "n_text"};
    static const char *const table_projection_values[] = {"1", "1", "2", "2"};
    static const char *const where_values[] = {"1"};
    static const char *const order_values[] = {"3", "2", "1"};
    static const char *const n_column[] = {"n"};
    static const char *const n_13[] = {"13"};
    static const char *const n_2[] = {"2"};
    static const char *const n_5[] = {"5"};
    static const char *const n_6[] = {"6"};
    static const char *const n_7[] = {"7"};
    static const char *const ids_2_3[] = {"2", "3"};
    static const char *const id_3[] = {"3"};
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open CAST database");

    failures += prepare_sql(database,
                            "SELECT CAST(NULL AS SIGNED) AS null_signed, "
                            "CAST(12.5 AS SIGNED) AS round_signed, "
                            "CAST(-1 AS UNSIGNED) AS unsigned_wrap, "
                            "CAST(12.345 AS DECIMAL(5,2)) AS decimal_value, "
                            "CAST(38.8 AS CHAR) AS char_value, "
                            "CAST('abc' AS BINARY) AS binary_value",
                            MYLITE_OK, &stmt);
    failures += expect_column_names(stmt, rowless_columns,
                                    (int)(sizeof(rowless_columns) / sizeof(rowless_columns[0])),
                                    "CAST rowless columns");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CAST rowless row");
    failures += expect_null_text(mylite_column_text(stmt, 0), "CAST null signed");
    failures += expect_string(mylite_column_text(stmt, 1), "13", "CAST rounded signed");
    failures += expect_string(mylite_column_text(stmt, 2), "18446744073709551615",
                              "CAST numeric unsigned wrap");
    failures += expect_string(mylite_column_text(stmt, 3), "12.35", "CAST decimal value");
    failures += expect_string(mylite_column_text(stmt, 4), "38.8", "CAST char value");
    failures += expect_string(mylite_column_text(stmt, 5), "abc", "CAST binary value");
    failures += expect_int(mylite_warning_count(database), 0, "CAST rowless warning count");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "CAST rowless done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "SELECT CAST('123' AS SIGNED) AS signed_value, "
                            "CAST('123' AS UNSIGNED) AS unsigned_value, "
                            "CAST('12.34' AS DECIMAL(6,2)) AS decimal_value, "
                            "CAST('abc' AS CHAR(3)) AS char_value, "
                            "CAST('abc' AS BINARY) AS binary_value, "
                            "CAST(NULL AS CHAR) AS null_char, "
                            "CAST('abc' AS CHAR CHARACTER SET 'binary') AS quoted_binary_char",
                            MYLITE_OK, &stmt);
    failures += expect_result_metadata(
        stmt, metadata, (int)(sizeof(metadata) / sizeof(metadata[0])), "CAST metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CAST metadata row");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "CAST metadata done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT CAST('12.5' AS SIGNED) AS value", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CAST string signed warning row");
    failures += expect_string(mylite_column_text(stmt, 0), "12", "CAST string signed value");
    failures += expect_int(mylite_warning_count(database), 1, "CAST string signed warning count");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_truncated_wrong_value, "CAST string signed warning code");
    failures += expect_contains(mylite_warning_message(database, 0), "INTEGER",
                                "CAST string signed warning message");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT CAST('-1' AS UNSIGNED) AS value", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CAST string unsigned warning row");
    failures += expect_string(mylite_column_text(stmt, 0), "18446744073709551615",
                              "CAST string unsigned value");
    failures += expect_int(mylite_warning_count(database), 1, "CAST string unsigned warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown,
                           "CAST string unsigned warning code");
    failures += expect_contains(mylite_warning_message(database, 0), "negative integer",
                                "CAST string unsigned warning message");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT CAST('18446744073709551615' AS UNSIGNED) AS value",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CAST string unsigned max row");
    failures += expect_string(mylite_column_text(stmt, 0), "18446744073709551615",
                              "CAST string unsigned max value");
    failures += expect_int(mylite_warning_count(database), 0, "CAST string unsigned max warnings");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT CAST('9223372036854775808' AS SIGNED) AS value",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CAST string signed complement row");
    failures += expect_string(mylite_column_text(stmt, 0), "-9223372036854775808",
                              "CAST string signed complement value");
    failures += expect_int(mylite_warning_count(database), 1,
                           "CAST string signed complement warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown,
                           "CAST string signed complement warning code");
    failures += expect_contains(mylite_warning_message(database, 0), "positive out-of-range",
                                "CAST string signed complement warning message");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT CAST('18446744073709551616' AS UNSIGNED) AS value",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CAST string unsigned overflow row");
    failures += expect_string(mylite_column_text(stmt, 0), "18446744073709551615",
                              "CAST string unsigned overflow value");
    failures += expect_int(mylite_warning_count(database), 1,
                           "CAST string unsigned overflow warning count");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_truncated_wrong_value,
                   "CAST string unsigned overflow warning code");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "SELECT CAST(1e20 AS SIGNED) AS signed_value, "
                            "CAST(1e20 AS UNSIGNED) AS unsigned_value",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CAST real overflow row");
    failures += expect_string(mylite_column_text(stmt, 0), "9223372036854775807",
                              "CAST real signed overflow value");
    failures += expect_string(mylite_column_text(stmt, 1), "9223372036854775807",
                              "CAST real unsigned overflow value");
    failures += expect_int(mylite_warning_count(database), 0, "CAST real overflow warnings");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "SELECT CAST('x' AS DECIMAL(5,2)) AS value", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CAST bad decimal warning row");
    failures += expect_string(mylite_column_text(stmt, 0), "0.00", "CAST bad decimal value");
    failures += expect_int(mylite_warning_count(database), 1, "CAST bad decimal warning count");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_truncated_wrong_value, "CAST bad decimal warning code");
    failures += expect_contains(mylite_warning_message(database, 0), "DECIMAL",
                                "CAST bad decimal warning message");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "SELECT CAST('abcdef' AS CHAR(3)) AS value", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CAST char warning row");
    failures += expect_string(mylite_column_text(stmt, 0), "abc", "CAST char truncated value");
    failures += expect_int(mylite_warning_count(database), 1, "CAST char warning count");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_truncated_wrong_value, "CAST char warning code");
    failures += expect_contains(mylite_warning_message(database, 0), "CHAR(3)",
                                "CAST char warning message");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT CAST('abc' AS CHAR(0)) AS value", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "CAST char zero warning row");
    failures += expect_string(mylite_column_text(stmt, 0), "", "CAST char zero value");
    failures += expect_int(mylite_warning_count(database), 1, "CAST char zero warning count");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_truncated_wrong_value, "CAST char zero warning code");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database,
                            "CREATE DATABASE mylite_cast_expression "
                            "DEFAULT CHARACTER SET utf8mb4",
                            MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_cast_expression", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE t ("
                            "id INT PRIMARY KEY, s VARCHAR(20), n INT)",
                            MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO t VALUES (1,'12',1),(2,'2',2),(3,NULL,NULL)",
                            MYLITE_DONE);
    failures +=
        expect_select_rows(database,
                           "SELECT id, CAST(n AS CHAR) AS n_text "
                           "FROM t WHERE n IS NOT NULL ORDER BY id",
                           id_text_columns, 2, table_projection_values, 2, "CAST table projection");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE CAST(s AS SIGNED) = 12",
                                   id_column, 1, where_values, 1, "CAST table where");
    failures += expect_select_rows(database, "SELECT id FROM t ORDER BY CAST(s AS UNSIGNED), id",
                                   id_column, 1, order_values, 3, "CAST table order");

    failures += execute_sql_expect_done_affected(
        database, "UPDATE t SET n = CAST(12.5 AS SIGNED) WHERE id = 1", 1,
        "CAST update assignment");
    failures += expect_select_rows(database, "SELECT n FROM t WHERE id = 1", n_column, 1, n_13, 1,
                                   "CAST update value");
    failures += prepare_sql(database, "UPDATE t SET n = CAST('12.5' AS SIGNED) WHERE id = 2",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Truncated incorrect INTEGER value",
                                  "CAST update warning promoted");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT n FROM t WHERE id = 2", n_column, 1, n_2, 1,
                                   "CAST update warning unchanged");
    failures +=
        execute_sql_expect_done_affected(database,
                                         "UPDATE t SET n = CASE WHEN id = 2 THEN CAST(5 AS SIGNED) "
                                         "ELSE CAST('bad' AS SIGNED) END WHERE id = 2",
                                         1, "CAST update unselected warning");
    failures += expect_select_rows(database, "SELECT n FROM t WHERE id = 2", n_column, 1, n_5, 1,
                                   "CAST update unselected warning value");
    failures += execute_sql_expect_done_affected(
        database, "UPDATE t SET n = n + 1 WHERE CAST(s AS SIGNED) = 2", 1, "CAST update predicate");
    failures += expect_select_rows(database, "SELECT n FROM t WHERE id = 2", n_column, 1, n_6, 1,
                                   "CAST update predicate value");
    failures += execute_sql_expect_done_affected(
        database, "UPDATE t SET n = 7 WHERE n IS NOT NULL ORDER BY CAST(s AS UNSIGNED), id LIMIT 1",
        1, "CAST update order");
    failures += expect_select_rows(database, "SELECT n FROM t WHERE id = 2", n_column, 1, n_7, 1,
                                   "CAST update order value");

    failures += execute_sql(database, "CREATE TABLE d (id INT PRIMARY KEY)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO d VALUES (1)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database, "DELETE FROM d WHERE CASE WHEN id = 1 THEN 0 ELSE CAST('bad' AS SIGNED) END", 0,
        "CAST delete unselected warning");
    failures += expect_select_row_count(database, "SELECT id FROM d WHERE id = 1", 1,
                                        "CAST delete unselected warning unchanged");
    failures += prepare_sql(database,
                            "DELETE FROM d WHERE CASE WHEN id = 1 THEN CAST('bad' AS SIGNED) "
                            "ELSE 0 END",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Truncated incorrect INTEGER value",
                                  "CAST delete warning promoted");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_row_count(database, "SELECT id FROM d WHERE id = 1", 1,
                                        "CAST delete warning unchanged");
    failures +=
        execute_sql(database, "CREATE TABLE del (id INT PRIMARY KEY, s VARCHAR(20))", MYLITE_DONE);
    failures +=
        execute_sql(database, "INSERT INTO del VALUES (1,'12'),(2,'2'),(3,'30')", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database, "DELETE FROM del WHERE CAST(s AS SIGNED) = 12", 1, "CAST delete predicate");
    failures += expect_select_rows(database, "SELECT id FROM del ORDER BY id", id_column, 1,
                                   ids_2_3, 2, "CAST delete predicate remaining rows");
    failures += execute_sql_expect_done_affected(
        database, "DELETE FROM del WHERE 1 ORDER BY CAST(s AS UNSIGNED), id LIMIT 1", 1,
        "CAST delete order");
    failures += expect_select_rows(database, "SELECT id FROM del ORDER BY id", id_column, 1, id_3,
                                   1, "CAST delete order remaining rows");

    mylite_close(database);
    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_aggregate_grouping_execution(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    static const char *const aggregate_columns[] = {
        "c_all", "c_n", "c_nullable", "sum_n", "avg_n", "min_n", "max_n",
    };
    static const char *const rowless_columns[] = {
        "c_all", "c_null", "c_one", "sum_one", "avg_one", "min_one", "max_one",
    };
    static const char *const rowless_values[] = {
        "1", "0", "1", "1", "1.0000", "1", "1",
    };
    static const char *const rowless_conversion_columns[] = {
        "sum_bad",
        "avg_text",
    };
    static const char *const rowless_conversion_values[] = {
        "0",
        "2.5",
    };
    static const char *const aggregate_values[] = {
        "5", "4", "3", "37", "9.2500", "0", "20",
    };
    static const char *const empty_values[] = {
        "0", "0", NULL, NULL, NULL, NULL,
    };
    static const char *const empty_columns[] = {"c_all", "c_n", "sum_n", "avg_n", "min_n", "max_n"};
    static const char *const grouped_columns[] = {"g",     "c",       "cn",     "sum_n",
                                                  "avg_n", "min_txt", "max_txt"};
    static const char *const grouped_values[] = {
        "a", "2", "2", "30", "15.0000", "alpha", "beta",
        "b", "2", "1", "0",  "0.0000",  "delta", "gamma",
    };
    static const char *const total_columns[] = {"g", "total"};
    static const char *const total_a[] = {"a", "30"};
    static const char *const derived_columns[] = {"next_id", "c"};
    static const char *const derived_values[] = {"4", "1", "5", "1", "6", "1"};
    static const char *const count_column[] = {"c"};
    static const char *const zero_count[] = {"0"};
    static const char *const alias_count_column[] = {"col2"};
    static const char *const alias_count_all[] = {"3"};
    static const char *const alias_count_group[] = {"2"};
    static const char *const conversion_columns[] = {"sum_s", "avg_s", "min_s", "max_s"};
    static const char *const conversion_values[] = {"12.5", "4.166666666666667", "10", "bad"};
    static const struct expected_result_metadata metadata[] = {
        {"grp", "mylite_aggregate_grouping", "t", "mylite_aggregate_grouping", "t", "grp", 40U,
         MYLITE_FIELD_TYPE_VAR_STRING, 0U, 255U, 0U, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"c", NULL, NULL, NULL, NULL, NULL, 21U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
        {"sum_n", NULL, NULL, NULL, NULL, NULL, 33U, MYLITE_FIELD_TYPE_NEWDECIMAL, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"avg_n", NULL, NULL, NULL, NULL, NULL, 16U, MYLITE_FIELD_TYPE_NEWDECIMAL, 4U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"min_txt", NULL, NULL, NULL, NULL, NULL, 80U, MYLITE_FIELD_TYPE_VAR_STRING, 0U, 255U, 0U,
         MYLITE_FIELD_FLAG_NOT_NULL, 1},
    };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open aggregate database");
    failures += execute_sql(database,
                            "CREATE DATABASE mylite_aggregate_grouping "
                            "DEFAULT CHARACTER SET utf8mb4",
                            MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_aggregate_grouping", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE t ("
                            "id INT PRIMARY KEY, "
                            "grp VARCHAR(10), "
                            "n INT, "
                            "decv DECIMAL(10,2), "
                            "txt VARCHAR(20), "
                            "nullable INT NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO t VALUES "
                            "(1,'a',10,1.50,'alpha',NULL),"
                            "(2,'a',20,2.25,'beta',5),"
                            "(3,'b',NULL,NULL,'gamma',NULL),"
                            "(4,'b',0,-3.75,'delta',0),"
                            "(5,NULL,7,4.00,'epsilon',7)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE empty_t ("
                            "id INT, n INT, decv DECIMAL(10,2), txt VARCHAR(20))",
                            MYLITE_DONE);

    failures += expect_select_rows(
        database,
        "SELECT COUNT(*) AS c_all, COUNT(NULL) AS c_null, COUNT(1) AS c_one, "
        "SUM(1) AS sum_one, AVG(1) AS avg_one, MIN(1) AS min_one, MAX(1) AS max_one",
        rowless_columns, 7, rowless_values, 1, "rowless aggregate");
    failures +=
        expect_select_rows(database, "SELECT SUM('bad') AS sum_bad, AVG('2.5x') AS avg_text",
                           rowless_conversion_columns, 2, rowless_conversion_values, 1,
                           "rowless string aggregate conversion");
    failures +=
        expect_int(mylite_warning_count(database), 2, "rowless string aggregate warning count");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_truncated_wrong_value,
                   "rowless string aggregate first warning code");
    failures +=
        expect_int((int)mylite_warning_code(database, 1), mysql_warning_truncated_wrong_value,
                   "rowless string aggregate second warning code");

    failures +=
        expect_select_rows(database,
                           "SELECT COUNT(*) AS c_all, COUNT(n) AS c_n, "
                           "COUNT(nullable) AS c_nullable, SUM(n) AS sum_n, "
                           "AVG(n) AS avg_n, MIN(n) AS min_n, MAX(n) AS max_n FROM t",
                           aggregate_columns, 7, aggregate_values, 1, "aggregate implicit group");

    failures += prepare_sql(database,
                            "SELECT COUNT(*) AS c_all, COUNT(n) AS c_n, SUM(n) AS sum_n, "
                            "AVG(n) AS avg_n, MIN(n) AS min_n, MAX(n) AS max_n FROM empty_t",
                            MYLITE_OK, &stmt);
    failures += expect_column_names(stmt, empty_columns, 6, "empty aggregate columns");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "empty aggregate row");
    failures += expect_string(mylite_column_text(stmt, 0), empty_values[0], "empty count star");
    failures += expect_string(mylite_column_text(stmt, 1), empty_values[1], "empty count expr");
    for (int index = 2; index < 6; ++index) {
        failures += expect_null_text(mylite_column_text(stmt, index), "empty aggregate null");
    }
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "empty aggregate done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        expect_select_rows(database,
                           "SELECT grp AS g, COUNT(*) AS c, COUNT(n) AS cn, SUM(n) AS sum_n, "
                           "AVG(n) AS avg_n, MIN(txt) AS min_txt, MAX(txt) AS max_txt "
                           "FROM t WHERE grp IS NOT NULL GROUP BY grp ORDER BY grp",
                           grouped_columns, 7, grouped_values, 2, "grouped aggregate rows");

    failures += expect_select_rows(database,
                                   "SELECT grp AS g, SUM(n) AS total "
                                   "FROM t GROUP BY g HAVING total > 10 ORDER BY g",
                                   total_columns, 2, total_a, 1, "group by alias having alias");
    failures += expect_select_rows(database,
                                   "SELECT grp AS g, SUM(n) AS total "
                                   "FROM t GROUP BY 1 HAVING SUM(n) >= 10 ORDER BY 2 DESC",
                                   total_columns, 2, total_a, 1, "group by ordinal order ordinal");
    failures +=
        expect_select_rows(database,
                           "SELECT id + 1 AS next_id, COUNT(*) AS c "
                           "FROM t GROUP BY id HAVING next_id > 3 ORDER BY next_id",
                           derived_columns, 2, derived_values, 3, "derived grouped expression");
    failures +=
        expect_select_rows(database, "SELECT COUNT(*) AS c FROM t WHERE id > 10 HAVING c = 0",
                           count_column, 1, zero_count, 1, "implicit group having zero count");
    failures +=
        expect_select_rows(database, "SELECT COUNT(*) AS c FROM t WHERE id > 10 HAVING c > 0",
                           count_column, 1, NULL, 0, "implicit group having filtered count");

    failures += prepare_sql(database,
                            "SELECT grp, COUNT(*) AS c, SUM(n) AS sum_n, AVG(n) AS avg_n, "
                            "MIN(txt) AS min_txt FROM t GROUP BY grp",
                            MYLITE_OK, &stmt);
    failures += expect_result_metadata(
        stmt, metadata, (int)(sizeof(metadata) / sizeof(metadata[0])), "aggregate metadata");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "CREATE TABLE conv_t (id INT PRIMARY KEY, s VARCHAR(20))",
                            MYLITE_DONE);
    failures += execute_sql(
        database, "INSERT INTO conv_t VALUES (1,'10'),(2,'bad'),(3,NULL),(4,'2.5x')", MYLITE_DONE);
    failures += prepare_sql(database,
                            "SELECT SUM(s) AS sum_s, AVG(s) AS avg_s, "
                            "MIN(s) AS min_s, MAX(s) AS max_s FROM conv_t",
                            MYLITE_OK, &stmt);
    failures += expect_column_names(stmt, conversion_columns, 4, "aggregate conversion columns");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "aggregate conversion row");
    for (int index = 0; index < 4; ++index) {
        failures += expect_string(mylite_column_text(stmt, index), conversion_values[index],
                                  "aggregate conversion value");
    }
    failures += expect_int(mylite_warning_count(database), 4, "aggregate conversion warning count");
    for (int index = 0; index < 4; ++index) {
        failures +=
            expect_int((int)mylite_warning_code(database, index),
                       mysql_warning_truncated_wrong_value, "aggregate conversion warning code");
    }
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        expect_prepare_error(database, "SELECT n FROM t WHERE COUNT(*) > 1", MYLITE_EXEC_ERROR,
                             "Invalid use of group function", "aggregate in where");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_invalid_group_function, "aggregate in where warning code");
    failures +=
        expect_prepare_error(database, "SELECT grp, n FROM t GROUP BY grp", MYLITE_EXEC_ERROR,
                             "not functionally dependent", "unsafe grouped select");
    failures += expect_int(mylite_warning_count(database), 2, "unsafe grouped select warnings");
    failures +=
        expect_int((int)mylite_warning_code(database, 1), mysql_warning_wrong_field_with_group,
                   "unsafe grouped select warning code");
    failures += expect_prepare_error(database, "SELECT n + SUM(n) AS bad FROM t", MYLITE_EXEC_ERROR,
                                     "without GROUP BY", "unsafe aggregate expression sibling");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_mix_group_function_fields,
                   "unsafe aggregate expression sibling warning code");
    failures += expect_prepare_error(database, "SELECT grp, COUNT(*) FROM t", MYLITE_EXEC_ERROR,
                                     "without GROUP BY", "unsafe implicit aggregate");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_mix_group_function_fields,
                   "unsafe implicit aggregate warning code");
    failures += expect_prepare_error(
        database, "SELECT grp, COUNT(*) AS c FROM t GROUP BY grp ORDER BY n", MYLITE_EXEC_ERROR,
        "not functionally dependent", "unsafe grouped order");
    failures += expect_prepare_error(
        database, "SELECT grp, COUNT(*) AS c FROM t GROUP BY grp HAVING n > 0", MYLITE_EXEC_ERROR,
        "Unknown column 'n' in 'having clause'", "unknown hidden having column");
    failures +=
        expect_prepare_error(database, "SELECT COUNT(*) FROM t GROUP BY 3", MYLITE_EXEC_ERROR,
                             "Unknown column '3'", "unknown group ordinal");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown_column,
                           "unknown group warning code");

    failures += execute_sql(database, "CREATE TABLE alias_t (col2 INT, col1 INT)", MYLITE_DONE);
    failures +=
        execute_sql(database, "INSERT INTO alias_t VALUES (2,10),(3,20),(2,30)", MYLITE_DONE);
    failures += expect_select_rows(
        database, "SELECT COUNT(col1) AS col2 FROM alias_t HAVING col2 = 3", alias_count_column, 1,
        alias_count_all, 1, "having aggregate alias over ungrouped table column");
    failures += expect_int(mylite_warning_count(database), 0, "having aggregate alias warnings");
    failures += expect_select_rows(database,
                                   "SELECT COUNT(col1) AS col2 "
                                   "FROM alias_t GROUP BY col2 HAVING col2 = 2",
                                   alias_count_column, 1, alias_count_group, 1,
                                   "having grouped table column over aggregate alias");
    failures += expect_int(mylite_warning_count(database), 2, "ambiguous group having warnings");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_ambiguous_column,
                           "ambiguous group warning code");
    failures += expect_int((int)mylite_warning_code(database, 1), mysql_warning_ambiguous_column,
                           "ambiguous having warning code");

    mylite_close(database);
    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_mylite_file_preamble_and_vfs_payload(void)
{
    enum { expected_payload_value = 7 };
    enum { expected_select_value = 123 };
    static const unsigned char sqlite_magic[] = "SQLite format 3";
    const char *path = MYLITE_RUNTIME_TEST_FILE_PATH;
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char sqlite_header[sizeof(sqlite_magic)];
    static const unsigned char zeroes[MYLITE_FILE_RESERVED_SIZE];
    sqlite3 *sqlite = NULL;
    sqlite3_stmt *sqlite_stmt = NULL;
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;
    int rc = SQLITE_OK;

    remove_runtime_test_files();
    failures += expect_status(mylite_open(path, &database), MYLITE_OK, "open mylite file");
    mylite_close(database);

    failures += read_file_at(path, 0L, preamble, sizeof(preamble));
    failures +=
        expect_bytes(preamble, MYLITE_FILE_MAGIC_TEXT, MYLITE_FILE_MAGIC_SIZE, "mylite file magic");
    failures +=
        expect_u16(mylite_file_preamble_get_u16(preamble, MYLITE_FILE_FORMAT_VERSION_OFFSET),
                   MYLITE_FILE_FORMAT_VERSION, "mylite format version");
    failures += expect_bytes(&preamble[MYLITE_FILE_RESERVED_OFFSET], zeroes,
                             MYLITE_FILE_RESERVED_SIZE, "mylite reserved preamble");

    failures += expect_default_sqlite_rejects_mylite(path);

    failures += expect_sqlite_status(
        sqlite3_open_v2(path, &sqlite, SQLITE_OPEN_READWRITE, mylite_vfs_name()), SQLITE_OK,
        "open sqlite through mylite vfs");
    if (sqlite != NULL) {
        failures += exec_sqlite(sqlite, "CREATE TABLE t(value INTEGER);");
        failures += exec_sqlite(sqlite, "INSERT INTO t VALUES(7);");
        sqlite3_close(sqlite);
        sqlite = NULL;
    }

    failures +=
        read_file_at(path, MYLITE_FILE_SQLITE_PAYLOAD_OFFSET, sqlite_header, sizeof(sqlite_header));
    failures +=
        expect_bytes(sqlite_header, sqlite_magic, sizeof(sqlite_magic), "sqlite payload magic");
    failures += expect_default_sqlite_rejects_mylite(path);

    failures += expect_sqlite_status(
        sqlite3_open_v2(path, &sqlite, SQLITE_OPEN_READWRITE, mylite_vfs_name()), SQLITE_OK,
        "reopen sqlite through mylite vfs");
    if (sqlite != NULL) {
        rc = sqlite3_prepare_v2(sqlite, "SELECT value FROM t", -1, &sqlite_stmt, NULL);
        failures += expect_sqlite_status(rc, SQLITE_OK, "prepare direct sqlite select");
        if (rc == SQLITE_OK) {
            failures += expect_sqlite_status(sqlite3_step(sqlite_stmt), SQLITE_ROW,
                                             "step direct sqlite select");
            failures += expect_int(sqlite3_column_int(sqlite_stmt, 0), expected_payload_value,
                                   "direct sqlite payload value");
        }
        sqlite3_finalize(sqlite_stmt);
        sqlite3_close(sqlite);
        sqlite = NULL;
        sqlite_stmt = NULL;
    }

    failures += expect_status(mylite_open(path, &database), MYLITE_OK, "reopen mylite file");
    failures += prepare_sql(database, "SELECT 123", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "mylite file select step");
    failures += expect_int64(mylite_column_int64(stmt, 0), expected_select_value,
                             "mylite file select value");

    mylite_finalize(stmt);
    mylite_close(database);
    remove_runtime_test_files();
    return failures;
}

static int test_mylite_open_rejects_plain_sqlite(void)
{
    const char *path = MYLITE_RUNTIME_TEST_FILE_PATH;
    sqlite3 *sqlite = NULL;
    mylite_db *database = NULL;
    int failures = 0;

    remove_runtime_test_files();
    failures += expect_sqlite_status(
        sqlite3_open_v2(path, &sqlite, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL), SQLITE_OK,
        "create plain sqlite file");
    if (sqlite != NULL) {
        failures += exec_sqlite(sqlite, "CREATE TABLE plain(value INTEGER);");
        sqlite3_close(sqlite);
        sqlite = NULL;
    }

    failures += expect_status(mylite_open(path, &database), MYLITE_SQLITE_ERROR,
                              "reject plain sqlite file");
    if (database != NULL) {
        fprintf(stderr, "mylite_open unexpectedly returned a database for a plain sqlite file\n");
        mylite_close(database);
        failures = 1;
    }

    remove_runtime_test_files();
    return failures;
}

static int test_unsupported_statement(void)
{
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open memory database");
    failures += prepare_sql(database, "SELECT CURRENT_TIMESTAMP", MYLITE_UNSUPPORTED, &stmt);
    failures += prepare_sql(database, "SELECT x'0a'", MYLITE_UNSUPPORTED, &stmt);
    failures += prepare_sql(database, "SELECT b'1010'", MYLITE_UNSUPPORTED, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "unsupported statement returned a statement handle\n");
        failures = 1;
    }

    mylite_close(database);
    return failures;
}

static int test_create_table_base_execution(void)
{
    const char *path = MYLITE_RUNTIME_TEST_FILE_PATH;
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;
    char *physical_name = NULL;

    remove_runtime_test_files();
    failures += expect_status(mylite_open(path, &database), MYLITE_OK, "open create table file");

    failures += prepare_sql(database, "CREATE TABLE no_default_table (a INT)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "create table no database");
    failures += expect_contains(mylite_error_message(database), "No database selected",
                                "create table no database error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database,
                            "CREATE DATABASE mylite_ct11 DEFAULT CHARSET utf8mb4 "
                            "COLLATE utf8mb4_bin",
                            MYLITE_DONE);
    failures +=
        execute_sql(database, "CREATE TABLE mylite_ct11.qualified_create (a INT)", MYLITE_DONE);
    failures += expect_information_schema_table_collation(database,
                                                          &(const struct expected_table_collation){
                                                              .table_name = "qualified_create",
                                                              .collation = "utf8mb4_bin",
                                                          });

    failures += prepare_sql(database, "CREATE TABLE missing_schema.t (a INT)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "create table missing schema");
    failures += expect_contains(mylite_error_message(database), "Unknown database",
                                "create table missing schema error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_table_name_row(database, "t");

    failures += prepare_sql(database, "CREATE TABLE information_schema.should_fail (a INT)",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "create table system schema");
    failures += expect_contains(mylite_error_message(database), "system schema",
                                "create table system schema error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_table_name_row(database, "should_fail");

    failures += execute_sql(database, "USE mylite_ct11", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE simple_create ("
                            "id INT, "
                            "name VARCHAR(20) DEFAULT 'x' COMMENT 'name col', "
                            "created TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                            "updated TIMESTAMP DEFAULT CURRENT_TIMESTAMP "
                            "ON UPDATE CURRENT_TIMESTAMP, "
                            "amount DECIMAL(10,2), "
                            "flag BOOL, "
                            "PRIMARY KEY (id), "
                            "UNIQUE KEY uq_name (name), "
                            "KEY amount_idx (amount)) "
                            "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin "
                            "COMMENT='hello table' AUTO_INCREMENT=10",
                            MYLITE_DONE);

    failures += expect_simple_create_table_row(database);
    failures += expect_simple_create_column_rows(database);
    failures += expect_simple_create_statistics_rows(database);

    failures += prepare_sql(database, "CREATE TABLE simple_create (a INT)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "duplicate table create");
    failures +=
        expect_contains(mylite_error_message(database), "already exists", "duplicate table error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        execute_sql(database, "CREATE TABLE IF NOT EXISTS simple_create (a INT)", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE IF NOT EXISTS new_table (a INT)", MYLITE_DONE);

    failures += prepare_sql(database,
                            "CREATE TABLE bad_charset (a VARCHAR(4)) "
                            "DEFAULT CHARSET latin1 COLLATE utf8mb4_bin",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "bad table charset");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_table_name_row(database, "bad_charset");

    failures += prepare_sql(database, "CREATE TABLE bad_columns (a INT, A INT)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "duplicate columns");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_table_name_row(database, "bad_columns");

    failures +=
        prepare_sql(database, "CREATE TABLE bad_index (a INT, b INT, KEY idx (a), KEY IDX (b))",
                    MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "duplicate indexes");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_table_name_row(database, "bad_index");

    failures += prepare_sql(database,
                            "CREATE TABLE bad_inline_index_collision ("
                            "a INT UNIQUE, b INT, KEY a (b))",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "inline duplicate index name");
    failures += expect_contains(mylite_error_message(database), "Duplicate key name",
                                "inline duplicate index name error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_table_name_row(database, "bad_inline_index_collision");

    failures +=
        prepare_sql(database, "CREATE TABLE bad_primary (a INT PRIMARY KEY, PRIMARY KEY (a))",
                    MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "duplicate primary key");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_table_name_row(database, "bad_primary");

    failures += execute_sql(database,
                            "CREATE TABLE inline_indexes ("
                            "key_alias INT KEY, "
                            "u INT UNIQUE, uk INT UNIQUE KEY, v INT, KEY (v))",
                            MYLITE_DONE);

    mylite_close(database);
    database = NULL;

    physical_name = expected_physical_table_name("mylite_ct11", "simple_create");
    if (physical_name == NULL) {
        fprintf(stderr, "out of memory while building expected physical table name\n");
        failures = 1;
    } else {
        struct sqlite_table_lookup lookup = {
            .path = path,
            .table_name = physical_name,
        };
        failures += expect_sqlite_table_exists(&lookup);
        free(physical_name);
    }

    remove_runtime_test_files();
    return failures;
}

static int expect_simple_create_table_row(mylite_db *database)
{
    mylite_stmt *stmt = NULL;
    int failures =
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.TABLES", MYLITE_OK, &stmt);
    int saw_simple_table = 0;

    while (failures == 0) {
        int status = mylite_step(stmt);

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "tables row");
        if (strcmp(mylite_column_text(stmt, tables_name_column), "simple_create") != 0) {
            continue;
        }
        saw_simple_table = 1;
        failures += expect_string(mylite_column_text(stmt, tables_catalog_column), "def",
                                  "created table catalog");
        failures += expect_string(mylite_column_text(stmt, tables_schema_column), "mylite_ct11",
                                  "created table schema");
        failures += expect_string(mylite_column_text(stmt, tables_type_column), "BASE TABLE",
                                  "created table type");
        failures +=
            expect_string(mylite_column_text(stmt, tables_engine_column), "InnoDB", "engine");
        failures += expect_int64(mylite_column_int64(stmt, tables_version_column),
                                 simple_create_table_version, "created table version");
        failures +=
            expect_int64(mylite_column_int64(stmt, tables_rows_column), 0, "created table rows");
        failures += expect_int64(mylite_column_int64(stmt, tables_auto_increment_column),
                                 simple_create_auto_increment, "created table auto_increment");
        failures += expect_string(mylite_column_text(stmt, tables_collation_column), "utf8mb4_bin",
                                  "created table collation");
        failures += expect_string(mylite_column_text(stmt, tables_comment_column), "hello table",
                                  "created table comment");
    }
    if (saw_simple_table == 0) {
        fprintf(stderr, "INFORMATION_SCHEMA.TABLES did not include simple_create\n");
        failures = 1;
    }
    mylite_finalize(stmt);
    return failures;
}

static int
expect_information_schema_table_collation(mylite_db *database,
                                          const struct expected_table_collation *expected)
{
    mylite_stmt *stmt = NULL;
    int failures =
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.TABLES", MYLITE_OK, &stmt);
    int saw_table = 0;

    while (failures == 0) {
        int status = mylite_step(stmt);

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "tables collation row");
        if (strcmp(mylite_column_text(stmt, tables_name_column), expected->table_name) != 0) {
            continue;
        }
        saw_table = 1;
        failures += expect_string(mylite_column_text(stmt, tables_collation_column),
                                  expected->collation, "table collation");
    }
    if (saw_table == 0) {
        fprintf(stderr, "INFORMATION_SCHEMA.TABLES did not include %s\n", expected->table_name);
        failures = 1;
    }
    mylite_finalize(stmt);
    return failures;
}

static int expect_simple_create_column_rows(mylite_db *database)
{
    mylite_stmt *stmt = NULL;
    int failures =
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.COLUMNS", MYLITE_OK, &stmt);
    int simple_columns = 0;

    while (failures == 0) {
        int status = mylite_step(stmt);
        const char *column_name = NULL;

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "columns row");
        if (strcmp(mylite_column_text(stmt, columns_table_name_column), "simple_create") != 0) {
            continue;
        }
        ++simple_columns;
        column_name = mylite_column_text(stmt, columns_name_column);
        if (strcmp(column_name, "id") == 0) {
            failures +=
                expect_int64(mylite_column_int64(stmt, columns_ordinal_column), 1, "id ordinal");
            failures += expect_string(mylite_column_text(stmt, columns_nullable_column), "NO",
                                      "id nullable");
            failures +=
                expect_string(mylite_column_text(stmt, columns_key_column), "PRI", "id key");
        } else if (strcmp(column_name, "name") == 0) {
            failures += expect_string(mylite_column_text(stmt, columns_default_column), "x",
                                      "name default");
            failures += expect_string(mylite_column_text(stmt, columns_data_type_column), "varchar",
                                      "name data type");
            failures += expect_int64(mylite_column_int64(stmt, columns_character_max_length_column),
                                     simple_create_name_length, "name max length");
            failures +=
                expect_int64(mylite_column_int64(stmt, columns_character_octet_length_column),
                             simple_create_name_octet_length, "name octet length");
            failures += expect_string(mylite_column_text(stmt, columns_character_set_column),
                                      "utf8mb4", "name charset");
            failures += expect_string(mylite_column_text(stmt, columns_collation_column),
                                      "utf8mb4_bin", "name collation");
            failures +=
                expect_string(mylite_column_text(stmt, columns_key_column), "UNI", "name key");
            failures += expect_string(mylite_column_text(stmt, columns_comment_column), "name col",
                                      "name comment");
        } else if (strcmp(column_name, "created") == 0) {
            failures += expect_string(mylite_column_text(stmt, columns_default_column),
                                      "CURRENT_TIMESTAMP", "created default");
            failures += expect_int64(mylite_column_int64(stmt, columns_datetime_precision_column),
                                     0, "created datetime precision");
            failures += expect_string(mylite_column_text(stmt, columns_extra_column),
                                      "DEFAULT_GENERATED", "created extra");
        } else if (strcmp(column_name, "updated") == 0) {
            failures += expect_string(mylite_column_text(stmt, columns_default_column),
                                      "CURRENT_TIMESTAMP", "updated default");
            failures +=
                expect_string(mylite_column_text(stmt, columns_extra_column),
                              "DEFAULT_GENERATED on update CURRENT_TIMESTAMP", "updated extra");
        } else if (strcmp(column_name, "amount") == 0) {
            failures += expect_int64(mylite_column_int64(stmt, columns_numeric_precision_column),
                                     simple_create_amount_precision, "amount precision");
            failures += expect_int64(mylite_column_int64(stmt, columns_numeric_scale_column), 2,
                                     "amount scale");
            failures +=
                expect_string(mylite_column_text(stmt, columns_key_column), "MUL", "amount key");
        } else if (strcmp(column_name, "flag") == 0) {
            failures += expect_string(mylite_column_text(stmt, columns_type_column), "tinyint(1)",
                                      "flag column type");
        }
    }
    if (simple_columns != simple_create_column_count) {
        fprintf(stderr, "expected %d simple_create columns, saw %d\n", simple_create_column_count,
                simple_columns);
        failures = 1;
    }
    mylite_finalize(stmt);
    return failures;
}

static int expect_simple_create_statistics_rows(mylite_db *database)
{
    mylite_stmt *stmt = NULL;
    int failures =
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.STATISTICS", MYLITE_OK, &stmt);
    int simple_statistics = 0;

    while (failures == 0) {
        int status = mylite_step(stmt);
        const char *index_name = NULL;

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "statistics row");
        if (strcmp(mylite_column_text(stmt, statistics_table_name_column), "simple_create") != 0) {
            continue;
        }
        ++simple_statistics;
        index_name = mylite_column_text(stmt, statistics_index_name_column);
        failures += expect_string(mylite_column_text(stmt, statistics_index_type_column), "BTREE",
                                  "statistics index type");
        failures += expect_string(mylite_column_text(stmt, statistics_visible_column), "YES",
                                  "statistics visible");
        if (strcmp(index_name, "PRIMARY") == 0) {
            failures += expect_int64(mylite_column_int64(stmt, statistics_non_unique_column), 0,
                                     "primary non unique");
            failures += expect_string(mylite_column_text(stmt, statistics_nullable_column), "",
                                      "primary nullable");
        } else if (strcmp(index_name, "uq_name") == 0) {
            failures += expect_int64(mylite_column_int64(stmt, statistics_non_unique_column), 0,
                                     "unique non unique");
            failures += expect_string(mylite_column_text(stmt, statistics_nullable_column), "YES",
                                      "unique nullable");
        } else if (strcmp(index_name, "amount_idx") == 0) {
            failures += expect_int64(mylite_column_int64(stmt, statistics_non_unique_column), 1,
                                     "secondary non unique");
            failures += expect_string(mylite_column_text(stmt, statistics_column_name_column),
                                      "amount", "secondary column");
        }
    }
    if (simple_statistics != simple_create_statistics_count) {
        fprintf(stderr, "expected %d simple_create statistics rows, saw %d\n",
                simple_create_statistics_count, simple_statistics);
        failures = 1;
    }
    mylite_finalize(stmt);
    return failures;
}

static int test_create_table_prepare_has_no_side_effects(void)
{
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open memory database");
    failures += prepare_sql(database,
                            "CREATE TABLE app.`integer_types` ("
                            "a TINYINT, b SMALLINT, c MEDIUMINT, d INT(0), e INTEGER, "
                            "f BIGINT UNSIGNED, g BOOL, h BOOLEAN, i INT1, j INT8, "
                            "`select` TINYINT(1), width255 INT(255), "
                            "mixed INT SIGNED UNSIGNED)",
                            MYLITE_OK, &stmt);
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_table_name_row(database, "integer_types");
    failures += prepare_sql(database,
                            "CREATE TABLE app.`string_binary_types` ("
                            "a CHAR, b CHAR(4) CHARACTER SET latin1, c VARCHAR(4), "
                            "d CHAR VARYING(5), e BINARY, f VARBINARY(4), "
                            "g TINYTEXT, h TEXT(63) CHARACTER SET binary, i MEDIUMTEXT, "
                            "j LONGTEXT, k TINYBLOB, l BLOB(256), m MEDIUMBLOB, "
                            "n LONGBLOB, o TEXT BINARY, p CHAR(4) BYTE, "
                            "q VARCHAR(4) CHARSET binary, r LONG VARCHAR, "
                            "s LONG VARBINARY, t NCHAR(4), u NVARCHAR(4), "
                            "v CHAR(4) COLLATE binary, w TEXT COLLATE binary)",
                            MYLITE_OK, &stmt);
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_table_name_row(database, "string_binary_types");
    failures += prepare_sql(database,
                            "CREATE TABLE app.`numeric_types` ("
                            "a DECIMAL, b DECIMAL(10,2), c DEC, d NUMERIC(8,3), "
                            "e FIXED(7,2), f FLOAT, g FLOAT(25), h FLOAT(25,2), "
                            "i DOUBLE, j DOUBLE PRECISION, k REAL, l FLOAT4, m FLOAT8, "
                            "n DECIMAL(10,2) UNSIGNED, o DECIMAL ZEROFILL SIGNED, "
                            "p FLOAT ZEROFILL SIGNED, q DOUBLE UNSIGNED ZEROFILL SIGNED, "
                            "r FLOAT4(10), s FLOAT4(25), t FLOAT4(10,2), "
                            "u FLOAT8(10,2), v DOUBLE PRECISION(10,2), w REAL(10,2))",
                            MYLITE_OK, &stmt);
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_table_name_row(database, "numeric_types");
    failures += prepare_sql(database,
                            "CREATE TABLE app.`temporal_types` ("
                            "a DATE, b TIME, c TIME(1), d TIME(6), "
                            "e DATETIME, f DATETIME(0), g DATETIME(6), "
                            "h TIMESTAMP, i TIMESTAMP(0), j TIMESTAMP(6), "
                            "k YEAR, l YEAR(4), m TIME(00), n DATETIME(06), o YEAR(004))",
                            MYLITE_OK, &stmt);
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_table_name_row(database, "temporal_types");
    failures += prepare_sql(database,
                            "CREATE TABLE app.`column_attributes` ("
                            "visible_col INT, a INT NULL, b INT NOT NULL, "
                            "c INT DEFAULT 7, d INT DEFAULT -1, e INT DEFAULT +2, "
                            "f INT DEFAULT 0x10, g INT DEFAULT b'101', "
                            "h VARCHAR(20) DEFAULT '', i INT DEFAULT (1 + 2), "
                            "j TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                            "k TIMESTAMP DEFAULT CURRENT_TIMESTAMP(), "
                            "l TIMESTAMP(6) DEFAULT CURRENT_TIMESTAMP(6) "
                            "ON UPDATE CURRENT_TIMESTAMP(6), "
                            "m TIMESTAMP DEFAULT (CURRENT_TIMESTAMP), "
                            "n INT COMMENT 'hello', o INT VISIBLE, p INT INVISIBLE, "
                            "q INT COLUMN_FORMAT DEFAULT STORAGE DEFAULT, "
                            "r INT COLUMN_FORMAT FIXED STORAGE DISK, "
                            "s INT COLUMN_FORMAT DYNAMIC STORAGE MEMORY, "
                            "t INT NULL NOT NULL DEFAULT 1 DEFAULT 2 VISIBLE INVISIBLE)",
                            MYLITE_OK, &stmt);
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_table_name_row(database, "column_attributes");
    failures += expect_no_information_schema_column_table_name_row(database, "column_attributes");
    failures += prepare_sql(database,
                            "CREATE TABLE app.`primary_key_auto_increment` ("
                            "id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, "
                            "shorthand INT KEY, no_key BIGINT AUTO_INCREMENT, "
                            "nullable_pk INT NULL PRIMARY KEY, "
                            "slug VARCHAR(64) NOT NULL DEFAULT '' COMMENT 'slug' VISIBLE, "
                            "decimal_auto DECIMAL AUTO_INCREMENT PRIMARY KEY, "
                            "float_auto FLOAT AUTO_INCREMENT PRIMARY KEY, "
                            "PRIMARY KEY pk_slug USING BTREE (slug(10) DESC, id ASC) "
                            "KEY_BLOCK_SIZE = 8 COMMENT 'pk' VISIBLE "
                            "ENGINE_ATTRIBUTE='{}' SECONDARY_ENGINE_ATTRIBUTE '', "
                            "CONSTRAINT PRIMARY KEY (shorthand) USING HASH INVISIBLE, "
                            "CONSTRAINT named PRIMARY KEY named_pk (nullable_pk DESC))",
                            MYLITE_OK, &stmt);
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_table_name_row(database, "primary_key_auto_increment");
    failures +=
        expect_no_information_schema_column_table_name_row(database, "primary_key_auto_increment");
    failures += expect_no_information_schema_statistics_table_name_row(
        database, "primary_key_auto_increment");
    failures += prepare_sql(database,
                            "CREATE TABLE app.`unique_secondary_indexes` ("
                            "a INT UNIQUE, b VARCHAR(64) UNIQUE KEY, c INT KEY, "
                            "btree INT, hash INT, "
                            "KEY (a), INDEX (hash), "
                            "INDEX idx_b USING BTREE (b(5) DESC, a ASC) "
                            "COMMENT 'secondary' VISIBLE KEY_BLOCK_SIZE = 8, "
                            "KEY USING HASH (btree) USING HASH USING BTREE INVISIBLE "
                            "ENGINE_ATTRIBUTE '{}' SECONDARY_ENGINE_ATTRIBUTE = '{}', "
                            "UNIQUE (a), UNIQUE KEY uk_b (b), "
                            "UNIQUE KEY USING BTREE (hash), "
                            "UNIQUE INDEX ux_c USING BTREE (c), "
                            "UNIQUE KEY uq_hash (a) USING HASH USING BTREE, "
                            "CONSTRAINT uq_d UNIQUE KEY unique_d (btree DESC), "
                            "CONSTRAINT UNIQUE uq_a (a))",
                            MYLITE_OK, &stmt);
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_table_name_row(database, "unique_secondary_indexes");
    failures +=
        expect_no_information_schema_column_table_name_row(database, "unique_secondary_indexes");
    failures += expect_no_information_schema_statistics_table_name_row(database,
                                                                       "unique_secondary_indexes");
    failures += prepare_sql(database, "CREATE TABLE invalid_width (a INT(256));",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "invalid-width CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures +=
        prepare_sql(database, "CREATE TABLE invalid_bool (a BOOL(1));", MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "invalid BOOL CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database, "CREATE TABLE invalid_attribute_comment (a INT COMMENT 123);",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "invalid comment-attribute CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_attribute_default "
                            "(a INT DEFAULT 1 + 2);",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "invalid default-attribute CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_current_timestamp_fsp "
                            "(a TIMESTAMP DEFAULT CURRENT_TIMESTAMP(7));",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "invalid current-timestamp CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_column_storage "
                            "(a INT STORAGE FLASH);",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "invalid storage-attribute CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database, "CREATE TABLE invalid_varchar (a VARCHAR);",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "invalid VARCHAR CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_blob_charset "
                            "(a BLOB CHARACTER SET utf8mb4);",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "invalid BLOB charset CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database, "CREATE TABLE invalid_decimal (a DECIMAL(66));",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "invalid DECIMAL CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database, "CREATE TABLE invalid_decimal_zero_scale (a DECIMAL(0,1));",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "invalid DECIMAL(0,1) CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database, "CREATE TABLE invalid_float_zero_display (a FLOAT(0,0));",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "invalid FLOAT(0,0) CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database, "CREATE TABLE invalid_double (a DOUBLE(10));",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "invalid DOUBLE CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database, "CREATE TABLE invalid_double_zero_display (a DOUBLE(0,0));",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "invalid DOUBLE(0,0) CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_numeric_overflow "
                            "(a FLOAT(18446744073709551616));",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "overflow numeric CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database, "CREATE TABLE invalid_time_fsp (a TIME(7));",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "invalid TIME CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database, "CREATE TABLE invalid_date_fsp (a DATE(0));",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "invalid DATE CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database, "CREATE TABLE invalid_year_width (a YEAR(5));",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "invalid YEAR CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_temporal_overflow "
                            "(a TIME(18446744073709551616));",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "overflow temporal CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_unique_inline_index "
                            "(a INT UNIQUE INDEX);",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid inline unique index CREATE TABLE");
    failures += prepare_sql(database, "CREATE TABLE invalid_secondary_empty (a INT, KEY ());",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid empty secondary key CREATE TABLE");
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_secondary_trailing "
                            "(a INT, KEY (a,));",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid trailing secondary key CREATE TABLE");
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_secondary_missing_parts "
                            "(a INT, KEY idx);",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid missing secondary key parts CREATE TABLE");
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_unique_missing_parts "
                            "(a INT, UNIQUE KEY idx);",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid missing unique key parts CREATE TABLE");
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_secondary_comment "
                            "(a INT, KEY idx (a) COMMENT = 'x');",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid secondary comment CREATE TABLE");
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_unique_overflow_prefix "
                            "(a VARCHAR(10), UNIQUE KEY uq "
                            "(a(18446744073709551616)));",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid unique prefix CREATE TABLE");
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_fulltext_key "
                            "(a TEXT, FULLTEXT KEY idx (a));",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid fulltext key CREATE TABLE");
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_spatial_key "
                            "(a INT, SPATIAL KEY idx (a));",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid spatial key CREATE TABLE");
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_functional_key_part "
                            "(a INT, KEY idx ((a + 1)));",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid functional key-part CREATE TABLE");
    failures += prepare_sql(database, "CREATE TABLE invalid_unique_identifier (unique INT);",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid unique identifier CREATE TABLE");
    failures += prepare_sql(database, "CREATE TABLE invalid_index_identifier (index INT);",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid index identifier CREATE TABLE");
    failures += prepare_sql(database, "CREATE TABLE invalid_key_identifier (key INT);",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid key identifier CREATE TABLE");
    failures += prepare_sql(database, "CREATE TABLE invalid_primary_empty (a INT, PRIMARY KEY ());",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid empty primary key CREATE TABLE");
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_primary_trailing "
                            "(a INT, PRIMARY KEY (a,));",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid trailing primary key CREATE TABLE");
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_primary_overflow_prefix "
                            "(a VARCHAR(10), PRIMARY KEY (a(18446744073709551616)));",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid primary prefix CREATE TABLE");
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_primary_key_block_string "
                            "(a INT, PRIMARY KEY (a) KEY_BLOCK_SIZE '8');",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid primary key block CREATE TABLE");
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_primary_comment_equal "
                            "(a INT, PRIMARY KEY (a) COMMENT = 'pk');",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid primary comment CREATE TABLE");
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_primary_using_rtree "
                            "(a INT, PRIMARY KEY USING RTREE (a));",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid primary RTREE CREATE TABLE");
    failures += prepare_sql(database,
                            "CREATE TABLE invalid_primary_engine_attribute "
                            "(a INT, PRIMARY KEY (a) ENGINE_ATTRIBUTE 123);",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "invalid primary engine attribute CREATE TABLE");

    mylite_close(database);
    return failures;
}

static int test_drop_table_base_execution(void)
{
    const char *path = MYLITE_RUNTIME_TEST_FILE_PATH;
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    char *drop_me_physical = NULL;
    int failures = 0;

    remove_runtime_test_files();
    failures += expect_status(mylite_open(path, &database), MYLITE_OK, "open drop table file");

    failures += prepare_sql(database, "DROP TABLE no_default_table", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "drop table no database");
    failures += expect_contains(mylite_error_message(database), "No database selected",
                                "drop table no database error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "DROP TABLE IF EXISTS no_default_table", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "drop table if exists no database");
    failures += expect_contains(mylite_error_message(database), "No database selected",
                                "drop table if exists no database error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "CREATE DATABASE mylite_dt12", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_dt12", MYLITE_DONE);

    failures += prepare_sql(database, "DROP TABLE missing_schema.t", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "drop missing schema table");
    failures += expect_contains(mylite_error_message(database), "Unknown table 'missing_schema.t'",
                                "drop missing schema table error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "DROP TABLE IF EXISTS missing_schema.t", MYLITE_DONE);

    failures += prepare_sql(database, "DROP TABLE information_schema.tables", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "drop system schema table");
    failures += expect_contains(mylite_error_message(database), "system schema",
                                "drop system schema table error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "DROP TEMPORARY TABLE information_schema.tables", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "drop temporary system schema table");
    failures += expect_contains(mylite_error_message(database), "system schema",
                                "drop temporary system schema table error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "DROP TEMPORARY TABLE IF EXISTS information_schema.tables",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR,
                              "drop temporary if exists system schema table");
    failures += expect_contains(mylite_error_message(database), "system schema",
                                "drop temporary if exists system schema table error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        execute_sql(database, "CREATE TABLE drop_me (id INT, KEY idx_id (id))", MYLITE_DONE);
    drop_me_physical = expected_physical_table_name("mylite_dt12", "drop_me");
    if (drop_me_physical == NULL) {
        fprintf(stderr, "out of memory while building drop_me physical table name\n");
        failures = 1;
    } else {
        failures += expect_sqlite_table_exists(&(const struct sqlite_table_lookup){
            .path = path,
            .table_name = drop_me_physical,
        });
    }
    failures += execute_sql(database, "DROP TABLE drop_me", MYLITE_DONE);
    failures += expect_no_information_schema_table_name_row(database, "drop_me");
    failures += expect_no_information_schema_column_table_name_row(database, "drop_me");
    failures += expect_no_information_schema_statistics_table_name_row(database, "drop_me");
    if (drop_me_physical != NULL) {
        failures += expect_sqlite_table_missing(&(const struct sqlite_table_lookup){
            .path = path,
            .table_name = drop_me_physical,
        });
        free(drop_me_physical);
        drop_me_physical = NULL;
    }

    failures += prepare_sql(database, "DROP TABLE missing", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "drop missing table");
    failures += expect_contains(mylite_error_message(database),
                                "Unknown table 'mylite_dt12.missing'", "drop missing table error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "DROP TABLE IF EXISTS missing", MYLITE_DONE);

    failures += execute_sql(database, "CREATE TABLE if_existing1 (id INT)", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE if_existing2 (id INT)", MYLITE_DONE);
    failures += execute_sql(database,
                            "DROP TABLE IF EXISTS if_existing1, missing_if_exists, if_existing2 "
                            "RESTRICT",
                            MYLITE_DONE);
    failures += expect_no_information_schema_table_name_row(database, "if_existing1");
    failures += expect_no_information_schema_table_name_row(database, "if_existing2");

    failures += execute_sql(database, "CREATE TABLE cascade_existing1 (id INT)", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE cascade_existing2 (id INT)", MYLITE_DONE);
    failures += execute_sql(database,
                            "DROP TABLE IF EXISTS cascade_existing1, cascade_existing2 "
                            "CASCADE",
                            MYLITE_DONE);
    failures += expect_no_information_schema_table_name_row(database, "cascade_existing1");
    failures += expect_no_information_schema_table_name_row(database, "cascade_existing2");

    failures += execute_sql(database, "CREATE TABLE atomic_d (id INT)", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE atomic_e (id INT)", MYLITE_DONE);
    failures +=
        prepare_sql(database, "DROP TABLE atomic_d, missing_atomic, atomic_e", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "drop table atomic missing");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_information_schema_table_collation(database,
                                                          &(const struct expected_table_collation){
                                                              .table_name = "atomic_d",
                                                              .collation = "utf8mb4_0900_ai_ci",
                                                          });
    failures += expect_information_schema_table_collation(database,
                                                          &(const struct expected_table_collation){
                                                              .table_name = "atomic_e",
                                                              .collation = "utf8mb4_0900_ai_ci",
                                                          });

    failures += execute_sql(database, "CREATE TABLE dup (id INT)", MYLITE_DONE);
    failures += prepare_sql(database, "DROP TABLE dup, dup", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "drop duplicate table");
    failures += expect_contains(mylite_error_message(database), "Not unique table/alias: 'dup'",
                                "drop duplicate table error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "DROP TABLE IF EXISTS dup, dup", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "drop duplicate table if exists");
    failures += expect_contains(mylite_error_message(database), "Not unique table/alias: 'dup'",
                                "drop duplicate table if exists error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "DROP TABLE dup, mylite_dt12.dup", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "drop duplicate qualified table");
    failures += expect_contains(mylite_error_message(database), "Not unique table/alias: 'dup'",
                                "drop duplicate qualified table error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_information_schema_table_collation(database,
                                                          &(const struct expected_table_collation){
                                                              .table_name = "dup",
                                                              .collation = "utf8mb4_0900_ai_ci",
                                                          });

    failures += execute_sql(database, "CREATE TABLE temp_base (id INT)", MYLITE_DONE);
    failures += prepare_sql(database, "DROP TEMPORARY TABLE temp_base", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "drop temporary base table");
    failures +=
        expect_contains(mylite_error_message(database), "Unknown table 'mylite_dt12.temp_base'",
                        "drop temporary base table error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_information_schema_table_collation(database,
                                                          &(const struct expected_table_collation){
                                                              .table_name = "temp_base",
                                                              .collation = "utf8mb4_0900_ai_ci",
                                                          });
    failures += execute_sql(database, "DROP TEMPORARY TABLE IF EXISTS temp_base", MYLITE_DONE);
    failures += expect_information_schema_table_collation(database,
                                                          &(const struct expected_table_collation){
                                                              .table_name = "temp_base",
                                                              .collation = "utf8mb4_0900_ai_ci",
                                                          });

    failures += execute_sql(database, "DROP TABLE temp_base, dup, atomic_d, atomic_e", MYLITE_DONE);
    failures += execute_sql(database, "DROP DATABASE mylite_dt12", MYLITE_DONE);

    mylite_close(database);
    remove_runtime_test_files();
    return failures;
}

static int test_create_drop_index_execution(void)
{
    static const char *const idx_base_columns[] = {"id", "a", "b", "c"};
    static const char *const idx_base_after_update_values[] = {"1", "10", "abcdef", NULL};
    static const char *const idx_base_after_drop_values[] = {
        "1", "10", "abcdef", NULL, "8", "10", "allowed_after_drop", "8"};
    static const char *const idx_odku_columns[] = {"id", "marker"};
    static const char *const idx_odku_values[] = {"1", "9", "2", "0"};
    static const char *const idx_replace_columns[] = {"id", "a", "b"};
    static const char *const idx_replace_values[] = {"3", "1", "1"};
    mylite_db *database = NULL;
    mylite_db *no_schema_database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open index database");
    failures += execute_sql(database, "CREATE DATABASE mylite_idx", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_idx", MYLITE_DONE);

    failures += execute_sql(database,
                            "CREATE TABLE idx_base ("
                            "id INT PRIMARY KEY, a INT, b VARCHAR(20), c INT NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO idx_base VALUES "
                            "(1,10,'abcdef',NULL),(2,20,'abzzzz',NULL),(3,30,'xyz',5)",
                            MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database,
        "CREATE INDEX idx_b USING BTREE ON idx_base "
        "(b(3) DESC, a ASC) COMMENT 'hello' INVISIBLE KEY_BLOCK_SIZE=8 "
        "ALGORITHM=INPLACE LOCK=NONE",
        0, "create secondary index");
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "idx_base",
                                                               .index_name = "idx_b",
                                                               .seq_in_index = 1,
                                                               .column_name = "b",
                                                               .non_unique = 1,
                                                               .collation = "D",
                                                               .sub_part = "3",
                                                               .index_comment = "hello",
                                                               .visible = "NO",
                                                           });
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "idx_base",
                                                               .index_name = "idx_b",
                                                               .seq_in_index = 2,
                                                               .column_name = "a",
                                                               .non_unique = 1,
                                                               .collation = "A",
                                                               .sub_part = NULL,
                                                               .index_comment = "hello",
                                                               .visible = "NO",
                                                           });

    failures += execute_sql_expect_done_affected(
        database, "CREATE UNIQUE INDEX uq_a ON idx_base (a) INVISIBLE", 0,
        "create standalone unique index");
    failures +=
        prepare_sql(database, "INSERT INTO idx_base VALUES (4,10,'dup',NULL)", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry",
                                  "standalone unique index rejects insert duplicate");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "INSERT INTO idx_base SET id=9, a=10, b='setdup', c=NULL",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry",
                                  "standalone unique index rejects insert set duplicate");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "UPDATE idx_base SET a = 20 WHERE id = 1", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry",
                                  "standalone unique index rejects update duplicate");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT id, a, b, c FROM idx_base WHERE id = 1",
                                   idx_base_columns, 4, idx_base_after_update_values, 1,
                                   "failed unique update leaves row unchanged");
    failures += execute_sql(database, "INSERT INTO idx_base VALUES (4,40,'new',NULL)", MYLITE_DONE);

    failures +=
        execute_sql_expect_done_affected(database, "CREATE UNIQUE INDEX uq_c ON idx_base (c)", 0,
                                         "create nullable standalone unique index");
    failures += execute_sql(database,
                            "INSERT INTO idx_base VALUES "
                            "(5,50,'null1',NULL),(6,60,'null2',NULL)",
                            MYLITE_DONE);
    failures +=
        prepare_sql(database, "INSERT INTO idx_base VALUES (7,70,'cdup',5)", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry",
                                  "standalone unique index rejects duplicate non-null");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql_expect_done_affected(database, "DROP INDEX uq_a ON idx_base LOCK=NONE",
                                                 0, "drop standalone unique index");
    failures += expect_no_information_schema_statistics_index_row(database, "idx_base", "uq_a");
    failures += execute_sql(database, "INSERT INTO idx_base VALUES (8,10,'allowed_after_drop',8)",
                            MYLITE_DONE);
    failures += expect_select_rows(
        database, "SELECT id, a, b, c FROM idx_base WHERE a = 10 ORDER BY id", idx_base_columns, 4,
        idx_base_after_drop_values, 2, "dropped unique index no longer rejects duplicates");

    failures += execute_sql_expect_done_affected(database,
                                                 "CREATE INDEX idx_hash USING HASH ON idx_base (b)",
                                                 0, "create hash fallback index");
    failures += expect_int(mylite_warning_count(database), 1, "hash fallback warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_using_other_handler,
                           "hash fallback warning code");
    failures += expect_contains(mylite_warning_message(database, 0), "HASH indexes",
                                "hash fallback warning message");
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "idx_base",
                                                               .index_name = "idx_hash",
                                                               .seq_in_index = 1,
                                                               .column_name = "b",
                                                               .non_unique = 1,
                                                               .collation = "A",
                                                               .sub_part = NULL,
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });

    failures += execute_sql_expect_done_affected(
        database, "CREATE INDEX idx_b_dup ON idx_base (b(3) DESC, a ASC)", 0,
        "create duplicate secondary index");
    failures += expect_int(mylite_warning_count(database), 1, "duplicate index warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_duplicate_index,
                           "duplicate index warning code");
    failures += expect_contains(mylite_warning_message(database, 0), "Duplicate index",
                                "duplicate index warning message");

    failures +=
        execute_sql(database, "CREATE TABLE idx_prefix (id INT, s VARCHAR(10))", MYLITE_DONE);
    failures +=
        execute_sql(database, "INSERT INTO idx_prefix VALUES (1,'ab1'),(2,'ac1')", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database, "CREATE UNIQUE INDEX uq_prefix ON idx_prefix (s(2))", 0,
        "create standalone prefix unique index");
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "idx_prefix",
                                                               .index_name = "uq_prefix",
                                                               .seq_in_index = 1,
                                                               .column_name = "s",
                                                               .non_unique = 0,
                                                               .collation = "A",
                                                               .sub_part = "2",
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });
    failures += prepare_sql(database, "INSERT INTO idx_prefix VALUES (3,'ab2')", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry",
                                  "prefix unique index rejects duplicate prefix");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "INSERT INTO idx_prefix VALUES (3,'zz1')", MYLITE_DONE);

    failures += execute_sql(database, "CREATE TABLE idx_dup_existing (a INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO idx_dup_existing VALUES (1),(1)", MYLITE_DONE);
    failures += prepare_sql(database, "CREATE UNIQUE INDEX uq_dup ON idx_dup_existing (a)",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry",
                                  "unique create validates existing rows");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        expect_no_information_schema_statistics_index_row(database, "idx_dup_existing", "uq_dup");

    failures += prepare_sql(database, "CREATE INDEX idx_missing_column ON idx_base (missing)",
                            MYLITE_OK, &stmt);
    failures +=
        expect_exec_error(stmt, database, "Key column", "create index rejects missing column");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_statistics_index_row(database, "idx_base",
                                                                  "idx_missing_column");

    failures += prepare_sql(database, "CREATE INDEX IDX_B ON idx_base (a)", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate key name",
                                  "create index rejects duplicate name case-insensitively");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "DROP INDEX missing_idx ON idx_base", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Can't DROP", "drop index rejects missing index");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "CREATE INDEX idx_missing_schema ON missing_schema.t (a)",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Unknown database",
                                  "create index rejects missing schema");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "CREATE INDEX idx_missing_table ON missing_table (a)",
                            MYLITE_OK, &stmt);
    failures +=
        expect_exec_error(stmt, database, "doesn't exist", "create index rejects missing table");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        prepare_sql(database, "CREATE INDEX idx_system ON information_schema.tables (TABLE_NAME)",
                    MYLITE_OK, &stmt);
    failures +=
        expect_exec_error(stmt, database, "system schema", "create index rejects system schema");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "CREATE FULLTEXT INDEX ft_missing_table ON missing_table (b)",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "doesn't exist",
                                  "fulltext create index resolves table before class support");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        prepare_sql(database, "CREATE FULLTEXT INDEX ft_missing_column ON idx_base (missing)",
                    MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Key column",
                                  "fulltext create index validates columns before class support");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(
        database, "CREATE FULLTEXT INDEX ft_b ON idx_base (b) WITH PARSER ngram", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_UNSUPPORTED,
                              "fulltext create index deferred unsupported");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "fulltext create index affected rows");
    failures +=
        expect_contains(mylite_error_message(database), "Unsupported standalone index class",
                        "fulltext create index unsupported error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_statistics_index_row(database, "idx_base", "ft_b");

    failures += execute_sql(database, "CREATE TABLE idx_odku (id INT, a INT, b INT, marker INT)",
                            MYLITE_DONE);
    failures +=
        execute_sql(database, "INSERT INTO idx_odku VALUES (1,1,2,0),(2,2,1,0)", MYLITE_DONE);
    failures += execute_sql(database, "CREATE UNIQUE INDEX uq_odku_a ON idx_odku (a)", MYLITE_DONE);
    failures += execute_sql(database, "CREATE UNIQUE INDEX uq_odku_b ON idx_odku (b)", MYLITE_DONE);
    failures +=
        execute_sql_expect_done_affected(database,
                                         "INSERT INTO idx_odku VALUES (3,1,1,9) "
                                         "ON DUPLICATE KEY UPDATE marker = VALUES(marker)",
                                         2, "standalone unique indexes drive ODKU conflict order");
    failures += expect_select_rows(database, "SELECT id, marker FROM idx_odku ORDER BY id",
                                   idx_odku_columns, 2, idx_odku_values, 2,
                                   "ODKU picks first standalone unique conflict");

    failures +=
        execute_sql(database, "CREATE TABLE idx_replace (id INT, a INT, b INT)", MYLITE_DONE);
    failures +=
        execute_sql(database, "INSERT INTO idx_replace VALUES (1,1,2),(2,2,1)", MYLITE_DONE);
    failures +=
        execute_sql(database, "CREATE UNIQUE INDEX uq_replace_a ON idx_replace (a)", MYLITE_DONE);
    failures +=
        execute_sql(database, "CREATE UNIQUE INDEX uq_replace_b ON idx_replace (b)", MYLITE_DONE);
    failures +=
        execute_sql_expect_done_affected(database, "REPLACE INTO idx_replace VALUES (3,1,1)", 3,
                                         "replace deletes standalone unique conflicts");
    failures += expect_select_rows(database, "SELECT id, a, b FROM idx_replace ORDER BY id",
                                   idx_replace_columns, 3, idx_replace_values, 1,
                                   "REPLACE keeps replacement row");

    failures += expect_status(mylite_open_memory(&no_schema_database), MYLITE_OK,
                              "open index no-schema database");
    failures +=
        prepare_sql(no_schema_database, "CREATE INDEX idx_no_schema ON t (a)", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, no_schema_database, "No database selected",
                                  "create index requires selected schema");
    mylite_finalize(stmt);
    stmt = NULL;
    mylite_close(no_schema_database);

    mylite_close(database);
    return failures;
}

static int test_alter_table_column_operations_execution(void)
{
    static const char *const all_columns[] = {"first_col", "id", "added",  "a",
                                              "b",         "c",  "hidden", "nn"};
    static const char *const visible_after_add_columns[] = {"first_col", "id", "added", "a",
                                                            "b",         "c",  "nn"};
    static const char *const final_columns[] = {"c", "first_col", "id", "b2", "added", "nn"};
    static const char *const ai_drop_columns[] = {"v"};
    static const char *const empty_default_columns[] = {"id", "s"};
    static const char *const one_col_columns[] = {"only_col"};
    static const char *const invisible_alter_columns[] = {"v"};
    static const char *const all_values[] = {"q", "1", "7", "10", "b1", NULL, "100", "0",
                                             "q", "2", "7", "20", "b2", "5",  "200", "0"};
    static const char *const visible_after_add_values[] = {"q", "1", "7", "10", "b1", NULL, "0",
                                                           "q", "2", "7", "20", "b2", "5",  "0"};
    static const struct expected_columns_row metadata_rows[] = {
        {.table_name = "alter_ops",
         .column_name = "first_col",
         .ordinal_position = 1,
         .column_default = "q",
         .is_nullable = "NO",
         .data_type = "varchar",
         .column_type = "varchar(5)",
         .column_key = "",
         .extra = ""},
        {.table_name = "alter_ops",
         .column_name = "id",
         .ordinal_position = 2,
         .column_default = NULL,
         .is_nullable = "NO",
         .data_type = "int",
         .column_type = "int",
         .column_key = "PRI",
         .extra = ""},
        {.table_name = "alter_ops",
         .column_name = "added",
         .ordinal_position = 3,
         .column_default = "7",
         .is_nullable = "YES",
         .data_type = "int",
         .column_type = "int",
         .column_key = "",
         .extra = ""},
        {.table_name = "alter_ops",
         .column_name = "a",
         .ordinal_position = 4,
         .column_default = NULL,
         .is_nullable = "YES",
         .data_type = "int",
         .column_type = "int",
         .column_key = "MUL",
         .extra = ""},
        {.table_name = "alter_ops",
         .column_name = "b",
         .ordinal_position = 5,
         .column_default = "x",
         .is_nullable = "YES",
         .data_type = "varchar",
         .column_type = "varchar(20)",
         .column_key = "MUL",
         .extra = ""},
        {.table_name = "alter_ops",
         .column_name = "c",
         .ordinal_position = 6,
         .column_default = NULL,
         .is_nullable = "YES",
         .data_type = "int",
         .column_type = "int",
         .column_key = "",
         .extra = ""},
        {.table_name = "alter_ops",
         .column_name = "hidden",
         .ordinal_position = 7,
         .column_default = NULL,
         .is_nullable = "YES",
         .data_type = "int",
         .column_type = "int",
         .column_key = "",
         .extra = "INVISIBLE"},
        {.table_name = "alter_ops",
         .column_name = "nn",
         .ordinal_position = 8,
         .column_default = NULL,
         .is_nullable = "NO",
         .data_type = "int",
         .column_type = "int",
         .column_key = "",
         .extra = ""},
    };
    static const char *const final_values[] = {"0", "q", "1", "b1", "7", "0",
                                               "5", "q", "2", "b2", "7", "0"};
    static const char *const ai_drop_values[] = {"10", "20"};
    static const char *const empty_default_values[] = {"1", ""};
    static const char *const invisible_values[] = {"1"};
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open alter database");
    failures += prepare_sql(database, "ALTER TABLE no_default ADD COLUMN c INT", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "No database selected", "alter no database");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "CREATE DATABASE mylite_alter", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_alter", MYLITE_DONE);

    failures +=
        prepare_sql(database, "ALTER TABLE missing_alter.t ADD COLUMN c INT", MYLITE_OK, &stmt);
    failures +=
        expect_exec_error(stmt, database, "Unknown database", "alter rejects missing schema");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "ALTER TABLE information_schema.tables ADD COLUMN c INT",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "system schema", "alter rejects system schema");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        execute_sql(database, "CREATE TABLE mylite_alter.qualified_alter (id INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO qualified_alter VALUES (1)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database,
        "ALTER TABLE mylite_alter.qualified_alter ADD COLUMN v INT DEFAULT 9, "
        "ALGORITHM=DEFAULT, LOCK DEFAULT",
        0, "alter schema-qualified target with default options");
    {
        static const char *const qualified_columns[] = {"id", "v"};
        static const char *const qualified_values[] = {"1", "9"};

        failures +=
            expect_select_rows(database, "SELECT id, v FROM qualified_alter", qualified_columns, 2,
                               qualified_values, 1, "alter schema-qualified data");
    }

    failures += execute_sql(database, "START TRANSACTION READ ONLY", MYLITE_DONE);
    failures += prepare_sql(database, "ALTER TABLE qualified_alter ADD COLUMN blocked INT",
                            MYLITE_OK, &stmt);
    failures +=
        expect_exec_error(stmt, database, "READ ONLY", "read only transaction rejects alter table");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "read only alter affected rows");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "ROLLBACK", MYLITE_DONE);
    failures += expect_no_information_schema_column_row(database, "qualified_alter", "blocked");

    failures += execute_sql(database,
                            "CREATE TABLE alter_ops ("
                            "id INT PRIMARY KEY, a INT, b VARCHAR(20) DEFAULT 'x', "
                            "c INT NULL, hidden INT INVISIBLE)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO alter_ops (id, a, b, c, hidden) VALUES "
                            "(1,10,'b1',NULL,100),(2,20,'b2',5,200)",
                            MYLITE_DONE);
    failures += execute_sql(database, "CREATE INDEX idx_ab ON alter_ops (a, b)", MYLITE_DONE);
    failures += execute_sql(database, "CREATE INDEX idx_b ON alter_ops (b)", MYLITE_DONE);

    failures += execute_sql_expect_done_affected(
        database,
        "ALTER TABLE alter_ops "
        "ADD COLUMN added INT DEFAULT 7 AFTER id, "
        "ADD COLUMN first_col VARCHAR(5) NOT NULL DEFAULT 'q' FIRST, "
        "ADD COLUMN nn INT NOT NULL",
        0, "alter add column multi-action affected rows");
    failures += expect_select_rows(database,
                                   "SELECT first_col, id, added, a, b, c, hidden, nn "
                                   "FROM alter_ops ORDER BY id",
                                   all_columns, (int)(sizeof(all_columns) / sizeof(all_columns[0])),
                                   all_values, 2, "alter add preserves data and backfills");
    failures += expect_select_rows(
        database, "SELECT * FROM alter_ops ORDER BY id", visible_after_add_columns,
        (int)(sizeof(visible_after_add_columns) / sizeof(visible_after_add_columns[0])),
        visible_after_add_values, 2, "alter add keeps invisible column hidden from wildcard");
    for (size_t index = 0U; index < sizeof(metadata_rows) / sizeof(metadata_rows[0]); ++index) {
        failures += expect_information_schema_column_row(database, &metadata_rows[index]);
    }

    failures += execute_sql_expect_done_affected(
        database, "ALTER TABLE alter_ops RENAME COLUMN b TO renamed_b", 0,
        "alter rename column affected rows");
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "alter_ops",
                                                               .index_name = "idx_ab",
                                                               .seq_in_index = 2,
                                                               .column_name = "renamed_b",
                                                               .non_unique = 1,
                                                               .collation = "A",
                                                               .sub_part = NULL,
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "alter_ops",
                                                               .index_name = "idx_b",
                                                               .seq_in_index = 1,
                                                               .column_name = "renamed_b",
                                                               .non_unique = 1,
                                                               .collation = "A",
                                                               .sub_part = NULL,
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });

    failures += execute_sql_expect_done_affected(
        database,
        "ALTER TABLE alter_ops CHANGE COLUMN renamed_b b2 BIGINT NOT NULL DEFAULT 5 AFTER id", 2,
        "alter change column affected rows");
    failures +=
        expect_no_information_schema_statistics_column_row(database, "alter_ops", "renamed_b");
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "alter_ops",
                                                               .index_name = "idx_ab",
                                                               .seq_in_index = 2,
                                                               .column_name = "b2",
                                                               .non_unique = 1,
                                                               .collation = "A",
                                                               .sub_part = NULL,
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });

    failures += execute_sql(database, "UPDATE alter_ops SET c = 0 WHERE c IS NULL", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database, "ALTER TABLE alter_ops MODIFY COLUMN c VARCHAR(20) NOT NULL DEFAULT 'z' FIRST", 0,
        "alter modify column affected rows");
    failures += execute_sql_expect_done_affected(
        database, "ALTER TABLE alter_ops DROP COLUMN a, DROP COLUMN hidden", 0,
        "alter drop column affected rows");
    failures += expect_int(mylite_warning_count(database), 1, "alter drop duplicate index warning");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_duplicate_index,
                           "alter drop duplicate index warning code");
    failures +=
        expect_select_rows(database,
                           "SELECT c, first_col, id, b2, added, nn "
                           "FROM alter_ops ORDER BY id",
                           final_columns, (int)(sizeof(final_columns) / sizeof(final_columns[0])),
                           final_values, 2, "alter final data and order");
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "alter_ops",
                                                               .index_name = "idx_ab",
                                                               .seq_in_index = 1,
                                                               .column_name = "b2",
                                                               .non_unique = 1,
                                                               .collation = "A",
                                                               .sub_part = NULL,
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });
    failures += expect_no_information_schema_statistics_column_row(database, "alter_ops", "a");

    failures += execute_sql(database,
                            "CREATE TABLE alter_unique ("
                            "id INT PRIMARY KEY, u INT, v INT, "
                            "UNIQUE KEY uq_u (u), UNIQUE KEY uq_vu (v, u))",
                            MYLITE_DONE);
    failures +=
        execute_sql(database, "INSERT INTO alter_unique VALUES (1,1,10),(2,2,20)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database, "ALTER TABLE alter_unique RENAME COLUMN u TO renamed_u", 0,
        "alter unique rename affected rows");
    failures += prepare_sql(database, "INSERT INTO alter_unique VALUES (3,1,30)", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry",
                                  "alter renamed unique index still rejects duplicates");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql_expect_done_affected(database, "ALTER TABLE alter_unique DROP COLUMN v",
                                                 0, "alter unique drop affected rows");
    failures +=
        expect_int(mylite_warning_count(database), 1, "alter unique drop duplicate index warning");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_duplicate_index,
                           "alter unique drop duplicate index warning code");
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "alter_unique",
                                                               .index_name = "uq_vu",
                                                               .seq_in_index = 1,
                                                               .column_name = "renamed_u",
                                                               .non_unique = 0,
                                                               .collation = "A",
                                                               .sub_part = NULL,
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });
    failures += prepare_sql(database, "INSERT INTO alter_unique (id, renamed_u) VALUES (4,2)",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry",
                                  "alter dropped unique index part still rejects duplicates");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(
        database, "CREATE TABLE ai_drop (id INT AUTO_INCREMENT PRIMARY KEY, v INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO ai_drop (v) VALUES (10),(20)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(database, "ALTER TABLE ai_drop DROP COLUMN id", 2,
                                                 "alter drop auto increment affected rows");
    failures += expect_select_rows(database, "SELECT v FROM ai_drop ORDER BY v", ai_drop_columns, 1,
                                   ai_drop_values, 2, "alter drop auto increment preserves rows");
    failures += expect_no_information_schema_statistics_index_row(database, "ai_drop", "PRIMARY");

    failures += execute_sql(database, "CREATE TABLE empty_default_alter (id INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO empty_default_alter VALUES (1)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database, "ALTER TABLE empty_default_alter ADD COLUMN s VARCHAR(5) NOT NULL DEFAULT ''", 0,
        "alter add empty string default affected rows");
    failures +=
        expect_select_rows(database, "SELECT id, s FROM empty_default_alter", empty_default_columns,
                           2, empty_default_values, 1, "alter add empty string default backfill");
    failures +=
        expect_information_schema_column_row(database, &(const struct expected_columns_row){
                                                           .table_name = "empty_default_alter",
                                                           .column_name = "s",
                                                           .ordinal_position = 2,
                                                           .column_default = "",
                                                           .is_nullable = "NO",
                                                           .data_type = "varchar",
                                                           .column_type = "varchar(5)",
                                                           .column_key = "",
                                                           .extra = "",
                                                       });

    failures += prepare_sql(database, "ALTER TABLE alter_ops DROP COLUMN b2, ADD COLUMN id INT",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate column name",
                                  "alter multi-action rollback on duplicate column");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        expect_select_rows(database,
                           "SELECT c, first_col, id, b2, added, nn "
                           "FROM alter_ops ORDER BY id",
                           final_columns, (int)(sizeof(final_columns) / sizeof(final_columns[0])),
                           final_values, 2, "failed alter leaves table data unchanged");
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "alter_ops",
                                                               .index_name = "idx_ab",
                                                               .seq_in_index = 1,
                                                               .column_name = "b2",
                                                               .non_unique = 1,
                                                               .collation = "A",
                                                               .sub_part = NULL,
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });

    failures += prepare_sql(database,
                            "ALTER TABLE alter_ops ADD COLUMN algorithm_col INT, "
                            "ALGORITHM=INSTANT",
                            MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_UNSUPPORTED, "alter rejects non-default algorithm");
    failures +=
        expect_int64(mylite_affected_rows(stmt), -1, "alter unsupported algorithm affected");
    failures += expect_contains(mylite_error_message(database), "ALGORITHM",
                                "alter unsupported algorithm error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_column_row(database, "alter_ops", "algorithm_col");

    failures += prepare_sql(database, "ALTER TABLE alter_ops ADD COLUMN lock_col INT, LOCK NONE",
                            MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_UNSUPPORTED, "alter rejects non-default lock");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "alter unsupported lock affected");
    failures +=
        expect_contains(mylite_error_message(database), "LOCK", "alter unsupported lock error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_column_row(database, "alter_ops", "lock_col");

    failures += prepare_sql(database, "ALTER TABLE alter_ops ADD COLUMN id INT", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate column name",
                                  "alter rejects duplicate resulting column");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        prepare_sql(database, "ALTER TABLE alter_ops DROP COLUMN missing_col", MYLITE_OK, &stmt);
    failures +=
        expect_exec_error(stmt, database, "Can't DROP", "alter rejects missing drop column");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database,
                            "ALTER TABLE alter_ops ADD COLUMN after_missing INT "
                            "AFTER missing_col",
                            MYLITE_OK, &stmt);
    failures +=
        expect_exec_error(stmt, database, "Unknown column", "alter rejects missing after column");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_column_row(database, "alter_ops", "after_missing");

    failures += execute_sql(database, "CREATE TABLE one_col (only_col INT)", MYLITE_DONE);
    failures += prepare_sql(database, "ALTER TABLE one_col DROP COLUMN only_col", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "delete all columns",
                                  "alter rejects dropping every column");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT only_col FROM one_col", one_col_columns, 1,
                                   NULL, 0, "drop every column leaves table queryable");

    failures += execute_sql(database, "CREATE TABLE invisible_alter (v INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO invisible_alter VALUES (1)", MYLITE_DONE);
    failures += prepare_sql(database, "ALTER TABLE invisible_alter MODIFY COLUMN v INT INVISIBLE",
                            MYLITE_OK, &stmt);
    failures +=
        expect_exec_error(stmt, database, "visible column", "alter rejects all-invisible result");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        expect_select_rows(database, "SELECT * FROM invisible_alter", invisible_alter_columns, 1,
                           invisible_values, 1, "all-invisible failure preserves column");

    failures += prepare_sql(database, "ALTER TABLE alter_ops ADD COLUMN ai INT AUTO_INCREMENT",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Incorrect table definition",
                                  "alter rejects inline auto increment");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "ALTER TABLE alter_ops ADD COLUMN inline_key INT UNIQUE",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Incorrect table definition",
                                  "alter rejects inline add unique key");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "ALTER TABLE alter_ops CHANGE COLUMN nn nn INT PRIMARY KEY",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Incorrect table definition",
                                  "alter rejects inline change primary key");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "ALTER TABLE alter_ops MODIFY COLUMN nn INT UNIQUE",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Incorrect table definition",
                                  "alter rejects inline modify unique key");
    mylite_finalize(stmt);
    stmt = NULL;

    mylite_close(database);
    return failures;
}

static int test_alter_table_key_operations_execution(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    static const char *const alter_key_columns[] = {"a", "b", "c", "d", "e", "body"};
    static const char *const alter_key_nullable_values[] = {"3", "30", "abe", "300", NULL, "null"};
    static const char *const alter_key_duplicate_a_values[] = {"2", "2"};
    static const char *const pk_duplicate_columns[] = {"a"};
    static const char *const pk_duplicate_values[] = {"1", "1"};
    static const char *const mixed_unique_columns[] = {"id", "marker", "u2"};
    static const char *const mixed_unique_values[] = {"1", "10", "7",  "2", "20",
                                                      "7", "4",  "10", "8"};
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open alter key database");
    failures += execute_sql(database, "CREATE DATABASE mylite_alter_key", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_alter_key", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE alter_key_base ("
                            "a INT NOT NULL, b INT, c VARCHAR(20), d INT NOT NULL, "
                            "e INT, body TEXT)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO alter_key_base VALUES "
                            "(1,10,'abc',100,NULL,'one'),"
                            "(2,20,'abd',200,NULL,'two')",
                            MYLITE_DONE);

    failures +=
        execute_sql_expect_done_affected(database, "ALTER TABLE alter_key_base ADD PRIMARY KEY (a)",
                                         0, "alter add primary key affected rows");
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "alter_key_base",
                                                               .index_name = "PRIMARY",
                                                               .seq_in_index = 1,
                                                               .column_name = "a",
                                                               .non_unique = 0,
                                                               .collation = "A",
                                                               .sub_part = NULL,
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });
    failures += expect_information_schema_column_row(database, &(const struct expected_columns_row){
                                                                   .table_name = "alter_key_base",
                                                                   .column_name = "a",
                                                                   .ordinal_position = 1,
                                                                   .column_default = NULL,
                                                                   .is_nullable = "NO",
                                                                   .data_type = "int",
                                                                   .column_type = "int",
                                                                   .column_key = "PRI",
                                                                   .extra = "",
                                                               });
    failures += prepare_sql(database,
                            "INSERT INTO alter_key_base VALUES "
                            "(5,50,'new',500,NULL,'primary check')",
                            MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_DONE, "insert after alter primary key succeeds");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database,
                            "INSERT INTO alter_key_base VALUES "
                            "(1,60,'dup',600,NULL,'primary dup')",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry",
                                  "alter primary key rejects insert duplicate");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql_expect_done_affected(
        database, "ALTER TABLE alter_key_base ADD UNIQUE uq_c (c(3)) INVISIBLE", 0,
        "alter add invisible prefix unique affected rows");
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "alter_key_base",
                                                               .index_name = "uq_c",
                                                               .seq_in_index = 1,
                                                               .column_name = "c",
                                                               .non_unique = 0,
                                                               .collation = "A",
                                                               .sub_part = "3",
                                                               .index_comment = "",
                                                               .visible = "NO",
                                                           });
    failures += execute_sql_expect_done_affected(
        database, "ALTER TABLE alter_key_base ADD UNIQUE KEY uq_e (e)", 0,
        "alter add nullable unique affected rows");
    failures += execute_sql(
        database, "INSERT INTO alter_key_base VALUES (3,30,'abe',300,NULL,'null')", MYLITE_DONE);
    failures += expect_select_rows(
        database, "SELECT a, b, c, d, e, body FROM alter_key_base WHERE a = 3", alter_key_columns,
        6, alter_key_nullable_values, 1, "nullable unique accepts multiple nulls");
    failures += prepare_sql(database,
                            "INSERT INTO alter_key_base VALUES "
                            "(4,40,'abcx',400,NULL,'prefix dup')",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry",
                                  "alter prefix unique rejects duplicate prefix");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql_expect_done_affected(
        database, "ALTER TABLE alter_key_base ADD INDEX idx_b (b DESC) COMMENT 'idx' INVISIBLE", 0,
        "alter add secondary index affected rows");
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "alter_key_base",
                                                               .index_name = "idx_b",
                                                               .seq_in_index = 1,
                                                               .column_name = "b",
                                                               .non_unique = 1,
                                                               .collation = "D",
                                                               .sub_part = NULL,
                                                               .index_comment = "idx",
                                                               .visible = "NO",
                                                           });
    failures +=
        execute_sql_expect_done_affected(database, "ALTER TABLE alter_key_base ADD INDEX (b)", 0,
                                         "alter generated secondary index name affected rows");
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "alter_key_base",
                                                               .index_name = "b",
                                                               .seq_in_index = 1,
                                                               .column_name = "b",
                                                               .non_unique = 1,
                                                               .collation = "A",
                                                               .sub_part = NULL,
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });
    failures += execute_sql_expect_done_affected(
        database, "ALTER TABLE alter_key_base ADD KEY idx_hash USING HASH (d)", 0,
        "alter hash fallback index affected rows");
    failures += expect_int(mylite_warning_count(database), 1, "alter hash fallback warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_using_other_handler,
                           "alter hash fallback warning code");
    failures += execute_sql_expect_done_affected(
        database, "ALTER TABLE alter_key_base ADD KEY idx_b_dup (b DESC)", 0,
        "alter duplicate index warning affected rows");
    failures +=
        expect_int(mylite_warning_count(database), 1, "alter duplicate index warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_duplicate_index,
                           "alter duplicate index warning code");

    failures += execute_sql_expect_done_affected(
        database, "ALTER TABLE alter_key_base RENAME INDEX idx_b TO idx_b_new", 0,
        "alter rename index affected rows");
    failures +=
        expect_no_information_schema_statistics_index_row(database, "alter_key_base", "idx_b");
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "alter_key_base",
                                                               .index_name = "idx_b_new",
                                                               .seq_in_index = 1,
                                                               .column_name = "b",
                                                               .non_unique = 1,
                                                               .collation = "D",
                                                               .sub_part = NULL,
                                                               .index_comment = "idx",
                                                               .visible = "NO",
                                                           });
    failures += execute_sql_expect_done_affected(
        database, "ALTER TABLE alter_key_base ALTER INDEX idx_b_new VISIBLE", 0,
        "alter index visible affected rows");
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "alter_key_base",
                                                               .index_name = "idx_b_new",
                                                               .seq_in_index = 1,
                                                               .column_name = "b",
                                                               .non_unique = 1,
                                                               .collation = "D",
                                                               .sub_part = NULL,
                                                               .index_comment = "idx",
                                                               .visible = "YES",
                                                           });
    failures +=
        execute_sql_expect_done_affected(database, "ALTER TABLE alter_key_base DROP KEY idx_b_new",
                                         0, "alter drop key affected rows");
    failures +=
        expect_no_information_schema_statistics_index_row(database, "alter_key_base", "idx_b_new");
    failures +=
        execute_sql_expect_done_affected(database, "ALTER TABLE alter_key_base DROP INDEX uq_c", 0,
                                         "alter drop unique affected rows");
    failures +=
        expect_no_information_schema_statistics_index_row(database, "alter_key_base", "uq_c");
    failures += execute_sql(
        database, "INSERT INTO alter_key_base VALUES (4,40,'abcx',400,NULL,'prefix allowed')",
        MYLITE_DONE);

    failures +=
        execute_sql_expect_done_affected(database, "ALTER TABLE alter_key_base DROP PRIMARY KEY", 0,
                                         "alter drop primary key affected rows");
    failures +=
        expect_no_information_schema_statistics_index_row(database, "alter_key_base", "PRIMARY");
    failures += expect_information_schema_column_row(database, &(const struct expected_columns_row){
                                                                   .table_name = "alter_key_base",
                                                                   .column_name = "a",
                                                                   .ordinal_position = 1,
                                                                   .column_default = NULL,
                                                                   .is_nullable = "NO",
                                                                   .data_type = "int",
                                                                   .column_type = "int",
                                                                   .column_key = "",
                                                                   .extra = "",
                                                               });
    failures += execute_sql(
        database, "INSERT INTO alter_key_base VALUES (2,50,'z',500,NULL,'duplicate a allowed')",
        MYLITE_DONE);
    failures += expect_select_rows(
        database, "SELECT a FROM alter_key_base WHERE a = 2 ORDER BY body", pk_duplicate_columns, 1,
        alter_key_duplicate_a_values, 2, "dropped primary key permits duplicate values");

    failures +=
        execute_sql(database, "CREATE TABLE mixed_unique (id INT, marker INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO mixed_unique VALUES (1,10),(2,20)", MYLITE_DONE);
    failures += prepare_sql(database,
                            "ALTER TABLE mixed_unique ADD COLUMN u INT NOT NULL DEFAULT 7, "
                            "ADD UNIQUE KEY uq_u (u)",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry",
                                  "alter added column unique validates default backfill");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_column_row(database, "mixed_unique", "u");
    failures += expect_no_information_schema_statistics_index_row(database, "mixed_unique", "uq_u");

    failures += prepare_sql(database,
                            "ALTER TABLE mixed_unique ADD COLUMN pk INT NOT NULL DEFAULT 1, "
                            "ADD PRIMARY KEY (pk)",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry",
                                  "alter added column primary validates default backfill");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_column_row(database, "mixed_unique", "pk");
    failures +=
        expect_no_information_schema_statistics_index_row(database, "mixed_unique", "PRIMARY");

    failures += execute_sql_expect_done_affected(
        database,
        "ALTER TABLE mixed_unique ADD COLUMN u2 INT NOT NULL DEFAULT 7, "
        "ADD UNIQUE KEY uq_marker_u2 (marker, u2)",
        0, "alter added column composite unique affected rows");
    failures += prepare_sql(database, "INSERT INTO mixed_unique (id, marker, u2) VALUES (3,10,7)",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry",
                                  "alter added column composite unique rejects duplicates");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "INSERT INTO mixed_unique (id, marker, u2) VALUES (4,10,8)",
                            MYLITE_DONE);
    failures += expect_select_rows(database, "SELECT id, marker, u2 FROM mixed_unique ORDER BY id",
                                   mixed_unique_columns, 3, mixed_unique_values, 3,
                                   "alter added column composite unique preserves rows");

    failures += execute_sql(database, "CREATE TABLE pk_dup (a INT NOT NULL)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO pk_dup VALUES (1),(1)", MYLITE_DONE);
    failures += prepare_sql(database, "ALTER TABLE pk_dup ADD PRIMARY KEY (a)", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry",
                                  "alter add primary validates duplicate existing rows");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_statistics_index_row(database, "pk_dup", "PRIMARY");
    failures += expect_select_rows(database, "SELECT a FROM pk_dup ORDER BY a",
                                   pk_duplicate_columns, 1, pk_duplicate_values, 2,
                                   "failed primary add leaves duplicate table unchanged");

    failures += execute_sql(database, "CREATE TABLE pk_null (a INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO pk_null VALUES (NULL)", MYLITE_DONE);
    failures += prepare_sql(database, "ALTER TABLE pk_null ADD PRIMARY KEY (a)", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Invalid use of NULL",
                                  "alter add primary rejects null existing rows");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_invalid_null,
                           "alter primary null warning code");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "CREATE TABLE pk_added_null (a INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO pk_added_null VALUES (1),(2)", MYLITE_DONE);
    failures +=
        prepare_sql(database, "ALTER TABLE pk_added_null ADD COLUMN k INT, ADD PRIMARY KEY (k)",
                    MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Invalid use of NULL",
                                  "mixed alter primary key rejects added nullable column");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_invalid_null,
                           "mixed alter primary null warning code");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_column_row(database, "pk_added_null", "k");
    failures +=
        expect_no_information_schema_statistics_index_row(database, "pk_added_null", "PRIMARY");
    failures += execute_sql(database, "CREATE TABLE uq_added_dup (a INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO uq_added_dup VALUES (1),(2)", MYLITE_DONE);
    failures += prepare_sql(database,
                            "ALTER TABLE uq_added_dup ADD COLUMN k INT NOT NULL DEFAULT 0, "
                            "ADD UNIQUE uq_k (k)",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry",
                                  "mixed alter unique validates added default values");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_no_information_schema_column_row(database, "uq_added_dup", "k");
    failures += expect_no_information_schema_statistics_index_row(database, "uq_added_dup", "uq_k");

    failures +=
        execute_sql(database, "CREATE TABLE pk_second (a INT PRIMARY KEY, b INT)", MYLITE_DONE);
    failures +=
        prepare_sql(database, "ALTER TABLE pk_second ADD PRIMARY KEY (b)", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Multiple primary",
                                  "alter add primary rejects existing primary key");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_multiple_primary,
                           "alter multiple primary warning code");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "ALTER TABLE pk_second ADD INDEX idx_missing (missing)",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Key column",
                                  "alter add index rejects missing key column");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_key_column_missing,
                           "alter missing key warning code");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "ALTER TABLE pk_second ALTER INDEX `PRIMARY` INVISIBLE",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "primary key index cannot be invisible",
                                  "alter primary index invisible rejected");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_primary_invisible,
                           "alter primary invisible warning code");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "CREATE TABLE pk_ai (id INT AUTO_INCREMENT PRIMARY KEY)",
                            MYLITE_DONE);
    failures += prepare_sql(database, "ALTER TABLE pk_ai DROP PRIMARY KEY", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Incorrect table definition",
                                  "alter drop primary rejects auto increment dependency");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "pk_ai",
                                                               .index_name = "PRIMARY",
                                                               .seq_in_index = 1,
                                                               .column_name = "id",
                                                               .non_unique = 0,
                                                               .collation = "A",
                                                               .sub_part = NULL,
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });

    failures +=
        execute_sql(database, "CREATE TABLE implicit_pk (u INT NOT NULL UNIQUE)", MYLITE_DONE);
    failures +=
        prepare_sql(database, "ALTER TABLE implicit_pk ALTER INDEX u INVISIBLE", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "primary key index cannot be invisible",
                                  "alter implicit primary index invisible rejected");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "ALTER TABLE alter_key_base ADD INDEX idx_hash (d)",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate key name",
                                  "alter add index rejects duplicate key name");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "ALTER TABLE alter_key_base DROP INDEX missing_idx",
                            MYLITE_OK, &stmt);
    failures +=
        expect_exec_error(stmt, database, "Can't DROP", "alter drop index rejects missing index");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "ALTER TABLE alter_key_base RENAME INDEX idx_b_dup TO b",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate key name",
                                  "alter rename index rejects duplicate target");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "alter_key_base",
                                                               .index_name = "idx_b_dup",
                                                               .seq_in_index = 1,
                                                               .column_name = "b",
                                                               .non_unique = 1,
                                                               .collation = "D",
                                                               .sub_part = NULL,
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });

    failures += prepare_sql(database,
                            "ALTER TABLE alter_key_base ADD INDEX idx_algo (d), "
                            "ALGORITHM=INPLACE",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_UNSUPPORTED,
                              "alter key rejects non-default algorithm");
    failures += expect_int64(mylite_affected_rows(stmt), -1,
                             "alter key unsupported algorithm affected rows");
    failures += expect_contains(mylite_error_message(database), "ALGORITHM",
                                "alter key unsupported algorithm error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        expect_no_information_schema_statistics_index_row(database, "alter_key_base", "idx_algo");

    failures += prepare_sql(
        database, "ALTER TABLE alter_key_base ADD FULLTEXT INDEX ft_body (body)", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_UNSUPPORTED, "alter fulltext placeholder rejected");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "alter fulltext affected rows");
    failures += expect_contains(mylite_error_message(database), "FULLTEXT",
                                "alter fulltext unsupported error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        expect_no_information_schema_statistics_index_row(database, "alter_key_base", "ft_body");

    failures += prepare_sql(database, "ALTER TABLE alter_key_base ADD SPATIAL INDEX sp_body (body)",
                            MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_UNSUPPORTED, "alter spatial placeholder rejected");
    failures += expect_contains(mylite_error_message(database), "SPATIAL",
                                "alter spatial unsupported error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        expect_no_information_schema_statistics_index_row(database, "alter_key_base", "sp_body");

    failures +=
        prepare_sql(database, "ALTER TABLE alter_key_base ADD CHECK (d > 0)", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_UNSUPPORTED, "alter check placeholder rejected");
    failures +=
        expect_contains(mylite_error_message(database), "CHECK", "alter check unsupported error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "ALTER TABLE alter_key_base ADD CONSTRAINT fk_base FOREIGN KEY (d) "
                            "REFERENCES missing_parent(id) ON DELETE CASCADE",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_UNSUPPORTED,
                              "alter foreign key placeholder rejected");
    failures += expect_contains(mylite_error_message(database), "FOREIGN KEY",
                                "alter foreign key unsupported error");
    mylite_finalize(stmt);
    stmt = NULL;

    mylite_close(database);
    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_rename_table_execution(void)
{
    // NOLINTBEGIN(readability-function-size,readability-magic-numbers)
    static const char *const id_columns[] = {"id"};
    static const char *const id_v_columns[] = {"id", "v"};
    static const char *const single_1[] = {"1"};
    static const char *const single_2[] = {"2"};
    static const char *const source_value[] = {"11"};
    static const char *const target_value[] = {"22"};
    static const char *const t2_values[] = {"1", "10", "2", "20"};
    const char *path = MYLITE_RUNTIME_TEST_FILE_PATH;
    mylite_db *database = NULL;
    mylite_db *file_database = NULL;
    mylite_db *no_schema_database = NULL;
    mylite_stmt *stmt = NULL;
    char *physical_source = NULL;
    char *physical_target = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open rename database");
    failures += prepare_sql(database, "RENAME TABLE no_default TO renamed", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "No database selected",
                                  "rename table requires selected schema");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "CREATE DATABASE mylite_rename_a", MYLITE_DONE);
    failures += execute_sql(database, "CREATE DATABASE mylite_rename_b", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_rename_a", MYLITE_DONE);

    failures += execute_sql(database,
                            "CREATE TABLE t1 (id INT PRIMARY KEY, v INT, "
                            "UNIQUE KEY uq_v (v), KEY k_v (v))",
                            MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO t1 VALUES (1,10),(2,20)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(database, "RENAME TABLE t1 TO t2", 0,
                                                 "rename table affected rows");
    failures += expect_no_information_schema_table_name_row(database, "t1");
    failures += expect_no_information_schema_column_table_name_row(database, "t1");
    failures += expect_no_information_schema_statistics_table_name_row(database, "t1");
    failures += expect_select_rows(database, "SELECT id, v FROM t2 ORDER BY id", id_v_columns, 2,
                                   t2_values, 2, "renamed table preserves rows");
    failures += expect_information_schema_column_row(database, &(const struct expected_columns_row){
                                                                   .table_name = "t2",
                                                                   .column_name = "id",
                                                                   .ordinal_position = 1,
                                                                   .column_default = NULL,
                                                                   .is_nullable = "NO",
                                                                   .data_type = "int",
                                                                   .column_type = "int",
                                                                   .column_key = "PRI",
                                                                   .extra = "",
                                                               });
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "t2",
                                                               .index_name = "PRIMARY",
                                                               .seq_in_index = 1,
                                                               .column_name = "id",
                                                               .non_unique = 0,
                                                               .collation = "A",
                                                               .sub_part = NULL,
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "t2",
                                                               .index_name = "k_v",
                                                               .seq_in_index = 1,
                                                               .column_name = "v",
                                                               .non_unique = 1,
                                                               .collation = "A",
                                                               .sub_part = NULL,
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });

    failures += execute_sql_expect_done_affected(database, "ALTER TABLE t2 RENAME t3", 0,
                                                 "alter table rename bare affected rows");
    failures += execute_sql_expect_done_affected(database, "ALTER TABLE t3 RENAME TO t4", 0,
                                                 "alter table rename to affected rows");
    failures += execute_sql_expect_done_affected(database, "ALTER TABLE t4 RENAME AS t5", 0,
                                                 "alter table rename as affected rows");
    failures += expect_select_rows(database, "SELECT id, v FROM t5 ORDER BY id", id_v_columns, 2,
                                   t2_values, 2, "alter renamed table preserves rows");
    failures += expect_no_information_schema_table_name_row(database, "t2");
    failures += expect_no_information_schema_table_name_row(database, "t3");
    failures += expect_no_information_schema_table_name_row(database, "t4");

    failures += execute_sql_expect_done_affected(database, "RENAME TABLE t5 TO mylite_rename_b.t5m",
                                                 0, "rename table cross-schema affected rows");
    failures +=
        expect_select_rows(database, "SELECT id, v FROM mylite_rename_b.t5m ORDER BY id",
                           id_v_columns, 2, t2_values, 2, "cross-schema rename preserves rows");
    failures += expect_no_information_schema_table_name_row(database, "t5");
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "t5m",
                                                               .index_name = "uq_v",
                                                               .seq_in_index = 1,
                                                               .column_name = "v",
                                                               .non_unique = 0,
                                                               .collation = "A",
                                                               .sub_part = NULL,
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });
    failures += execute_sql_expect_done_affected(
        database, "ALTER TABLE mylite_rename_b.t5m RENAME mylite_rename_a.t5back", 0,
        "alter table cross-schema rename affected rows");
    failures += expect_select_rows(database, "SELECT id, v FROM t5back ORDER BY id", id_v_columns,
                                   2, t2_values, 2, "alter cross-schema rename preserves rows");

    failures += execute_sql(database, "USE mylite_rename_b", MYLITE_DONE);
    failures +=
        execute_sql(database, "CREATE TABLE mylite_rename_a.selected_src (id INT)", MYLITE_DONE);
    failures +=
        execute_sql(database, "INSERT INTO mylite_rename_a.selected_src VALUES (1)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database, "RENAME TABLE mylite_rename_a.selected_src TO selected_dst", 0,
        "rename table resolves unqualified target through selected schema");
    failures += expect_select_rows(database, "SELECT id FROM selected_dst", id_columns, 1, single_1,
                                   1, "rename selected target rows");
    failures += expect_no_information_schema_table_name_row(database, "selected_src");

    failures += execute_sql(database, "USE mylite_rename_a", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE swap_a (id INT)", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE swap_b (id INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO swap_a VALUES (1)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO swap_b VALUES (2)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database, "RENAME TABLE swap_a TO swap_tmp, swap_b TO swap_a, swap_tmp TO swap_b", 0,
        "rename table swap affected rows");
    failures += expect_select_rows(database, "SELECT id FROM swap_a", id_columns, 1, single_2, 1,
                                   "rename swap first table");
    failures += expect_select_rows(database, "SELECT id FROM swap_b", id_columns, 1, single_1, 1,
                                   "rename swap second table");

    failures += execute_sql(database, "CREATE TABLE coll_src (id INT)", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE coll_dst (id INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO coll_src VALUES (11)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO coll_dst VALUES (22)", MYLITE_DONE);
    failures += prepare_sql(database, "RENAME TABLE coll_src TO coll_dst", MYLITE_OK, &stmt);
    failures +=
        expect_exec_error(stmt, database, "already exists", "rename table rejects existing target");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT id FROM coll_src", id_columns, 1, source_value,
                                   1, "rename collision preserves source");
    failures += expect_select_rows(database, "SELECT id FROM coll_dst", id_columns, 1, target_value,
                                   1, "rename collision preserves target");

    failures += prepare_sql(database, "RENAME TABLE coll_src TO coll_src", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "already exists",
                                  "rename table rejects same source and target");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "CREATE TABLE rollback_src (id INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO rollback_src VALUES (1)", MYLITE_DONE);
    failures += prepare_sql(database,
                            "RENAME TABLE rollback_src TO rollback_done, missing_rename "
                            "TO missing_done",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "doesn't exist",
                                  "rename table rolls back missing later source");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT id FROM rollback_src", id_columns, 1, single_1,
                                   1, "failed multi-rename preserves first source");
    failures += expect_no_information_schema_table_name_row(database, "rollback_done");

    failures += prepare_sql(database, "RENAME TABLE rollback_src TO missing_schema.rollback_dst",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Unknown database",
                                  "rename table rejects missing target schema");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT id FROM rollback_src", id_columns, 1, single_1,
                                   1, "missing schema leaves source table");

    failures += prepare_sql(database,
                            "ALTER TABLE rollback_src RENAME rollback_renamed, "
                            "ADD COLUMN blocked INT",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_UNSUPPORTED,
                              "alter table rename rejects mixed actions");
    failures += expect_contains(mylite_error_message(database), "with other actions",
                                "alter table rename mixed action error");
    failures += expect_int64(mylite_affected_rows(stmt), -1,
                             "alter table rename mixed action affected rows");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT id FROM rollback_src", id_columns, 1, single_1,
                                   1, "mixed alter rename leaves source table");
    failures += expect_no_information_schema_table_name_row(database, "rollback_renamed");

    failures += prepare_sql(database,
                            "ALTER TABLE rollback_src RENAME algorithm_blocked, "
                            "ALGORITHM=INPLACE",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_UNSUPPORTED,
                              "alter table rename rejects non-default algorithm");
    failures += expect_contains(mylite_error_message(database), "ALGORITHM",
                                "alter table rename algorithm error");
    failures +=
        expect_int64(mylite_affected_rows(stmt), -1, "alter table rename algorithm affected rows");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT id FROM rollback_src", id_columns, 1, single_1,
                                   1, "alter rename algorithm leaves source table");
    failures += expect_no_information_schema_table_name_row(database, "algorithm_blocked");

    mylite_close(database);

    failures += expect_status(mylite_open_memory(&no_schema_database), MYLITE_OK,
                              "open no-schema rename database");
    failures +=
        execute_sql(no_schema_database, "CREATE DATABASE mylite_rename_no_default", MYLITE_DONE);
    failures +=
        execute_sql(no_schema_database,
                    "CREATE TABLE mylite_rename_no_default.qualified_src (id INT)", MYLITE_DONE);
    failures +=
        execute_sql(no_schema_database,
                    "INSERT INTO mylite_rename_no_default.qualified_src VALUES (1)", MYLITE_DONE);
    failures += prepare_sql(no_schema_database,
                            "RENAME TABLE mylite_rename_no_default.qualified_src "
                            "TO unqualified_dst",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, no_schema_database, "No database selected",
                                  "rename qualified source requires schema for target");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(
        no_schema_database, "SELECT id FROM mylite_rename_no_default.qualified_src", id_columns, 1,
        single_1, 1, "failed no-default target preserves qualified source");
    failures +=
        execute_sql_expect_done_affected(no_schema_database,
                                         "RENAME TABLE mylite_rename_no_default.qualified_src "
                                         "TO mylite_rename_no_default.qualified_dst",
                                         0, "rename fully-qualified names without selected schema");
    failures += expect_select_rows(
        no_schema_database, "SELECT id FROM mylite_rename_no_default.qualified_dst", id_columns, 1,
        single_1, 1, "qualified rename without selected schema preserves rows");
    mylite_close(no_schema_database);

    remove_runtime_test_files();
    failures += expect_status(mylite_open(path, &file_database), MYLITE_OK,
                              "open rename physical database");
    failures += execute_sql(file_database, "CREATE DATABASE mylite_rename_file", MYLITE_DONE);
    failures += execute_sql(file_database, "USE mylite_rename_file", MYLITE_DONE);
    failures += execute_sql(file_database, "CREATE TABLE physical_src (id INT)", MYLITE_DONE);
    failures +=
        execute_sql_expect_done_affected(file_database, "RENAME TABLE physical_src TO physical_dst",
                                         0, "rename physical table affected rows");
    mylite_close(file_database);

    physical_source = expected_physical_table_name("mylite_rename_file", "physical_src");
    physical_target = expected_physical_table_name("mylite_rename_file", "physical_dst");
    if (physical_source == NULL || physical_target == NULL) {
        fprintf(stderr, "out of memory while building rename physical table names\n");
        failures = 1;
    } else {
        failures += expect_sqlite_table_missing(&(const struct sqlite_table_lookup){
            .path = path,
            .table_name = physical_source,
        });
        failures += expect_sqlite_table_exists(&(const struct sqlite_table_lookup){
            .path = path,
            .table_name = physical_target,
        });
    }
    free(physical_source);
    free(physical_target);
    remove_runtime_test_files();

    // NOLINTEND(readability-function-size,readability-magic-numbers)
    return failures;
}

static int test_truncate_table_execution(void)
{
    // NOLINTBEGIN(readability-function-size,readability-magic-numbers)
    static const char *const count_column[] = {"c"};
    static const char *const id_v_columns[] = {"id", "v"};
    static const char *const zero_count[] = {"0"};
    static const char *const post_truncate_values[] = {"1", "30"};
    static const char *const qualified_count[] = {"0"};
    const char *path = MYLITE_RUNTIME_TEST_FILE_PATH;
    mylite_db *database = NULL;
    mylite_db *file_database = NULL;
    mylite_db *no_schema_database = NULL;
    mylite_stmt *stmt = NULL;
    char *physical_name = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open truncate database");
    failures += prepare_sql(database, "TRUNCATE TABLE no_default", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "No database selected",
                                  "truncate table requires selected schema");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "CREATE DATABASE mylite_truncate_a", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_truncate_a", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE t ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
                            "v INT, UNIQUE KEY uq_v (v), KEY k_v (v)) "
                            "AUTO_INCREMENT=10",
                            MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO t(v) VALUES (10),(20)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(database, "TRUNCATE TABLE t", 0,
                                                 "truncate table affected rows");
    failures += expect_select_rows(database, "SELECT COUNT(*) AS c FROM t", count_column, 1,
                                   zero_count, 1, "truncate removes rows");
    failures += expect_information_schema_table_collation(database,
                                                          &(const struct expected_table_collation){
                                                              .table_name = "t",
                                                              .collation = "utf8mb4_0900_ai_ci",
                                                          });
    failures += expect_information_schema_column_row(database, &(const struct expected_columns_row){
                                                                   .table_name = "t",
                                                                   .column_name = "id",
                                                                   .ordinal_position = 1,
                                                                   .column_default = NULL,
                                                                   .is_nullable = "NO",
                                                                   .data_type = "int",
                                                                   .column_type = "int",
                                                                   .column_key = "PRI",
                                                                   .extra = "auto_increment",
                                                               });
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "t",
                                                               .index_name = "PRIMARY",
                                                               .seq_in_index = 1,
                                                               .column_name = "id",
                                                               .non_unique = 0,
                                                               .collation = "A",
                                                               .sub_part = NULL,
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });
    failures +=
        expect_information_schema_statistics_row(database, &(const struct expected_statistics_row){
                                                               .table_name = "t",
                                                               .index_name = "k_v",
                                                               .seq_in_index = 1,
                                                               .column_name = "v",
                                                               .non_unique = 1,
                                                               .collation = "A",
                                                               .sub_part = NULL,
                                                               .index_comment = "",
                                                               .visible = "YES",
                                                           });

    failures += execute_sql(database, "INSERT INTO t(v) VALUES (30)", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), 1,
                             "truncate resets auto increment id");
    failures += expect_select_rows(database, "SELECT id, v FROM t", id_v_columns, 2,
                                   post_truncate_values, 1, "truncate auto increment row");
    failures += prepare_sql(database, "INSERT INTO t(v) VALUES (30)", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry",
                                  "truncate preserves unique index enforcement");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql_expect_done_affected(database, "TRUNCATE t", 0,
                                                 "truncate shorthand affected rows");
    failures += expect_select_rows(database, "SELECT COUNT(*) AS c FROM t", count_column, 1,
                                   zero_count, 1, "truncate shorthand removes rows");

    failures += execute_sql(database, "CREATE TABLE truncate (id INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO truncate VALUES (1)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(database, "TRUNCATE TABLE truncate", 0,
                                                 "truncate nonreserved table name");
    failures += expect_select_rows(database, "SELECT COUNT(*) AS c FROM truncate", count_column, 1,
                                   zero_count, 1, "truncate nonreserved table removes rows");

    failures += prepare_sql(database, "TRUNCATE TABLE missing", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "doesn't exist",
                                  "truncate rejects missing selected-schema table");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "TRUNCATE TABLE missing_schema.t", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "doesn't exist",
                                  "truncate rejects missing qualified schema");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "TRUNCATE TABLE information_schema.tables", MYLITE_OK, &stmt);
    failures +=
        expect_exec_error(stmt, database, "system schema", "truncate rejects system schema");
    mylite_finalize(stmt);
    stmt = NULL;

    mylite_close(database);

    failures += expect_status(mylite_open_memory(&no_schema_database), MYLITE_OK,
                              "open truncate no-schema database");
    failures +=
        execute_sql(no_schema_database, "CREATE DATABASE mylite_truncate_no_default", MYLITE_DONE);
    failures += execute_sql(no_schema_database,
                            "CREATE TABLE mylite_truncate_no_default.q (id INT)", MYLITE_DONE);
    failures += execute_sql(no_schema_database,
                            "INSERT INTO mylite_truncate_no_default.q VALUES (1)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        no_schema_database, "TRUNCATE TABLE mylite_truncate_no_default.q", 0,
        "truncate qualified target without selected schema");
    failures += expect_select_rows(
        no_schema_database, "SELECT COUNT(*) AS c FROM mylite_truncate_no_default.q", count_column,
        1, qualified_count, 1, "qualified truncate without selected schema removes rows");
    mylite_close(no_schema_database);

    remove_runtime_test_files();
    failures += expect_status(mylite_open(path, &file_database), MYLITE_OK,
                              "open truncate physical database");
    failures += execute_sql(file_database, "CREATE DATABASE mylite_truncate_file", MYLITE_DONE);
    failures += execute_sql(file_database, "USE mylite_truncate_file", MYLITE_DONE);
    failures += execute_sql(file_database, "CREATE TABLE physical_t (id INT)", MYLITE_DONE);
    failures += execute_sql(file_database, "INSERT INTO physical_t VALUES (1)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(file_database, "TRUNCATE TABLE physical_t", 0,
                                                 "truncate physical affected rows");
    mylite_close(file_database);

    physical_name = expected_physical_table_name("mylite_truncate_file", "physical_t");
    if (physical_name == NULL) {
        fprintf(stderr, "out of memory while building truncate physical table name\n");
        failures = 1;
    } else {
        failures += expect_sqlite_table_exists(&(const struct sqlite_table_lookup){
            .path = path,
            .table_name = physical_name,
        });
    }
    free(physical_name);
    remove_runtime_test_files();

    // NOLINTEND(readability-function-size,readability-magic-numbers)
    return failures;
}

static int test_show_tables_execution(void)
{
    // NOLINTBEGIN(readability-function-size,readability-magic-numbers)
    static const char *const show_a_columns[] = {"Tables_in_mylite_show_tables_a"};
    static const char *const show_full_a_columns[] = {"Tables_in_mylite_show_tables_a",
                                                      "Table_type"};
    static const char *const alpha_columns[] = {"Tables_in_mylite_show_tables_a (alpha%)"};
    static const char *const beta_columns[] = {"Tables_in_mylite_show_tables_a (beta\\_%)"};
    static const char *const camel_columns[] = {"Tables_in_mylite_show_tables_a (Camel%)"};
    static const char *const camel_lower_columns[] = {"Tables_in_mylite_show_tables_a (camel%)"};
    static const char *const show_b_columns[] = {"Tables_in_mylite_show_tables_b"};
    static const char *const show_empty_columns[] = {"Tables_in_mylite_show_tables_empty"};
    static const char *const keyword_tables_columns[] = {"Tables_in_mylite_show_tables_keywords "
                                                         "(tables)"};
    static const char *const keyword_full_columns[] = {"Tables_in_mylite_show_tables_keywords "
                                                       "(full)"};
    static const char *const keyword_extended_columns[] = {"Tables_in_mylite_show_tables_keywords "
                                                           "(extended)"};
    static const char *const info_columns[] = {"Tables_in_information_schema (TABLES)"};
    static const char *const info_full_columns[] = {"Tables_in_information_schema (TABLES)",
                                                    "Table_type"};
    static const char *const info_mixed_columns[] = {"Tables_in_information_schema (TABLES)"};
    static const char *const show_a_values[] = {"alpha", "alpha_extra", "beta_1"};
    static const char *const show_full_a_values[] = {
        "alpha", "BASE TABLE", "alpha_extra", "BASE TABLE", "beta_1", "BASE TABLE",
    };
    static const char *const alpha_values[] = {"alpha", "alpha_extra"};
    static const char *const beta_values[] = {"beta_1"};
    static const char *const camel_values[] = {"CamelCase"};
    static const char *const show_b_values[] = {"in_table"};
    static const char *const keyword_tables_values[] = {"tables"};
    static const char *const keyword_full_values[] = {"full"};
    static const char *const keyword_extended_values[] = {"extended"};
    static const char *const info_values[] = {"TABLES"};
    static const char *const info_full_values[] = {"TABLES", "SYSTEM VIEW"};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_status(mylite_open_memory(&database), MYLITE_OK, "open show tables database");
    failures +=
        expect_prepare_error(database, "SHOW TABLES", MYLITE_EXEC_ERROR, "No database selected",
                             "show tables requires selected schema");

    failures += execute_sql(database, "CREATE DATABASE mylite_show_tables_a", MYLITE_DONE);
    failures += execute_sql(database, "CREATE DATABASE mylite_show_tables_b", MYLITE_DONE);
    failures += execute_sql(database, "CREATE DATABASE mylite_show_tables_empty", MYLITE_DONE);
    failures += execute_sql(database, "CREATE DATABASE mylite_show_tables_keywords", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_show_tables_a", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE alpha (id INT)", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE alpha_extra (id INT)", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE beta_1 (id INT)", MYLITE_DONE);

    failures += expect_select_rows(database, "SHOW TABLES", show_a_columns, 1, show_a_values, 3,
                                   "show tables selected schema");
    failures += expect_select_rows(database, "SHOW FULL TABLES", show_full_a_columns, 2,
                                   show_full_a_values, 3, "show full tables selected schema");
    failures += expect_select_rows(database, "SHOW EXTENDED TABLES", show_a_columns, 1,
                                   show_a_values, 3, "show extended tables selected schema");
    failures +=
        expect_select_rows(database, "SHOW EXTENDED FULL TABLES", show_full_a_columns, 2,
                           show_full_a_values, 3, "show extended full tables selected schema");
    failures += expect_select_rows(database, "SHOW TABLES LIKE 'alpha%'", alpha_columns, 1,
                                   alpha_values, 2, "show tables like alpha");
    failures +=
        expect_select_rows(database, "SHOW TABLES FROM mylite_show_tables_a LIKE 'beta\\_%'",
                           beta_columns, 1, beta_values, 1, "show tables escaped underscore");
    failures += expect_prepare_error(database, "SHOW FULL TABLES WHERE Table_type = 'BASE TABLE'",
                                     MYLITE_UNSUPPORTED, "SHOW TABLES WHERE is not supported",
                                     "show tables where is parsed but unsupported");

    failures += execute_sql(database, "CREATE TABLE CamelCase (id INT)", MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW TABLES LIKE 'camel%'", camel_lower_columns, 1,
                                   NULL, 0, "show tables like is case-sensitive");
    failures += expect_select_rows(database, "SHOW TABLES LIKE 'Camel%'", camel_columns, 1,
                                   camel_values, 1, "show tables like uppercase pattern");

    failures +=
        execute_sql(database, "CREATE TABLE mylite_show_tables_b.in_table (id INT)", MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW TABLES FROM mylite_show_tables_b",
                                   show_b_columns, 1, show_b_values, 1, "show tables from schema");
    failures += expect_select_rows(database, "SHOW TABLES IN mylite_show_tables_b", show_b_columns,
                                   1, show_b_values, 1, "show tables in schema");
    failures += expect_select_rows(database, "SHOW TABLES FROM mylite_show_tables_empty",
                                   show_empty_columns, 1, NULL, 0, "show tables empty schema");

    failures += execute_sql(database, "USE mylite_show_tables_keywords", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE tables (id INT)", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE full (id INT)", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE extended (id INT)", MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW TABLES LIKE 'tables'", keyword_tables_columns, 1,
                                   keyword_tables_values, 1, "show tables keyword table name");
    failures += expect_select_rows(database, "SHOW TABLES LIKE 'full'", keyword_full_columns, 1,
                                   keyword_full_values, 1, "show tables full table name");
    failures +=
        expect_select_rows(database, "SHOW TABLES LIKE 'extended'", keyword_extended_columns, 1,
                           keyword_extended_values, 1, "show tables extended table name");

    failures +=
        expect_select_rows(database, "SHOW TABLES FROM information_schema LIKE 'TABLES'",
                           info_columns, 1, info_values, 1, "show tables information schema");
    failures += expect_select_rows(
        database, "SHOW FULL TABLES FROM information_schema LIKE 'tables'", info_full_columns, 2,
        info_full_values, 1, "show full tables information schema lower-case pattern");
    failures += expect_select_rows(database, "SHOW TABLES FROM Information_Schema LIKE 'tables'",
                                   info_mixed_columns, 1, info_values, 1,
                                   "show tables information schema mixed-case schema");
    failures += expect_prepare_error(database, "SHOW TABLES FROM missing_show_tables_schema",
                                     MYLITE_EXEC_ERROR, "Unknown database",
                                     "show tables rejects missing schema");

    mylite_close(database);

    // NOLINTEND(readability-function-size,readability-magic-numbers)
    return failures;
}

static int test_show_variables_execution(void)
{
    // NOLINTBEGIN(readability-function-size,readability-magic-numbers)
    static const char *const columns[] = {"Variable_name", "Value"};
    static const char *const autocommit_values[] = {"autocommit", "ON"};
    static const char *const initial_charset_values[] = {
        "character_set_client",   "utf8mb4", "character_set_connection", "utf8mb4",
        "character_set_database", "utf8mb4", "character_set_filesystem", "binary",
        "character_set_results",  "utf8mb4", "character_set_server",     "utf8mb4",
        "character_set_system",   "utf8mb3",
    };
    static const char *const unescaped_charset_values[] = {
        "character_set_client",   "utf8mb4", "character_set_connection", "utf8mb4",
        "character_set_database", "utf8mb4", "character_set_filesystem", "binary",
        "character_set_results",  "utf8mb4", "character_set_server",     "utf8mb4",
        "character_set_system",   "utf8mb3", "character_sets_dir",       "",
    };
    static const char *const latin1_session_charset_values[] = {
        "character_set_client",   "latin1",  "character_set_connection", "latin1",
        "character_set_database", "utf8mb4", "character_set_filesystem", "binary",
        "character_set_results",  "latin1",  "character_set_server",     "utf8mb4",
        "character_set_system",   "utf8mb3",
    };
    static const char *const global_charset_values[] = {
        "character_set_client",   "utf8mb4", "character_set_connection", "utf8mb4",
        "character_set_database", "utf8mb4", "character_set_filesystem", "binary",
        "character_set_results",  "utf8mb4", "character_set_server",     "utf8mb4",
        "character_set_system",   "utf8mb3",
    };
    static const char *const latin1_selected_charset_values[] = {
        "character_set_client",   "utf8mb4", "character_set_connection", "latin1",
        "character_set_database", "latin1",  "character_set_filesystem", "binary",
        "character_set_results",  "utf8mb4", "character_set_server",     "utf8mb4",
        "character_set_system",   "utf8mb3",
    };
    static const char *const session_collation_values[] = {"collation_connection", "latin1_bin"};
    static const char *const global_collation_values[] = {"collation_connection",
                                                          "utf8mb4_0900_ai_ci"};
    static const char *const reset_collation_values[] = {"collation_connection",
                                                         "utf8mb4_0900_ai_ci"};
    static const char *const selected_database_collation_values[] = {"collation_database",
                                                                     "latin1_bin"};
    static const char *const selected_connection_collation_values[] = {"collation_connection",
                                                                       "latin1_bin"};
    static const char *const global_database_collation_values[] = {"collation_database",
                                                                   "utf8mb4_0900_ai_ci"};
    static const char *const warning_count_values[] = {"warning_count", "0"};
    static const char *const error_count_values[] = {"error_count", "0"};
    static const char *const diagnostics_columns[] = {"Level", "Code", "Message"};
    const char *const version_values[] = {
        "version",
        mylite_version(),
        "version_comment",
        "MyLite",
        "version_compile_machine",
        "",
        "version_compile_os",
        "",
        "version_compile_zlib",
        "",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_status(mylite_open_memory(&database), MYLITE_OK, "open show variables database");

    failures += expect_select_rows(database, "SHOW VARIABLES LIKE 'autocommit'", columns, 2,
                                   autocommit_values, 1, "show variables autocommit");
    failures +=
        expect_select_rows(database, "SHOW VARIABLES LIKE 'character\\_set\\_%'", columns, 2,
                           initial_charset_values, 7, "show variables escaped charset pattern");
    failures += expect_select_rows(database, "SHOW VARIABLES LIKE 'character_set_%'", columns, 2,
                                   unescaped_charset_values, 8,
                                   "show variables unescaped charset wildcard");
    failures +=
        expect_select_rows(database, "SHOW VARIABLES LIKE 'CHARACTER\\_SET\\_%'", columns, 2,
                           initial_charset_values, 7, "show variables like is case-insensitive");
    failures += expect_select_rows(database, "SHOW VARIABLES LIKE 'version%'", columns, 2,
                                   version_values, 5, "show variables version rows");
    failures += expect_select_rows(database, "SHOW VARIABLES LIKE 'no_such_variable'", columns, 2,
                                   NULL, 0, "show variables empty like result");
    failures += expect_show_variables_contains(
        database, &(const struct show_variable_expectation){
                      .sql = "SHOW VARIABLES",
                      .variable_name = "sql_mode",
                      .value = "ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,"
                               "NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,"
                               "NO_ENGINE_SUBSTITUTION",
                      .context = "show variables unfiltered sql mode",
                  });
    failures +=
        expect_show_variables_contains(database, &(const struct show_variable_expectation){
                                                     .sql = "SHOW VARIABLES",
                                                     .variable_name = "version",
                                                     .value = mylite_version(),
                                                     .context = "show variables unfiltered version",
                                                 });
    failures += expect_prepare_error(database, "SHOW VARIABLES WHERE Variable_name = 'autocommit'",
                                     MYLITE_UNSUPPORTED, "SHOW VARIABLES WHERE is not supported",
                                     "show variables where is parsed but unsupported");
    failures += expect_prepare_error(database, "SHOW VARIABLES WHERE Value = 'ON'",
                                     MYLITE_UNSUPPORTED, "SHOW VARIABLES WHERE is not supported",
                                     "show variables where value column is parsed but unsupported");
    failures += expect_prepare_error(database, "SHOW VARIABLES LIMIT 1", MYLITE_PARSE_ERROR,
                                     "syntax_error", "show variables limit syntax");

    failures += execute_sql(database, "SET NAMES latin1 COLLATE latin1_bin", MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW SESSION VARIABLES LIKE 'character\\_set\\_%'",
                                   columns, 2, latin1_session_charset_values, 7,
                                   "show session variables after set names");
    failures += expect_select_rows(database, "SHOW LOCAL VARIABLES LIKE 'character\\_set\\_%'",
                                   columns, 2, latin1_session_charset_values, 7,
                                   "show local variables matches session");
    failures +=
        expect_select_rows(database, "SHOW GLOBAL VARIABLES LIKE 'character\\_set\\_%'", columns, 2,
                           global_charset_values, 7, "show global variables charset defaults");
    failures += expect_select_rows(database, "SHOW SESSION VARIABLES LIKE 'collation_connection'",
                                   columns, 2, session_collation_values, 1,
                                   "show variables session collation after set names");
    failures += expect_select_rows(database, "SHOW GLOBAL VARIABLES LIKE 'collation_connection'",
                                   columns, 2, global_collation_values, 1,
                                   "show variables global collation default");
    failures += expect_select_rows(database, "SHOW GLOBAL VARIABLES LIKE 'warning_count'", columns,
                                   2, NULL, 0, "show global variables omits warning count");
    failures += expect_select_rows(database, "SHOW GLOBAL VARIABLES LIKE 'error_count'", columns, 2,
                                   NULL, 0, "show global variables omits error count");

    failures += execute_sql(database, "SET CHARACTER SET utf8mb4", MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW VARIABLES LIKE 'collation_connection'", columns,
                                   2, reset_collation_values, 1,
                                   "show variables set character set resets collation");

    failures += execute_sql(database,
                            "CREATE DATABASE mylite_show_variables_latin1 "
                            "DEFAULT CHARACTER SET latin1 COLLATE latin1_bin",
                            MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_show_variables_latin1", MYLITE_DONE);
    failures += execute_sql(database, "SET CHARACTER SET utf8mb4", MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW VARIABLES LIKE 'character\\_set\\_%'", columns,
                                   2, latin1_selected_charset_values, 7,
                                   "show variables selected schema charset defaults");
    failures += expect_select_rows(database, "SHOW VARIABLES LIKE 'collation_database'", columns, 2,
                                   selected_database_collation_values, 1,
                                   "show variables selected schema collation database");
    failures += expect_select_rows(database, "SHOW VARIABLES LIKE 'collation_connection'", columns,
                                   2, selected_connection_collation_values, 1,
                                   "show variables selected schema collation connection");
    failures += expect_select_rows(database, "SHOW GLOBAL VARIABLES LIKE 'collation_database'",
                                   columns, 2, global_database_collation_values, 1,
                                   "show global variables database collation default");

    failures +=
        expect_select_rows(database, "SELECT 1/0 AS divzero", (const char *const[]){"divzero"}, 1,
                           (const char *const[]){NULL}, 1, "show variables warning source");
    failures += expect_select_rows(database, "SHOW VARIABLES LIKE 'warning_count'", columns, 2,
                                   warning_count_values, 1, "show variables clears warning count");
    failures += expect_select_rows(database, "SHOW WARNINGS", diagnostics_columns, 3, NULL, 0,
                                   "show variables cleared diagnostics");

    failures += execute_sql(database, "CREATE TABLE show_variable_errors (id INT)", MYLITE_DONE);
    failures +=
        expect_prepare_error(database, "SELECT missing_show_variable FROM show_variable_errors",
                             MYLITE_EXEC_ERROR, "Unknown column", "show variables error source");
    failures += expect_select_rows(database, "SHOW VARIABLES LIKE 'error_count'", columns, 2,
                                   error_count_values, 1, "show variables clears error count");

    mylite_close(database);
    // NOLINTEND(readability-function-size,readability-magic-numbers)
    return failures;
}

static int expect_show_variables_contains(mylite_db *database,
                                          const struct show_variable_expectation *expected)
{
    static const char *const columns[] = {"Variable_name", "Value"};
    mylite_stmt *stmt = NULL;
    int failures = prepare_sql(database, expected->sql, MYLITE_OK, &stmt);
    int saw_variable = 0;

    failures += expect_column_names(stmt, columns, 2, expected->context);
    while (failures == 0) {
        int status = mylite_step(stmt);

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, expected->context);
        if (failures != 0) {
            break;
        }
        if (strcmp(mylite_column_text(stmt, 0), expected->variable_name) == 0) {
            saw_variable = 1;
            failures +=
                expect_string(mylite_column_text(stmt, 1), expected->value, expected->context);
            break;
        }
    }
    if (!saw_variable) {
        fprintf(stderr, "%s: SHOW VARIABLES did not return '%s'\n", expected->context,
                expected->variable_name);
        failures = 1;
    }
    failures += expect_int64(mylite_affected_rows(stmt), -1, expected->context);
    mylite_finalize(stmt);
    return failures;
}

static int test_show_status_execution(void)
{
    // NOLINTBEGIN(readability-function-size,readability-magic-numbers)
    static const char *const columns[] = {"Variable_name", "Value"};
    static const char *const diagnostics_columns[] = {"Level", "Code", "Message"};
    static const char *const uptime_names[] = {"Uptime", "Uptime_since_flush_status"};
    static const char *const threads_values[] = {
        "Threads_cached",  "0", "Threads_connected", "1",
        "Threads_created", "1", "Threads_running",   "1",
    };
    static const char *const connections_values[] = {"Connections", "1"};
    static const char *const questions_values[] = {"Questions", "0"};
    static const char *const com_select_values[] = {"Com_select", "0"};
    static const char *const com_show_values[] = {
        "Com_show_errors",   "0", "Com_show_fields", "0", "Com_show_keys",      "0",
        "Com_show_status",   "0", "Com_show_tables", "0", "Com_show_variables", "0",
        "Com_show_warnings", "0",
    };
    static const struct show_status_row_expectation catalog_rows[] = {
        {"Com_begin", "0"},
        {"Com_commit", "0"},
        {"Com_create_db", "0"},
        {"Com_create_index", "0"},
        {"Com_create_table", "0"},
        {"Com_delete", "0"},
        {"Com_drop_db", "0"},
        {"Com_drop_index", "0"},
        {"Com_drop_table", "0"},
        {"Com_insert", "0"},
        {"Com_release_savepoint", "0"},
        {"Com_rename_table", "0"},
        {"Com_replace", "0"},
        {"Com_rollback", "0"},
        {"Com_rollback_to_savepoint", "0"},
        {"Com_savepoint", "0"},
        {"Com_select", "0"},
        {"Com_set_option", "0"},
        {"Com_show_errors", "0"},
        {"Com_show_fields", "0"},
        {"Com_show_keys", "0"},
        {"Com_show_status", "0"},
        {"Com_show_tables", "0"},
        {"Com_show_variables", "0"},
        {"Com_show_warnings", "0"},
        {"Com_truncate", "0"},
        {"Com_update", "0"},
        {"Connections", "1"},
        {"Questions", "0"},
        {"Threads_cached", "0"},
        {"Threads_connected", "1"},
        {"Threads_created", "1"},
        {"Threads_running", "1"},
        {"Uptime", NULL},
        {"Uptime_since_flush_status", NULL},
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open show status");

    failures += expect_show_status_catalog_rows(
        database, &(const struct show_status_catalog_expectation){
                      .sql = "SHOW STATUS",
                      .rows = catalog_rows,
                      .row_count = (int)(sizeof(catalog_rows) / sizeof(catalog_rows[0])),
                      .context = "show status ordered catalog",
                  });
    failures +=
        expect_show_status_numeric_rows(database, &(const struct show_status_numeric_expectation){
                                                      .sql = "SHOW STATUS LIKE 'Uptime%'",
                                                      .variable_names = uptime_names,
                                                      .row_count = 2,
                                                      .context = "show status uptime rows",
                                                  });
    failures += expect_select_rows(database, "SHOW STATUS LIKE 'Threads%'", columns, 2,
                                   threads_values, 4, "show status thread rows");
    failures += expect_select_rows(database, "SHOW STATUS LIKE 'threads_%'", columns, 2,
                                   threads_values, 4, "show status like is case-insensitive");
    failures +=
        expect_select_rows(database, "SHOW STATUS LIKE 'THREADS\\_%'", columns, 2, threads_values,
                           4, "show status escaped like is case-insensitive");
    failures += expect_select_rows(database, "SHOW SESSION STATUS LIKE 'Threads%'", columns, 2,
                                   threads_values, 4, "show session status threads");
    failures += expect_select_rows(database, "SHOW LOCAL STATUS LIKE 'Threads%'", columns, 2,
                                   threads_values, 4, "show local status threads");
    failures += expect_select_rows(database, "SHOW GLOBAL STATUS LIKE 'Threads%'", columns, 2,
                                   threads_values, 4, "show global status threads");
    failures += expect_select_rows(database, "SHOW STATUS LIKE 'Connections'", columns, 2,
                                   connections_values, 1, "show status connections");
    failures += expect_select_rows(database, "SHOW STATUS LIKE 'Questions'", columns, 2,
                                   questions_values, 1, "show status questions placeholder");
    failures += expect_select_rows(database, "SHOW STATUS LIKE 'Com_select'", columns, 2,
                                   com_select_values, 1, "show status com select placeholder");
    failures += expect_select_rows(database, "SHOW STATUS LIKE 'Com\\_show\\_%'", columns, 2,
                                   com_show_values, 7, "show status escaped com show rows");
    failures += expect_select_rows(database, "SHOW STATUS LIKE 'no_such_status'", columns, 2, NULL,
                                   0, "show status empty like result");

    failures += expect_prepare_error(database, "SHOW STATUS WHERE Variable_name = 'Uptime'",
                                     MYLITE_UNSUPPORTED, "SHOW STATUS WHERE is not supported",
                                     "show status where is parsed but unsupported");
    failures += expect_prepare_error(database, "SHOW STATUS WHERE Value = '0'", MYLITE_UNSUPPORTED,
                                     "SHOW STATUS WHERE is not supported",
                                     "show status where value is parsed but unsupported");
    failures += expect_prepare_error(database, "SHOW STATUS LIMIT 1", MYLITE_PARSE_ERROR,
                                     "syntax_error", "show status limit syntax");

    failures += execute_sql(database, "CREATE DATABASE mylite_show_status", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_show_status", MYLITE_DONE);
    failures +=
        expect_prepare_error(database, "SELECT * FROM missing_show_status_table", MYLITE_EXEC_ERROR,
                             "doesn't exist", "show status error source");
    failures += expect_select_rows(database, "SHOW STATUS LIKE 'Questions'", columns, 2,
                                   questions_values, 1, "show status clears diagnostics");
    failures += expect_select_rows(database, "SHOW ERRORS", diagnostics_columns, 3, NULL, 0,
                                   "show status cleared diagnostics");

    mylite_close(database);
    // NOLINTEND(readability-function-size,readability-magic-numbers)
    return failures;
}

static int expect_show_status_catalog_rows(mylite_db *database,
                                           const struct show_status_catalog_expectation *expected)
{
    static const char *const columns[] = {"Variable_name", "Value"};
    mylite_stmt *stmt = NULL;
    int failures = prepare_sql(database, expected->sql, MYLITE_OK, &stmt);

    failures += expect_column_names(stmt, columns, 2, expected->context);
    for (int row = 0; row < expected->row_count; ++row) {
        failures += expect_status(mylite_step(stmt), MYLITE_ROW, expected->context);
        failures += expect_string(mylite_column_text(stmt, 0), expected->rows[row].variable_name,
                                  expected->context);
        if (expected->rows[row].value == NULL) {
            failures +=
                expect_unsigned_decimal_text(mylite_column_text(stmt, 1), expected->context);
        } else {
            failures += expect_string(mylite_column_text(stmt, 1), expected->rows[row].value,
                                      expected->context);
        }
    }
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, expected->context);
    failures += expect_int64(mylite_affected_rows(stmt), -1, expected->context);
    mylite_finalize(stmt);
    return failures;
}

static int expect_show_status_numeric_rows(mylite_db *database,
                                           const struct show_status_numeric_expectation *expected)
{
    static const char *const columns[] = {"Variable_name", "Value"};
    mylite_stmt *stmt = NULL;
    int failures = prepare_sql(database, expected->sql, MYLITE_OK, &stmt);

    failures += expect_column_names(stmt, columns, 2, expected->context);
    for (int row = 0; row < expected->row_count; ++row) {
        failures += expect_status(mylite_step(stmt), MYLITE_ROW, expected->context);
        failures += expect_string(mylite_column_text(stmt, 0), expected->variable_names[row],
                                  expected->context);
        failures += expect_unsigned_decimal_text(mylite_column_text(stmt, 1), expected->context);
    }
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, expected->context);
    failures += expect_int64(mylite_affected_rows(stmt), -1, expected->context);
    mylite_finalize(stmt);
    return failures;
}

static int test_show_engines_execution(void)
{
    // NOLINTBEGIN(readability-function-size,readability-magic-numbers)
    static const char *const columns[] = {"Engine",       "Support", "Comment",
                                          "Transactions", "XA",      "Savepoints"};
    static const char *const diagnostics_columns[] = {"Level", "Code", "Message"};
    static const char *const warning_count_column[] = {"@@session.warning_count"};
    static const char *const error_count_column[] = {"@@session.error_count"};
    static const char *const zero_count[] = {"0"};
    static const char *const one_count[] = {"1"};
    static const char *const values[] = {
        "InnoDB",     "DEFAULT", "MyLite SQLite-backed transactional engine facade",
        "YES",        "NO",      "YES",
        "MEMORY",     "NO",      "In-memory tables are not supported by MyLite",
        NULL,         NULL,      NULL,
        "MyISAM",     "NO",      "MyISAM tables are not supported by MyLite",
        NULL,         NULL,      NULL,
        "FEDERATED",  "NO",      "Federated tables are not supported by MyLite",
        NULL,         NULL,      NULL,
        "MRG_MYISAM", "NO",      "Merge MyISAM tables are not supported by MyLite",
        NULL,         NULL,      NULL,
        "BLACKHOLE",  "NO",      "Blackhole tables are not supported by MyLite",
        NULL,         NULL,      NULL,
        "CSV",        "NO",      "CSV-backed tables are not supported by MyLite",
        NULL,         NULL,      NULL,
        "ARCHIVE",    "NO",      "Archive tables are not supported by MyLite",
        NULL,         NULL,      NULL,
    };
    static const struct expected_result_metadata metadata[] = {
        {"Engine", NULL, NULL, NULL, NULL, NULL, 64U, MYLITE_FIELD_TYPE_VAR_STRING, 31U, 8U,
         MYLITE_FIELD_FLAG_NOT_NULL, 0U, 0},
        {"Support", NULL, NULL, NULL, NULL, NULL, 8U, MYLITE_FIELD_TYPE_VAR_STRING, 31U, 8U,
         MYLITE_FIELD_FLAG_NOT_NULL, 0U, 0},
        {"Comment", NULL, NULL, NULL, NULL, NULL, 80U, MYLITE_FIELD_TYPE_VAR_STRING, 31U, 8U,
         MYLITE_FIELD_FLAG_NOT_NULL, 0U, 0},
        {"Transactions", NULL, NULL, NULL, NULL, NULL, 3U, MYLITE_FIELD_TYPE_VAR_STRING, 31U, 8U,
         0U, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"XA", NULL, NULL, NULL, NULL, NULL, 3U, MYLITE_FIELD_TYPE_VAR_STRING, 31U, 8U, 0U,
         MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"Savepoints", NULL, NULL, NULL, NULL, NULL, 3U, MYLITE_FIELD_TYPE_VAR_STRING, 31U, 8U, 0U,
         MYLITE_FIELD_FLAG_NOT_NULL, 1},
    };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open show engines");

    failures += expect_select_rows(database, "SHOW ENGINES", columns, 6, values, 8,
                                   "show engines registry");
    failures += expect_select_rows(database, "SHOW STORAGE ENGINES", columns, 6, values, 8,
                                   "show storage engines registry");

    failures += prepare_sql(database, "SHOW ENGINES", MYLITE_OK, &stmt);
    failures += expect_result_metadata(stmt, metadata, 6, "show engines metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "show engines metadata first row");
    failures +=
        expect_string(mylite_column_text(stmt, 0), "InnoDB", "show engines metadata first engine");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "show engines affected rows");
    mylite_finalize(stmt);

    failures += expect_prepare_error(database, "SHOW ENGINES LIKE 'InnoDB'", MYLITE_PARSE_ERROR,
                                     "syntax_error", "show engines like syntax");
    failures +=
        expect_prepare_error(database, "SHOW ENGINES WHERE Engine = 'InnoDB'", MYLITE_PARSE_ERROR,
                             "syntax_error", "show engines where syntax");
    failures += expect_prepare_error(database, "SHOW ENGINES LIMIT 1", MYLITE_PARSE_ERROR,
                                     "syntax_error", "show engines limit syntax");
    failures +=
        expect_prepare_error(database, "SHOW STORAGE ENGINES LIKE 'InnoDB'", MYLITE_PARSE_ERROR,
                             "syntax_error", "show storage engines like syntax");
    failures += expect_prepare_error(database, "SHOW STORAGE ENGINES WHERE Engine = 'InnoDB'",
                                     MYLITE_PARSE_ERROR, "syntax_error",
                                     "show storage engines where syntax");

    failures += execute_sql(database, "CREATE DATABASE mylite_show_engines", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_show_engines", MYLITE_DONE);
    failures +=
        expect_prepare_error(database, "SELECT * FROM missing_show_engines_table",
                             MYLITE_EXEC_ERROR, "doesn't exist", "show engines error source");
    failures += expect_select_rows(database, "SHOW COUNT(*) ERRORS", error_count_column, 1,
                                   one_count, 1, "show engines source error count");
    failures += expect_select_rows(database, "SHOW ENGINES", columns, 6, values, 8,
                                   "show engines clears diagnostics");
    failures += expect_select_rows(database, "SHOW COUNT(*) ERRORS", error_count_column, 1,
                                   zero_count, 1, "show engines cleared error count");
    failures += expect_select_rows(database, "SHOW ERRORS", diagnostics_columns, 3, NULL, 0,
                                   "show engines cleared diagnostics");

    failures +=
        expect_select_rows(database, "SELECT 1/0 AS divzero", (const char *const[]){"divzero"}, 1,
                           (const char *const[]){NULL}, 1, "show engines warning source");
    failures += expect_select_rows(database, "SHOW COUNT(*) WARNINGS", warning_count_column, 1,
                                   one_count, 1, "show engines source warning count");
    failures += expect_select_rows(database, "SHOW ENGINES", columns, 6, values, 8,
                                   "show engines clears warning diagnostics");
    failures += expect_select_rows(database, "SHOW COUNT(*) WARNINGS", warning_count_column, 1,
                                   zero_count, 1, "show engines cleared warning count");
    failures += expect_select_rows(database, "SHOW WARNINGS", diagnostics_columns, 3, NULL, 0,
                                   "show engines cleared warnings");

    mylite_close(database);
    // NOLINTEND(readability-function-size,readability-magic-numbers)
    return failures;
}

static int test_show_character_set_execution(void)
{
    // NOLINTBEGIN(readability-function-size,readability-magic-numbers)
    static const char *const columns[] = {"Charset", "Description", "Default collation", "Maxlen"};
    static const char *const diagnostics_columns[] = {"Level", "Code", "Message"};
    static const char *const error_count_column[] = {"@@session.error_count"};
    static const char *const zero_count[] = {"0"};
    static const char *const one_count[] = {"1"};
    static const char *const all_values[] = {
        "binary",
        "Binary pseudo charset",
        "binary",
        "1",
        "latin1",
        "cp1252 West European",
        "latin1_swedish_ci",
        "1",
        "utf8mb3",
        "UTF-8 Unicode",
        "utf8mb3_general_ci",
        "3",
        "utf8mb4",
        "UTF-8 Unicode",
        "utf8mb4_0900_ai_ci",
        "4",
    };
    static const char *const utf8_values[] = {
        "utf8mb3", "UTF-8 Unicode", "utf8mb3_general_ci", "3",
        "utf8mb4", "UTF-8 Unicode", "utf8mb4_0900_ai_ci", "4",
    };
    static const char *const binary_values[] = {"binary", "Binary pseudo charset", "binary", "1"};
    static const char *const latin1_values[] = {"latin1", "cp1252 West European",
                                                "latin1_swedish_ci", "1"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open show character set");

    failures += expect_select_rows(database, "SHOW CHARACTER SET", columns, 4, all_values, 4,
                                   "show character set supported catalog");
    failures += expect_select_rows(database, "SHOW CHARSET", columns, 4, all_values, 4,
                                   "show charset synonym");
    failures += expect_select_rows(database, "SHOW CHAR SET", columns, 4, all_values, 4,
                                   "show char set synonym");
    failures += expect_select_rows(database, "SHOW CHARACTER SET LIKE 'utf8%'", columns, 4,
                                   utf8_values, 2, "show character set utf8 wildcard");
    failures += expect_select_rows(database, "SHOW CHARACTER SET LIKE 'UTF8%'", columns, 4,
                                   utf8_values, 2, "show character set like is case-insensitive");
    failures += expect_select_rows(database, "SHOW CHARSET LIKE 'utf8%'", columns, 4, utf8_values,
                                   2, "show charset like synonym");
    failures += expect_select_rows(database, "SHOW CHAR SET LIKE 'utf8%'", columns, 4, utf8_values,
                                   2, "show char set like synonym");
    failures += expect_select_rows(database, "SHOW CHARACTER SET LIKE 'utf8\\_mb%'", columns, 4,
                                   NULL, 0, "show character set escaped underscore");
    failures += expect_select_rows(database, "SHOW CHARACTER SET LIKE 'utf8_mb%'", columns, 4, NULL,
                                   0, "show character set unescaped underscore miss");
    failures += expect_select_rows(database, "SHOW CHARACTER SET LIKE 'binary'", columns, 4,
                                   binary_values, 1, "show character set binary row");
    failures += expect_select_rows(database, "SHOW CHARACTER SET LIKE 'latin%'", columns, 4,
                                   latin1_values, 1, "show character set latin subset");
    failures += expect_select_rows(database, "SHOW CHARACTER SET LIKE 'filename'", columns, 4, NULL,
                                   0, "show character set hides filename");
    failures += expect_show_character_set_maxlen_int64(database, "SHOW CHARACTER SET LIKE 'binary'",
                                                       1, "show character set maxlen is integer");

    failures +=
        expect_prepare_error(database, "SHOW CHARACTER SET WHERE Charset = 'utf8mb4'",
                             MYLITE_UNSUPPORTED, "SHOW CHARACTER SET WHERE is not supported",
                             "show character set where is parsed but unsupported");
    failures +=
        expect_prepare_error(database, "SHOW CHARACTER SET WHERE `Default collation` = 'binary'",
                             MYLITE_UNSUPPORTED, "SHOW CHARACTER SET WHERE is not supported",
                             "show character set where default collation unsupported");
    failures += expect_prepare_error(database, "SHOW CHARACTER SET LIKE 1", MYLITE_PARSE_ERROR,
                                     "syntax_error", "show character set non-string like syntax");
    failures += expect_prepare_error(database, "SHOW CHARACTER SET LIMIT 1", MYLITE_PARSE_ERROR,
                                     "syntax_error", "show character set limit syntax");

    failures += execute_sql(database, "CREATE DATABASE mylite_show_character_set", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_show_character_set", MYLITE_DONE);
    failures +=
        expect_prepare_error(database, "SELECT * FROM missing_show_character_set_table",
                             MYLITE_EXEC_ERROR, "doesn't exist", "show character set error source");
    failures += expect_select_rows(database, "SHOW COUNT(*) ERRORS", error_count_column, 1,
                                   one_count, 1, "show character set source error count");
    failures += expect_select_rows(database, "SHOW CHARACTER SET LIKE 'utf8%'", columns, 4,
                                   utf8_values, 2, "show character set clears diagnostics");
    failures += expect_select_rows(database, "SHOW COUNT(*) ERRORS", error_count_column, 1,
                                   zero_count, 1, "show character set cleared error count");
    failures += expect_select_rows(database, "SHOW ERRORS", diagnostics_columns, 3, NULL, 0,
                                   "show character set cleared diagnostics");

    mylite_close(database);
    // NOLINTEND(readability-function-size,readability-magic-numbers)
    return failures;
}

static int expect_show_character_set_maxlen_int64(mylite_db *database, const char *sql,
                                                  int64_t expected, const char *context)
{
    static const char *const columns[] = {"Charset", "Description", "Default collation", "Maxlen"};
    mylite_stmt *stmt = NULL;
    int failures = prepare_sql(database, sql, MYLITE_OK, &stmt);

    failures += expect_column_names(stmt, columns, 4, context);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, context);
    failures += expect_int64(mylite_column_int64(stmt, 3), expected, context);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, context);
    failures += expect_int64(mylite_affected_rows(stmt), -1, context);
    mylite_finalize(stmt);
    return failures;
}

static int test_show_collation_execution(void)
{
    // NOLINTBEGIN(readability-function-size,readability-magic-numbers)
    static const char *const columns[] = {"Collation", "Charset", "Id",           "Default",
                                          "Compiled",  "Sortlen", "Pad_attribute"};
    static const char *const diagnostics_columns[] = {"Level", "Code", "Message"};
    static const char *const error_count_column[] = {"@@session.error_count"};
    static const char *const zero_count[] = {"0"};
    static const char *const one_count[] = {"1"};
    static const char *const all_values[] = {
        "binary",
        "binary",
        "63",
        "Yes",
        "Yes",
        "1",
        "NO PAD",
        "latin1_bin",
        "latin1",
        "47",
        "",
        "Yes",
        "1",
        "PAD SPACE",
        "latin1_swedish_ci",
        "latin1",
        "8",
        "Yes",
        "Yes",
        "1",
        "PAD SPACE",
        "utf8mb3_bin",
        "utf8mb3",
        "83",
        "",
        "Yes",
        "1",
        "PAD SPACE",
        "utf8mb3_general_ci",
        "utf8mb3",
        "33",
        "Yes",
        "Yes",
        "1",
        "PAD SPACE",
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
        "255",
        "Yes",
        "Yes",
        "0",
        "NO PAD",
        "utf8mb4_bin",
        "utf8mb4",
        "46",
        "",
        "Yes",
        "1",
        "PAD SPACE",
    };
    static const char *const utf8mb4_values[] = {
        "utf8mb4_0900_ai_ci", "utf8mb4", "255", "Yes", "Yes", "0", "NO PAD",
        "utf8mb4_bin",        "utf8mb4", "46",  "",    "Yes", "1", "PAD SPACE",
    };
    static const char *const binary_values[] = {"binary", "binary", "63",    "Yes",
                                                "Yes",    "1",      "NO PAD"};
    static const char *const latin1_values[] = {
        "latin1_bin",        "latin1", "47", "",    "Yes", "1", "PAD SPACE",
        "latin1_swedish_ci", "latin1", "8",  "Yes", "Yes", "1", "PAD SPACE",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open show collation");

    failures += expect_select_rows(database, "SHOW COLLATION", columns, 7, all_values, 7,
                                   "show collation supported catalog");
    failures += expect_select_rows(database, "SHOW COLLATION LIKE 'utf8mb4%'", columns, 7,
                                   utf8mb4_values, 2, "show collation utf8mb4 wildcard");
    failures += expect_select_rows(database, "SHOW COLLATION LIKE 'UTF8MB4%'", columns, 7,
                                   utf8mb4_values, 2, "show collation like is case-insensitive");
    failures += expect_select_rows(database, "SHOW COLLATION LIKE 'utf8mb4\\_%'", columns, 7,
                                   utf8mb4_values, 2, "show collation escaped underscore wildcard");
    failures += expect_select_rows(
        database, "SHOW COLLATION LIKE 'UTF8MB4\\_BIN'", columns, 7,
        (const char *const[]){"utf8mb4_bin", "utf8mb4", "46", "", "Yes", "1", "PAD SPACE"}, 1,
        "show collation escaped exact is case-insensitive");
    failures += expect_select_rows(database, "SHOW COLLATION LIKE 'binary'", columns, 7,
                                   binary_values, 1, "show collation binary row");
    failures += expect_select_rows(database, "SHOW COLLATION LIKE 'BINARY'", columns, 7,
                                   binary_values, 1, "show collation binary case-insensitive");
    failures += expect_select_rows(database, "SHOW COLLATION LIKE 'latin1\\_%'", columns, 7,
                                   latin1_values, 2, "show collation latin1 escaped wildcard");
    failures += expect_select_rows(database, "SHOW COLLATION LIKE 'no_such_collation'", columns, 7,
                                   NULL, 0, "show collation empty like result");
    failures += expect_show_collation_numeric_columns(
        database, "SHOW COLLATION LIKE 'utf8mb4\\_0900\\_ai\\_ci'", 255, 0,
        "show collation id and sortlen are integers");

    failures += expect_prepare_error(database, "SHOW COLLATION WHERE Charset = 'latin1'",
                                     MYLITE_UNSUPPORTED, "SHOW COLLATION WHERE is not supported",
                                     "show collation where charset unsupported");
    failures += expect_prepare_error(database,
                                     "SHOW COLLATION WHERE `Default` = 'Yes' AND Charset IN "
                                     "('binary','latin1','utf8mb3','utf8mb4')",
                                     MYLITE_UNSUPPORTED, "SHOW COLLATION WHERE is not supported",
                                     "show collation where default unsupported");
    failures += expect_prepare_error(
        database, "SHOW COLLATION WHERE Pad_attribute = 'NO PAD' AND Charset = 'utf8mb4'",
        MYLITE_UNSUPPORTED, "SHOW COLLATION WHERE is not supported",
        "show collation where pad attribute unsupported");
    failures += expect_prepare_error(
        database, "SHOW COLLATION WHERE Sortlen > 1 AND Charset = 'latin1'", MYLITE_UNSUPPORTED,
        "SHOW COLLATION WHERE is not supported", "show collation where sortlen unsupported");
    failures += expect_prepare_error(database, "SHOW COLLATION LIKE 1", MYLITE_PARSE_ERROR,
                                     "syntax_error", "show collation non-string like syntax");
    failures += expect_prepare_error(database, "SHOW COLLATION LIMIT 1", MYLITE_PARSE_ERROR,
                                     "syntax_error", "show collation limit syntax");

    failures += execute_sql(database, "CREATE DATABASE mylite_show_collation", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_show_collation", MYLITE_DONE);
    failures +=
        expect_prepare_error(database, "SELECT * FROM missing_show_collation_table",
                             MYLITE_EXEC_ERROR, "doesn't exist", "show collation error source");
    failures += expect_select_rows(database, "SHOW COUNT(*) ERRORS", error_count_column, 1,
                                   one_count, 1, "show collation source error count");
    failures += expect_select_rows(database, "SHOW COLLATION LIKE 'binary'", columns, 7,
                                   binary_values, 1, "show collation clears diagnostics");
    failures += expect_select_rows(database, "SHOW COUNT(*) ERRORS", error_count_column, 1,
                                   zero_count, 1, "show collation cleared error count");
    failures += expect_select_rows(database, "SHOW ERRORS", diagnostics_columns, 3, NULL, 0,
                                   "show collation cleared diagnostics");

    mylite_close(database);
    // NOLINTEND(readability-function-size,readability-magic-numbers)
    return failures;
}

static int expect_show_collation_numeric_columns(mylite_db *database, const char *sql,
                                                 int64_t expected_id, int64_t expected_sortlen,
                                                 const char *context)
{
    static const char *const columns[] = {"Collation", "Charset", "Id",           "Default",
                                          "Compiled",  "Sortlen", "Pad_attribute"};
    mylite_stmt *stmt = NULL;
    int failures = prepare_sql(database, sql, MYLITE_OK, &stmt);

    failures += expect_column_names(stmt, columns, show_collation_column_count, context);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, context);
    failures +=
        expect_int64(mylite_column_int64(stmt, show_collation_id_column), expected_id, context);
    failures += expect_int64(mylite_column_int64(stmt, show_collation_sortlen_column),
                             expected_sortlen, context);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, context);
    failures += expect_int64(mylite_affected_rows(stmt), -1, context);
    mylite_finalize(stmt);
    return failures;
}

static int test_show_columns_execution(void)
{
    // NOLINTBEGIN(readability-function-size,readability-magic-numbers)
    static const char *const standard_columns[] = {"Field", "Type",    "Null",
                                                   "Key",   "Default", "Extra"};
    static const char *const full_columns[] = {"Field",   "Type",  "Collation",  "Null",   "Key",
                                               "Default", "Extra", "Privileges", "Comment"};
    static const char *const b_columns[] = {"Field", "Type", "Null", "Key", "Default", "Extra"};
    static const char *const meta_values[] = {
        "id",     "int",           "NO",  "PRI", NULL,   "auto_increment",
        "name",   "varchar(20)",   "NO",  "MUL", "",     "",
        "amount", "decimal(10,2)", "YES", "UNI", "0.00", "",
        "flag",   "tinyint",       "YES", "",    NULL,   "",
        "hidden", "int",           "YES", "",    NULL,   "INVISIBLE",
        "a_1",    "int",           "YES", "",    NULL,   "",
    };
    static const char *const full_values[] = {
        "id",
        "int",
        NULL,
        "NO",
        "PRI",
        NULL,
        "auto_increment",
        "select,insert,update,references",
        "",
        "name",
        "varchar(20)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "MUL",
        "",
        "",
        "select,insert,update,references",
        "Name comment",
        "amount",
        "decimal(10,2)",
        NULL,
        "YES",
        "UNI",
        "0.00",
        "",
        "select,insert,update,references",
        "",
        "flag",
        "tinyint",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "hidden",
        "int",
        NULL,
        "YES",
        "",
        NULL,
        "INVISIBLE",
        "select,insert,update,references",
        "",
        "a_1",
        "int",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const name_values[] = {"name", "varchar(20)", "NO", "MUL", "", ""};
    static const char *const escaped_values[] = {"a_1", "int", "YES", "", NULL, ""};
    static const char *const b_values[] = {"code", "int", "YES", "", NULL, ""};
    static const char *const override_values[] = {"override_col", "int", "YES", "", NULL, ""};
    static const char *const keyword_fields_values[] = {"fields", "int", "YES", "", NULL, ""};
    static const char *const keyword_columns_values[] = {"columns", "int", "YES", "", NULL, ""};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_status(mylite_open_memory(&database), MYLITE_OK, "open show columns database");
    failures +=
        expect_prepare_error(database, "SHOW COLUMNS FROM meta", MYLITE_EXEC_ERROR,
                             "No database selected", "show columns requires selected schema");

    failures += execute_sql(database, "CREATE DATABASE mylite_show_columns_a", MYLITE_DONE);
    failures += execute_sql(database, "CREATE DATABASE mylite_show_columns_b", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_show_columns_a", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE meta ("
                            "id INT NOT NULL AUTO_INCREMENT, "
                            "name VARCHAR(20) NOT NULL DEFAULT '' COMMENT 'Name comment', "
                            "amount DECIMAL(10,2) DEFAULT 0.00, "
                            "flag TINYINT NULL, "
                            "hidden INT INVISIBLE, "
                            "a_1 INT, "
                            "PRIMARY KEY (id), KEY name_idx (name), "
                            "UNIQUE KEY amount_unique (amount))",
                            MYLITE_DONE);

    failures += expect_select_rows(database, "SHOW COLUMNS FROM meta", standard_columns, 6,
                                   meta_values, 6, "show columns selected schema");
    failures += expect_select_rows(database, "SHOW FIELDS FROM meta", standard_columns, 6,
                                   meta_values, 6, "show fields synonym");
    failures += expect_select_rows(database, "SHOW FULL COLUMNS FROM meta", full_columns, 9,
                                   full_values, 6, "show full columns selected schema");
    failures += expect_select_rows(database, "SHOW EXTENDED COLUMNS FROM meta", standard_columns, 6,
                                   meta_values, 6, "show extended columns current no-op");
    failures += expect_select_rows(database, "SHOW FULL FIELDS IN meta", full_columns, 9,
                                   full_values, 6, "show full fields selected schema");
    failures += expect_select_rows(database, "SHOW COLUMNS FROM meta LIKE 'name'", standard_columns,
                                   6, name_values, 1, "show columns like name");
    failures += expect_select_rows(database, "SHOW COLUMNS FROM meta LIKE 'Name'", standard_columns,
                                   6, name_values, 1, "show columns like is case-insensitive");
    failures +=
        expect_select_rows(database, "SHOW COLUMNS FROM meta LIKE 'a\\_%'", standard_columns, 6,
                           escaped_values, 1, "show columns escaped underscore");
    failures += expect_select_rows(database, "SHOW COLUMNS FROM meta LIKE 'missing%'",
                                   standard_columns, 6, NULL, 0, "show columns empty like result");
    failures += expect_prepare_error(database, "SHOW COLUMNS FROM meta WHERE Field = 'name'",
                                     MYLITE_UNSUPPORTED, "SHOW COLUMNS WHERE is not supported",
                                     "show columns where is parsed but unsupported");

    failures += execute_sql(database, "CREATE TABLE mylite_show_columns_b.in_table (code INT)",
                            MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE mylite_show_columns_b.meta (override_col INT)",
                            MYLITE_DONE);
    failures +=
        expect_select_rows(database, "SHOW COLUMNS FROM in_table FROM mylite_show_columns_b",
                           b_columns, 6, b_values, 1, "show columns from schema");
    failures += expect_select_rows(database,
                                   "SHOW COLUMNS FROM in_table FROM mylite_show_columns_b "
                                   "LIKE 'co%'",
                                   b_columns, 6, b_values, 1, "show columns explicit schema like");
    failures += expect_select_rows(database, "SHOW COLUMNS IN in_table IN mylite_show_columns_b",
                                   b_columns, 6, b_values, 1, "show columns in schema");
    failures += expect_select_rows(database, "SHOW FIELDS FROM mylite_show_columns_b.in_table",
                                   b_columns, 6, b_values, 1, "show fields qualified table");
    failures += expect_select_rows(
        database, "SHOW COLUMNS FROM mylite_show_columns_a.meta FROM mylite_show_columns_b",
        b_columns, 6, override_values, 1, "show columns explicit schema overrides qualifier");
    failures += expect_prepare_error(database, "SHOW COLUMNS FROM in_table FROM missing_columns_db",
                                     MYLITE_EXEC_ERROR, "Unknown database",
                                     "show columns rejects missing schema");
    failures +=
        expect_prepare_error(database, "SHOW COLUMNS FROM missing_columns_table", MYLITE_EXEC_ERROR,
                             "doesn't exist", "show columns rejects missing table");
    failures += expect_prepare_error(database, "SHOW COLUMNS FROM COLUMNS FROM information_schema",
                                     MYLITE_UNSUPPORTED,
                                     "SHOW COLUMNS for information_schema tables is not supported",
                                     "show columns information schema unsupported");
    failures += expect_prepare_error(
        database, "SHOW COLUMNS FROM missing_info FROM information_schema", MYLITE_EXEC_ERROR,
        "Unknown table 'MISSING_INFO' in information_schema",
        "show columns unknown information schema table");

    failures += execute_sql(database,
                            "CREATE TABLE columns (fields INT, columns INT, "
                            "extended INT, full INT)",
                            MYLITE_DONE);
    failures +=
        expect_select_rows(database, "SHOW COLUMNS FROM columns LIKE 'FIELDS'", standard_columns, 6,
                           keyword_fields_values, 1, "show columns keyword field name");
    failures +=
        expect_select_rows(database, "SHOW FIELDS FROM columns LIKE 'columns'", standard_columns, 6,
                           keyword_columns_values, 1, "show fields keyword column name");

    mylite_close(database);
    // NOLINTEND(readability-function-size,readability-magic-numbers)
    return failures;
}

static int test_describe_table_execution(void)
{
    // NOLINTBEGIN(readability-function-size,readability-magic-numbers)
    static const char *const standard_columns[] = {"Field", "Type",    "Null",
                                                   "Key",   "Default", "Extra"};
    static const char *const meta_values[] = {
        "id",      "int",         "NO",  "PRI", NULL, "auto_increment",
        "name",    "varchar(20)", "NO",  "MUL", "",   "",
        "NameTwo", "int",         "YES", "",    NULL, "",
        "a_1",     "int",         "YES", "",    NULL, "",
        "ax1",     "int",         "YES", "",    NULL, "",
        "ab",      "int",         "YES", "",    NULL, "",
        "aX",      "int",         "YES", "",    NULL, "",
    };
    static const char *const name_values[] = {"name", "varchar(20)", "NO", "MUL", "", ""};
    static const char *const escaped_values[] = {"a_1", "int", "YES", "", NULL, ""};
    static const char *const identifier_underscore_values[] = {
        "a_1", "int", "YES", "", NULL, "", "ax1", "int", "YES", "", NULL, "",
    };
    static const char *const wildcard_underscore_values[] = {
        "ab", "int", "YES", "", NULL, "", "aX", "int", "YES", "", NULL, "",
    };
    static const char *const b_values[] = {"code", "int", "YES", "", NULL, ""};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_status(mylite_open_memory(&database), MYLITE_OK, "open describe table database");
    failures += expect_prepare_error(database, "DESCRIBE meta", MYLITE_EXEC_ERROR,
                                     "No database selected", "describe requires selected schema");

    failures += execute_sql(database, "CREATE DATABASE mylite_describe_a", MYLITE_DONE);
    failures += execute_sql(database, "CREATE DATABASE mylite_describe_b", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_describe_a", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE meta ("
                            "id INT NOT NULL AUTO_INCREMENT, "
                            "name VARCHAR(20) NOT NULL DEFAULT '', "
                            "NameTwo INT, "
                            "a_1 INT, "
                            "ax1 INT, "
                            "ab INT, "
                            "aX INT, "
                            "PRIMARY KEY (id), KEY name_idx (name))",
                            MYLITE_DONE);

    failures += expect_select_rows(database, "DESCRIBE meta", standard_columns, 6, meta_values, 7,
                                   "describe selected schema");
    failures += expect_select_rows(database, "DESC meta", standard_columns, 6, meta_values, 7,
                                   "desc synonym");
    failures += expect_select_rows(database, "EXPLAIN meta", standard_columns, 6, meta_values, 7,
                                   "explain table synonym");
    failures += expect_select_rows(database, "DESCRIBE meta name", standard_columns, 6, name_values,
                                   1, "describe identifier filter");
    failures += expect_select_rows(database, "DESCRIBE meta Name", standard_columns, 6, name_values,
                                   1, "describe identifier filter case");
    failures += expect_select_rows(database, "DESCRIBE meta `name`", standard_columns, 6,
                                   name_values, 1, "describe quoted identifier filter");
    failures += expect_select_rows(database, "DESCRIBE meta `a_1`", standard_columns, 6,
                                   identifier_underscore_values, 2,
                                   "describe quoted identifier underscore wildcard");
    failures += expect_select_rows(database, "DESCRIBE meta 'Name'", standard_columns, 6,
                                   name_values, 1, "describe literal filter case");
    failures += expect_select_rows(database, "DESCRIBE meta 'a\\_%'", standard_columns, 6,
                                   escaped_values, 1, "describe escaped underscore");
    failures += expect_select_rows(database, "DESCRIBE meta a_1", standard_columns, 6,
                                   identifier_underscore_values, 2,
                                   "describe identifier underscore wildcard");
    failures +=
        expect_select_rows(database, "DESCRIBE meta 'a_'", standard_columns, 6,
                           wildcard_underscore_values, 2, "describe literal underscore wildcard");
    failures += expect_select_rows(database, "EXPLAIN meta name", standard_columns, 6, name_values,
                                   1, "explain table filtered synonym");
    failures += expect_select_rows(database, "DESCRIBE meta 'missing%'", standard_columns, 6, NULL,
                                   0, "describe empty wildcard result");

    failures +=
        execute_sql(database, "CREATE TABLE mylite_describe_b.meta (code INT)", MYLITE_DONE);
    failures += expect_select_rows(database, "DESCRIBE mylite_describe_b.meta", standard_columns, 6,
                                   b_values, 1, "describe schema qualified target");
    failures +=
        expect_prepare_error(database, "DESCRIBE missing_describe_db.meta", MYLITE_EXEC_ERROR,
                             "Unknown database", "describe rejects missing schema");
    failures += expect_prepare_error(database, "DESCRIBE missing_describe_table", MYLITE_EXEC_ERROR,
                                     "doesn't exist", "describe rejects missing table");
    failures +=
        expect_prepare_error(database, "DESCRIBE information_schema.TABLES", MYLITE_UNSUPPORTED,
                             "DESCRIBE for information_schema tables is not supported",
                             "describe information schema unsupported");
    failures += expect_prepare_error(database, "DESCRIBE information_schema.missing_info",
                                     MYLITE_EXEC_ERROR,
                                     "Unknown table 'MISSING_INFO' in information_schema",
                                     "describe unknown information schema table");

    mylite_close(database);
    // NOLINTEND(readability-function-size,readability-magic-numbers)
    return failures;
}

static int test_show_create_table_execution(void)
{
    // NOLINTBEGIN(readability-function-size,readability-magic-numbers)
    static const char *const columns[] = {"Table", "Create Table"};
    static const char simple_create[] = "CREATE TABLE `simple_table` (\n"
                                        "  `id` int NOT NULL,\n"
                                        "  `v` varchar(10) DEFAULT NULL,\n"
                                        "  PRIMARY KEY (`id`)\n"
                                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 "
                                        "COLLATE=utf8mb4_0900_ai_ci";
    static const char *const simple_values[] = {"simple_table", simple_create};
    static const char qualified_create[] = "CREATE TABLE `qualified` (\n"
                                           "  `code` int DEFAULT NULL\n"
                                           ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 "
                                           "COLLATE=utf8mb4_0900_ai_ci";
    static const char *const qualified_values[] = {"qualified", qualified_create};
    static const char meta_create[] =
        "CREATE TABLE `meta` (\n"
        "  `id` int NOT NULL AUTO_INCREMENT,\n"
        "  `name` varchar(20) COLLATE utf8mb4_bin NOT NULL DEFAULT '' "
        "COMMENT 'Name comment',\n"
        "  `amount` decimal(10,2) DEFAULT '0.00',\n"
        "  `flag` tinyint DEFAULT NULL,\n"
        "  `hidden` int DEFAULT NULL /*!80023 INVISIBLE */,\n"
        "  `a_1` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`),\n"
        "  UNIQUE KEY `amount_unique` (`amount`),\n"
        "  KEY `name_idx` (`name`),\n"
        "  KEY `idx_name_tail` (`name`(5) DESC,`a_1`) COMMENT 'idx comment' "
        "/*!80000 INVISIBLE */\n"
        ") ENGINE=InnoDB AUTO_INCREMENT=42 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin "
        "COMMENT='table comment'";
    static const char *const meta_values[] = {"meta", meta_create};
    static const char collation_default_create[] =
        "CREATE TABLE `collation_default` (\n"
        "  `c` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin DEFAULT NULL,\n"
        "  `d` varchar(10) DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci";
    static const char *const collation_default_values[] = {"collation_default",
                                                           collation_default_create};
    static const char collation_table_bin_create[] =
        "CREATE TABLE `collation_table_bin` (\n"
        "  `c` varchar(10) COLLATE utf8mb4_bin DEFAULT NULL,\n"
        "  `d` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
    static const char *const collation_table_bin_values[] = {"collation_table_bin",
                                                             collation_table_bin_create};
    static const char escaped_create[] = "CREATE TABLE `weird``name` (\n"
                                         "  `select` int DEFAULT NULL,\n"
                                         "  `b``c` varchar(4) DEFAULT NULL,\n"
                                         "  KEY `idx``s` (`b``c`)\n"
                                         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 "
                                         "COLLATE=utf8mb4_0900_ai_ci";
    static const char *const escaped_values[] = {"weird`name", escaped_create};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_status(mylite_open_memory(&database), MYLITE_OK, "open show create table database");
    failures +=
        expect_prepare_error(database, "SHOW CREATE TABLE meta", MYLITE_EXEC_ERROR,
                             "No database selected", "show create table requires selected schema");

    failures += execute_sql(database, "CREATE DATABASE mylite_show_create_a", MYLITE_DONE);
    failures += execute_sql(database, "CREATE DATABASE mylite_show_create_b", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_show_create_a", MYLITE_DONE);
    failures += execute_sql(
        database, "CREATE TABLE simple_table (id INT PRIMARY KEY, v VARCHAR(10))", MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW CREATE TABLE simple_table", columns, 2,
                                   simple_values, 1, "show create simple table");

    failures += execute_sql(database,
                            "CREATE TABLE meta ("
                            "id INT NOT NULL AUTO_INCREMENT, "
                            "name VARCHAR(20) NOT NULL DEFAULT '' COMMENT 'Name comment', "
                            "amount DECIMAL(10,2) DEFAULT 0.00, "
                            "flag TINYINT NULL, "
                            "hidden INT INVISIBLE, "
                            "a_1 INT, "
                            "PRIMARY KEY (id), "
                            "KEY name_idx (name), "
                            "UNIQUE KEY amount_unique (amount), "
                            "KEY idx_name_tail (name(5) DESC, a_1 ASC) "
                            "COMMENT 'idx comment' INVISIBLE"
                            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin "
                            "COMMENT='table comment' AUTO_INCREMENT=42",
                            MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW CREATE TABLE meta", columns, 2, meta_values, 1,
                                   "show create table full metadata");

    failures += execute_sql(database,
                            "CREATE TABLE collation_default ("
                            "c VARCHAR(10) COLLATE utf8mb4_bin, d VARCHAR(10)"
                            ") DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
                            MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW CREATE TABLE collation_default", columns, 2,
                                   collation_default_values, 1,
                                   "show create table explicit column collation");

    failures += execute_sql(database,
                            "CREATE TABLE collation_table_bin ("
                            "c VARCHAR(10), d VARCHAR(10) COLLATE utf8mb4_0900_ai_ci"
                            ") DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin",
                            MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW CREATE TABLE collation_table_bin", columns, 2,
                                   collation_table_bin_values, 1,
                                   "show create table table default collation");

    failures += execute_sql(database, "CREATE TABLE mylite_show_create_b.qualified (code INT)",
                            MYLITE_DONE);
    failures +=
        expect_select_rows(database, "SHOW CREATE TABLE mylite_show_create_b.qualified", columns, 2,
                           qualified_values, 1, "show create table schema qualified");

    failures += execute_sql(database,
                            "CREATE TABLE `weird``name` (`select` INT, `b``c` VARCHAR(4), "
                            "KEY `idx``s` (`b``c`))",
                            MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW CREATE TABLE `weird``name`", columns, 2,
                                   escaped_values, 1, "show create table escaped identifiers");

    failures += expect_prepare_error(database, "SHOW CREATE TABLE missing_show_create_db.t",
                                     MYLITE_EXEC_ERROR, "Unknown database",
                                     "show create table rejects missing schema");
    failures += expect_prepare_error(database, "SHOW CREATE TABLE missing_show_create_table",
                                     MYLITE_EXEC_ERROR, "doesn't exist",
                                     "show create table rejects missing table");
    failures += expect_prepare_error(database, "SHOW CREATE TABLE information_schema.TABLES",
                                     MYLITE_UNSUPPORTED,
                                     "SHOW CREATE TABLE for information_schema tables is not "
                                     "supported",
                                     "show create table information schema unsupported");
    failures += expect_prepare_error(database, "SHOW CREATE TABLE information_schema.missing_info",
                                     MYLITE_EXEC_ERROR,
                                     "Unknown table 'MISSING_INFO' in information_schema",
                                     "show create table unknown information schema table");

    mylite_close(database);
    // NOLINTEND(readability-function-size,readability-magic-numbers)
    return failures;
}

static int test_show_diagnostics_execution(void)
{
    // NOLINTBEGIN(readability-function-size,readability-magic-numbers)
    static const char *const diagnostics_columns[] = {"Level", "Code", "Message"};
    static const char *const warning_count_column[] = {"@@session.warning_count"};
    static const char *const error_count_column[] = {"@@session.error_count"};
    static const char *const zero_count[] = {"0"};
    static const char *const one_count[] = {"1"};
    static const char *const division_warning[] = {"Warning", "1365", "Division by 0"};
    static const char *const delayed_warning[] = {
        "Warning",
        "3005",
        "REPLACE DELAYED is no longer supported. The statement was converted to REPLACE.",
    };
    static const char *const syntax_error[] = {"Error", "1064", "syntax_error"};
    static const char *const missing_table_error[] = {
        "Error",
        "1146",
        "Table 'mylite_show_diagnostics.missing_show_diagnostics_table' doesn't exist",
    };
    static const char *const unknown_column_error[] = {
        "Error",
        "1054",
        "Unknown column 'missing_column' in 'field list'",
    };
    static const char *const savepoint_error[] = {
        "Error",
        "1305",
        "SAVEPOINT missing_sp does not exist",
    };
    static const char *const missing_drop_note[] = {
        "Note",
        "1051",
        "Unknown table 'mylite_show_diagnostics.missing_show_diagnostics_table'",
    };
    static const char *const existing_table_note[] = {
        "Note",
        "1050",
        "Table 'r' already exists",
    };
    static const char *const existing_schema_note[] = {
        "Note",
        "1007",
        "Can't create database 'mylite_show_diagnostics'; database exists",
    };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures +=
        expect_status(mylite_open_memory(&database), MYLITE_OK, "open show diagnostics database");

    failures += expect_select_rows(database, "SHOW WARNINGS", diagnostics_columns, 3, NULL, 0,
                                   "empty show warnings");
    failures += expect_select_rows(database, "SHOW ERRORS", diagnostics_columns, 3, NULL, 0,
                                   "empty show errors");
    failures += expect_select_rows(database, "SHOW COUNT(*) WARNINGS", warning_count_column, 1,
                                   zero_count, 1, "empty warning count");
    failures += expect_select_rows(database, "SHOW COUNT(*) ERRORS", error_count_column, 1,
                                   zero_count, 1, "empty error count");

    failures +=
        expect_select_rows(database, "SELECT 1/0 AS divzero", (const char *const[]){"divzero"}, 1,
                           (const char *const[]){NULL}, 1, "division warning source");
    failures += expect_select_rows(database, "SHOW WARNINGS", diagnostics_columns, 3,
                                   division_warning, 1, "show warnings after division");
    failures += expect_select_rows(database, "SHOW COUNT(*) WARNINGS", warning_count_column, 1,
                                   one_count, 1, "warning count after division");
    failures += expect_select_rows(database, "SHOW ERRORS", diagnostics_columns, 3, NULL, 0,
                                   "show errors excludes warning");
    failures += expect_select_rows(database, "SHOW COUNT(*) ERRORS", error_count_column, 1,
                                   zero_count, 1, "error count excludes warning");
    failures += expect_select_rows(database, "SHOW WARNINGS LIMIT 0", diagnostics_columns, 3, NULL,
                                   0, "show warnings limit zero");
    failures += expect_select_rows(database, "SHOW WARNINGS LIMIT 1, 0", diagnostics_columns, 3,
                                   NULL, 0, "show warnings comma zero");
    failures += expect_select_rows(database, "SHOW WARNINGS LIMIT 1, 1", diagnostics_columns, 3,
                                   NULL, 0, "show warnings beyond end");
    failures += expect_select_rows(database, "SHOW WARNINGS LIMIT 0 OFFSET 1", diagnostics_columns,
                                   3, NULL, 0, "show warnings offset beyond end");
    failures +=
        expect_select_rows(database, "SHOW WARNINGS LIMIT 18446744073709551615",
                           diagnostics_columns, 3, division_warning, 1, "show warnings max limit");

    failures += execute_sql(database, "SELECT 1", MYLITE_ROW);
    failures += expect_select_rows(database, "SHOW COUNT(*) WARNINGS", warning_count_column, 1,
                                   zero_count, 1, "warning count after clear");

    failures += execute_sql(database, "CREATE DATABASE mylite_show_diagnostics", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_show_diagnostics", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE r (id INT PRIMARY KEY)", MYLITE_DONE);
    failures +=
        execute_sql(database, "DROP TABLE IF EXISTS missing_show_diagnostics_table", MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW WARNINGS", diagnostics_columns, 3,
                                   missing_drop_note, 1, "show warnings after drop missing note");
    failures += expect_select_rows(database, "SHOW ERRORS", diagnostics_columns, 3, NULL, 0,
                                   "show errors excludes drop missing note");
    failures += expect_select_rows(database, "SHOW COUNT(*) WARNINGS", warning_count_column, 1,
                                   one_count, 1, "warning count includes note");
    failures += expect_select_rows(database, "SHOW COUNT(*) ERRORS", error_count_column, 1,
                                   zero_count, 1, "error count excludes note");
    failures += execute_sql(database, "CREATE TABLE IF NOT EXISTS r (id INT)", MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW WARNINGS", diagnostics_columns, 3,
                                   existing_table_note, 1, "show warnings after existing table");
    failures +=
        execute_sql(database, "CREATE DATABASE IF NOT EXISTS mylite_show_diagnostics", MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW WARNINGS", diagnostics_columns, 3,
                                   existing_schema_note, 1, "show warnings after existing schema");
    failures += execute_sql(database, "REPLACE DELAYED INTO r VALUES (1)", MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW WARNINGS", diagnostics_columns, 3,
                                   delayed_warning, 1, "show warnings after replace delayed");

    failures += expect_prepare_error(database, "SHOW WARNINGS LIKE 'x'", MYLITE_PARSE_ERROR,
                                     "syntax_error", "show warnings like syntax");
    failures += expect_select_rows(database, "SHOW ERRORS", diagnostics_columns, 3, syntax_error, 1,
                                   "show errors after syntax error");
    failures += expect_select_rows(database, "SHOW WARNINGS", diagnostics_columns, 3, syntax_error,
                                   1, "show warnings includes syntax error");
    failures += expect_select_rows(database, "SHOW ERRORS LIMIT 1", diagnostics_columns, 3,
                                   syntax_error, 1, "show errors limit row count");
    failures += expect_select_rows(database, "SHOW ERRORS LIMIT 0, 1", diagnostics_columns, 3,
                                   syntax_error, 1, "show errors comma limit");
    failures += expect_select_rows(database, "SHOW ERRORS LIMIT 1 OFFSET 0", diagnostics_columns, 3,
                                   syntax_error, 1, "show errors offset limit");
    failures += expect_select_rows(database, "SHOW COUNT(*) WARNINGS", warning_count_column, 1,
                                   one_count, 1, "warning count includes syntax error");
    failures += expect_select_rows(database, "SHOW COUNT(*) ERRORS", error_count_column, 1,
                                   one_count, 1, "error count includes syntax error");

    failures += execute_sql(database, "SELECT 1", MYLITE_ROW);
    failures += expect_select_rows(database, "SHOW COUNT(*) ERRORS", error_count_column, 1,
                                   zero_count, 1, "error count after clear");

    failures +=
        expect_prepare_error(database, "SELECT * FROM missing_show_diagnostics_table",
                             MYLITE_EXEC_ERROR, "doesn't exist", "missing table diagnostic source");
    failures += expect_select_rows(database, "SHOW ERRORS", diagnostics_columns, 3,
                                   missing_table_error, 1, "show errors after prepare failure");
    failures +=
        expect_select_rows(database, "SHOW WARNINGS", diagnostics_columns, 3, missing_table_error,
                           1, "show warnings includes missing table error");

    failures +=
        expect_prepare_error(database, "SELECT missing_column FROM r", MYLITE_EXEC_ERROR,
                             "Unknown column 'missing_column'", "unknown column diagnostic source");
    failures += expect_select_rows(database, "SHOW ERRORS", diagnostics_columns, 3,
                                   unknown_column_error, 1, "show errors after unknown column");

    failures += execute_sql(database, "SAVEPOINT outside_sp", MYLITE_DONE);
    failures += prepare_sql(database, "ROLLBACK TO SAVEPOINT missing_sp", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "SAVEPOINT missing_sp does not exist",
                                  "missing savepoint diagnostic source");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SHOW ERRORS", diagnostics_columns, 3, savepoint_error,
                                   1, "show errors after missing savepoint");

    mylite_close(database);
    // NOLINTEND(readability-function-size,readability-magic-numbers)
    return failures;
}

static int test_show_index_execution(void)
{
    // NOLINTBEGIN(readability-function-size,readability-magic-numbers)
    static const char *const show_index_columns[] = {
        "Table",      "Non_unique",  "Key_name",      "Seq_in_index", "Column_name",
        "Collation",  "Cardinality", "Sub_part",      "Packed",       "Null",
        "Index_type", "Comment",     "Index_comment", "Visible",      "Expression",
    };
    static const char *const meta_values[] = {
        "meta",     "0",     "PRIMARY",
        "1",        "id",    "A",
        NULL,       NULL,    NULL,
        "",         "BTREE", "",
        "",         "YES",   NULL,
        "meta",     "0",     "uq_code",
        "1",        "code",  "A",
        NULL,       NULL,    NULL,
        "",         "BTREE", "",
        "",         "YES",   NULL,
        "meta",     "1",     "idx_name_tail",
        "1",        "name",  "D",
        NULL,       "5",     NULL,
        "YES",      "BTREE", "",
        "idx note", "NO",    NULL,
        "meta",     "1",     "idx_name_tail",
        "2",        "tail",  "A",
        NULL,       NULL,    NULL,
        "YES",      "BTREE", "",
        "idx note", "NO",    NULL,
    };
    static const char *const b_meta_values[] = {
        "meta", "1", "other_idx", "1", "other_id", "A",   NULL, NULL,
        NULL,   "",  "BTREE",     "",  "",         "YES", NULL,
    };
    static const char *const b_in_table_values[] = {
        "in_table", "1", "in_idx", "1", "code", "A",   NULL, NULL,
        NULL,       "",  "BTREE",  "",  "",     "YES", NULL,
    };
    static const char *const keyword_indexes_values[] = {
        "indexes", "1",   "indexes", "1", "id", "A",   NULL, NULL,
        NULL,      "YES", "BTREE",   "",  "",   "YES", NULL,
    };
    static const char *const quoted_keys_values[] = {
        "keys", "1", "keys", "1", "id", "A", NULL, NULL, NULL, "YES", "BTREE", "", "", "YES", NULL,
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open show index database");
    failures += expect_prepare_error(database, "SHOW INDEX FROM meta", MYLITE_EXEC_ERROR,
                                     "No database selected", "show index requires selected schema");

    failures += execute_sql(database, "CREATE DATABASE mylite_show_index_a", MYLITE_DONE);
    failures += execute_sql(database, "CREATE DATABASE mylite_show_index_b", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_show_index_a", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE meta ("
                            "id INT NOT NULL, "
                            "name VARCHAR(20) NULL, "
                            "code VARCHAR(20) NOT NULL, "
                            "tail INT NULL, "
                            "PRIMARY KEY (id), "
                            "UNIQUE KEY uq_code (code), "
                            "KEY idx_name_tail (name(5) DESC, tail ASC) "
                            "COMMENT 'idx note' INVISIBLE)",
                            MYLITE_DONE);

    failures += expect_select_rows(database, "SHOW INDEX FROM meta", show_index_columns, 15,
                                   meta_values, 4, "show index selected schema");
    failures += expect_select_rows(database, "SHOW INDEXES FROM meta", show_index_columns, 15,
                                   meta_values, 4, "show indexes synonym");
    failures += expect_select_rows(database, "SHOW KEYS IN meta", show_index_columns, 15,
                                   meta_values, 4, "show keys synonym");
    failures += expect_select_rows(database, "SHOW EXTENDED INDEX FROM meta", show_index_columns,
                                   15, meta_values, 4, "show extended index current no-op");
    failures += expect_prepare_error(database, "SHOW INDEX FROM meta WHERE Key_name = 'PRIMARY'",
                                     MYLITE_UNSUPPORTED, "SHOW INDEX WHERE is not supported",
                                     "show index where is parsed but unsupported");

    failures += execute_sql(database, "CREATE TABLE no_idx (id INT)", MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW INDEX FROM no_idx", show_index_columns, 15, NULL,
                                   0, "show index no index rows");

    failures += execute_sql(database,
                            "CREATE TABLE mylite_show_index_b.meta ("
                            "other_id INT NOT NULL, KEY other_idx (other_id))",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE mylite_show_index_b.in_table ("
                            "code INT NOT NULL, KEY in_idx (code))",
                            MYLITE_DONE);
    failures +=
        expect_select_rows(database, "SHOW INDEX FROM in_table FROM mylite_show_index_b",
                           show_index_columns, 15, b_in_table_values, 1, "show index from schema");
    failures +=
        expect_select_rows(database, "SHOW INDEX IN in_table IN mylite_show_index_b",
                           show_index_columns, 15, b_in_table_values, 1, "show index in schema");
    failures += expect_select_rows(database, "SHOW INDEX FROM mylite_show_index_b.in_table",
                                   show_index_columns, 15, b_in_table_values, 1,
                                   "show index qualified table");
    failures += expect_select_rows(database,
                                   "SHOW INDEX FROM mylite_show_index_a.meta "
                                   "FROM mylite_show_index_b",
                                   show_index_columns, 15, b_meta_values, 1,
                                   "show index explicit schema overrides qualifier");
    failures += expect_prepare_error(database, "SHOW INDEX FROM in_table FROM missing_index_db",
                                     MYLITE_EXEC_ERROR, "Unknown database",
                                     "show index rejects missing schema");
    failures +=
        expect_prepare_error(database, "SHOW INDEX FROM missing_index_table", MYLITE_EXEC_ERROR,
                             "doesn't exist", "show index rejects missing table");
    failures += expect_select_rows(database, "SHOW INDEX FROM TABLES FROM information_schema",
                                   show_index_columns, 15, NULL, 0,
                                   "show index information schema known table");
    failures += expect_prepare_error(
        database, "SHOW INDEX FROM missing_info FROM information_schema", MYLITE_EXEC_ERROR,
        "Unknown table 'MISSING_INFO' in information_schema",
        "show index unknown information schema table");

    failures +=
        execute_sql(database, "CREATE TABLE indexes (id INT, KEY indexes (id))", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE `keys` (id INT, KEY `keys` (id))", MYLITE_DONE);
    failures += expect_select_rows(database, "SHOW INDEX FROM indexes", show_index_columns, 15,
                                   keyword_indexes_values, 1, "show index keyword table");
    failures += expect_select_rows(database, "SHOW KEYS FROM `keys`", show_index_columns, 15,
                                   quoted_keys_values, 1, "show keys quoted table");

    mylite_close(database);
    // NOLINTEND(readability-function-size,readability-magic-numbers)
    return failures;
}

static int test_insert_values_execution(void)
{
    enum {
        insert_forms_row_count = 5,
        ai_first_insert_id = 10,
        ai_default_value_insert_id = 12,
        ai_empty_column_insert_id = 13,
        ai_default_row_insert_id = 14,
        ai_inserted_row_count = 5,
        ai_default_column_n = 7,
        ai_failed_first_insert_id = 10,
        ai_failed_after_rollback_id = 12,
        ai_failed_pending_next_id = 23,
        ai_reserved_generated_id = 21,
        ai_reserved_after_statement_id = 23,
        explicit_auto_after_duplicate_id = 7,
        explicit_auto_after_explicit_id = 9,
        explicit_auto_generated_after_explicit_v = 60,
        defaults_explicit_default_nd = 9,
    };
    const char *path = MYLITE_RUNTIME_TEST_FILE_PATH;
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    char *forms_physical = NULL;
    char *unique_physical = NULL;
    char *ai_physical = NULL;
    char *failed_ai_physical = NULL;
    char *failed_pending_ai_physical = NULL;
    char *reserve_ai_physical = NULL;
    char *explicit_physical = NULL;
    char *defaults_physical = NULL;
    char *atomic_physical = NULL;
    char *expr_physical = NULL;
    int failures = 0;

    remove_runtime_test_files();
    failures += expect_status(mylite_open(path, &database), MYLITE_OK, "open insert file");

    failures += prepare_sql(database, "INSERT INTO no_default_table VALUES (1)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert no database");
    failures += expect_contains(mylite_error_message(database), "No database selected",
                                "insert no database error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "CREATE DATABASE mylite_iv13", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "create insert schema");
    failures += expect_int64(mylite_affected_rows(stmt), 0, "create schema affected rows");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "USE mylite_iv13", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "use insert schema");
    failures += expect_int64(mylite_affected_rows(stmt), 0, "use schema affected rows");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "INSERT INTO missing_schema.t VALUES (1)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert missing schema");
    failures += expect_contains(mylite_error_message(database), "Unknown database",
                                "insert missing schema error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "INSERT INTO mylite_iv13.missing VALUES (1)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert missing table");
    failures +=
        expect_contains(mylite_error_message(database), "Table 'mylite_iv13.missing' doesn't exist",
                        "insert missing table error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "INSERT INTO information_schema.tables VALUES ()", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert system schema");
    failures +=
        expect_contains(mylite_error_message(database), "system schema", "insert system error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        execute_sql(database, "CREATE TABLE insert_forms (a INT, b VARCHAR(10))", MYLITE_DONE);
    forms_physical = expected_physical_table_name("mylite_iv13", "insert_forms");
    if (forms_physical == NULL) {
        fprintf(stderr, "out of memory while building insert_forms physical table name\n");
        failures = 1;
    }
    failures += execute_sql(database, "INSERT insert_forms VALUE (1, 'one')", MYLITE_DONE);
    failures += execute_sql(
        database, "INSERT INTO insert_forms VALUES ROW(2, 'two'), ROW(3, 'three')", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO insert_forms VALUES ()", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO insert_forms () VALUES ()", MYLITE_DONE);
    failures += prepare_sql(database, "INSERT INTO insert_forms () VALUES (4)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR,
                              "explicit empty insert list wrong count");
    failures += expect_contains(mylite_error_message(database),
                                "Column count doesn't match value count at row 1",
                                "explicit empty insert list wrong count error");
    mylite_finalize(stmt);
    stmt = NULL;
    if (forms_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, forms_physical, "COUNT(*)", "",
                                                 insert_forms_row_count, "insert form row count");
        failures += expect_sqlite_physical_text(path, forms_physical, "b", "WHERE a = 3", "three",
                                                "ROW constructor insert value");
        failures +=
            expect_sqlite_physical_null(path, forms_physical, "a", "WHERE a IS NULL AND b IS NULL",
                                        "default row without column list");
    }

    failures +=
        execute_sql(database, "CREATE TABLE unique_insert (a INT UNIQUE, b INT)", MYLITE_DONE);
    unique_physical = expected_physical_table_name("mylite_iv13", "unique_insert");
    if (unique_physical == NULL) {
        fprintf(stderr, "out of memory while building unique_insert physical table name\n");
        failures = 1;
    }
    failures +=
        execute_sql(database, "INSERT INTO unique_insert VALUES (NULL,1),(NULL,2)", MYLITE_DONE);
    failures +=
        prepare_sql(database, "INSERT INTO unique_insert VALUES (5,3),(5,4)", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "intra statement unique duplicate");
    failures += expect_contains(mylite_error_message(database), "Duplicate entry '5'",
                                "intra statement unique duplicate error");
    mylite_finalize(stmt);
    stmt = NULL;
    if (unique_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, unique_physical, "COUNT(*)", "", 2,
                                                 "unique duplicate rollback count");
    }
    failures += execute_sql(database, "INSERT INTO unique_insert VALUES (5,3)", MYLITE_DONE);
    failures += prepare_sql(database, "INSERT INTO unique_insert VALUES (5,4)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "existing unique duplicate");
    failures += expect_contains(mylite_error_message(database), "Duplicate entry '5'",
                                "existing unique duplicate error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database,
                            "CREATE TABLE ai ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
                            "v VARCHAR(10) DEFAULT 'd', "
                            "n INT NOT NULL DEFAULT 7, "
                            "nullable INT, "
                            "ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP) "
                            "AUTO_INCREMENT=10",
                            MYLITE_DONE);
    ai_physical = expected_physical_table_name("mylite_iv13", "ai");
    if (ai_physical == NULL) {
        fprintf(stderr, "out of memory while building ai physical table name\n");
        failures = 1;
    }

    failures += prepare_sql(database, "INSERT INTO ai (v) VALUES ('a'), ('b')", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "insert ai implicit values");
    failures += expect_int64(mylite_affected_rows(stmt), 2, "insert ai affected rows");
    failures += expect_int64((int64_t)mylite_last_insert_id(database), ai_first_insert_id,
                             "insert ai last insert id");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database,
                            "INSERT INTO ai VALUES "
                            "(DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT)",
                            MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), ai_default_value_insert_id,
                             "insert ai default value last insert id");
    failures += execute_sql(database, "INSERT INTO ai () VALUES ()", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), ai_empty_column_insert_id,
                             "insert ai empty column last insert id");
    failures += execute_sql(database, "INSERT INTO ai VALUES ()", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), ai_default_row_insert_id,
                             "insert ai default row last insert id");

    if (ai_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, ai_physical, "COUNT(*)", "",
                                                 ai_inserted_row_count, "ai inserted row count");
        failures += expect_sqlite_physical_int64(path, ai_physical, "id", "WHERE v = 'a'",
                                                 ai_first_insert_id, "ai first implicit id");
        failures += expect_sqlite_physical_int64(path, ai_physical, "n", "WHERE id = 10",
                                                 ai_default_column_n, "ai first default n");
        failures += expect_sqlite_physical_null(path, ai_physical, "nullable", "WHERE id = 10",
                                                "ai nullable default");
        failures += expect_sqlite_physical_not_null(path, ai_physical, "ts", "WHERE id = 10",
                                                    "ai timestamp default");
        failures += expect_sqlite_physical_text(path, ai_physical, "v", "WHERE id = 12", "d",
                                                "ai DEFAULT text value");
        failures +=
            expect_sqlite_physical_int64(path, ai_physical, "id", "WHERE id = 14",
                                         ai_default_row_insert_id, "ai empty row generated id");
    }

    failures += execute_sql(database,
                            "CREATE TABLE ai_failed_sequence ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, u INT UNIQUE) "
                            "AUTO_INCREMENT=10",
                            MYLITE_DONE);
    failed_ai_physical = expected_physical_table_name("mylite_iv13", "ai_failed_sequence");
    if (failed_ai_physical == NULL) {
        fprintf(stderr, "out of memory while building ai_failed_sequence physical table name\n");
        failures = 1;
    }
    failures += prepare_sql(database, "INSERT INTO ai_failed_sequence VALUES (NULL,1),(NULL,1)",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR,
                              "failed auto-increment duplicate insert");
    failures += expect_contains(mylite_error_message(database), "Duplicate entry '1'",
                                "failed auto-increment duplicate error");
    failures += expect_int64((int64_t)mylite_last_insert_id(database), ai_failed_first_insert_id,
                             "failed auto-increment first insert id");
    mylite_finalize(stmt);
    stmt = NULL;
    if (failed_ai_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, failed_ai_physical, "COUNT(*)", "", 0,
                                                 "failed auto-increment rollback count");
    }
    failures +=
        execute_sql(database, "INSERT INTO ai_failed_sequence VALUES (NULL,2)", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), ai_failed_after_rollback_id,
                             "failed auto-increment next insert id");
    if (failed_ai_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, failed_ai_physical, "id", "WHERE u = 2",
                                                 ai_failed_after_rollback_id,
                                                 "failed auto-increment consumed ids");
    }

    failures += execute_sql(database,
                            "CREATE TABLE ai_failed_pending ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, u INT UNIQUE) "
                            "AUTO_INCREMENT=10",
                            MYLITE_DONE);
    failed_pending_ai_physical = expected_physical_table_name("mylite_iv13", "ai_failed_pending");
    if (failed_pending_ai_physical == NULL) {
        fprintf(stderr, "out of memory while building ai_failed_pending physical table name\n");
        failures = 1;
    }
    failures += prepare_sql(database, "INSERT INTO ai_failed_pending VALUES (20,1),(NULL,1)",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR,
                              "failed pending auto-increment duplicate insert");
    failures += expect_contains(mylite_error_message(database), "Duplicate entry '1'",
                                "failed pending auto-increment duplicate error");
    failures += expect_int64((int64_t)mylite_last_insert_id(database), ai_failed_after_rollback_id,
                             "failed pending auto-increment leaves last insert id");
    mylite_finalize(stmt);
    stmt = NULL;
    if (failed_pending_ai_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, failed_pending_ai_physical, "COUNT(*)", "",
                                                 0, "failed pending auto-increment rollback count");
    }
    failures += execute_sql(database, "INSERT INTO ai_failed_pending VALUES (NULL,2)", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), ai_failed_pending_next_id,
                             "failed pending auto-increment next insert id");
    if (failed_pending_ai_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, failed_pending_ai_physical, "id",
                                                 "WHERE u = 2", ai_failed_pending_next_id,
                                                 "failed pending auto-increment consumed ids");
    }

    failures += execute_sql(database,
                            "CREATE TABLE ai_reserve ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, u INT UNIQUE) "
                            "AUTO_INCREMENT=10",
                            MYLITE_DONE);
    reserve_ai_physical = expected_physical_table_name("mylite_iv13", "ai_reserve");
    if (reserve_ai_physical == NULL) {
        fprintf(stderr, "out of memory while building ai_reserve physical table name\n");
        failures = 1;
    }
    failures += execute_sql(database, "INSERT INTO ai_reserve VALUES (20,1),(NULL,2)", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), ai_reserved_generated_id,
                             "reserved auto-increment generated id");
    failures += execute_sql(database, "INSERT INTO ai_reserve VALUES (NULL,3)", MYLITE_DONE);
    if (reserve_ai_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, reserve_ai_physical, "id", "WHERE u = 2",
                                                 ai_reserved_generated_id,
                                                 "reserved auto-increment generated row");
        failures += expect_sqlite_physical_int64(path, reserve_ai_physical, "id", "WHERE u = 3",
                                                 ai_reserved_after_statement_id,
                                                 "reserved auto-increment next row");
    }

    failures += execute_sql(database,
                            "CREATE TABLE ai_explicit ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, v INT) "
                            "AUTO_INCREMENT=3",
                            MYLITE_DONE);
    explicit_physical = expected_physical_table_name("mylite_iv13", "ai_explicit");
    if (explicit_physical == NULL) {
        fprintf(stderr, "out of memory while building ai_explicit physical table name\n");
        failures = 1;
    }
    failures += prepare_sql(database,
                            "INSERT INTO ai_explicit VALUES "
                            "(NULL,10),(0,20),(5,50),(NULL,60)",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "insert explicit auto mix");
    failures += expect_int64(mylite_affected_rows(stmt), 4, "explicit auto affected rows");
    failures +=
        expect_int64((int64_t)mylite_last_insert_id(database), 3, "explicit auto last insert id");
    mylite_finalize(stmt);
    stmt = NULL;
    if (explicit_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, explicit_physical, "COUNT(*)", "", 4,
                                                 "explicit auto row count");
        failures += expect_sqlite_physical_int64(path, explicit_physical, "v", "WHERE id = 6",
                                                 explicit_auto_generated_after_explicit_v,
                                                 "explicit auto generated after explicit");
    }

    failures += prepare_sql(database, "INSERT INTO ai_explicit VALUES (4, 40)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "duplicate primary insert");
    failures += expect_contains(mylite_error_message(database), "Duplicate entry '4'",
                                "duplicate primary error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "INSERT INTO ai_explicit VALUES (NULL,70)", MYLITE_DONE);
    if (explicit_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, explicit_physical, "id", "WHERE v = 70",
                                                 explicit_auto_after_duplicate_id,
                                                 "explicit auto next after duplicate");
    }
    failures += execute_sql(database, "INSERT INTO ai_explicit VALUES (8,80)", MYLITE_DONE);
    failures +=
        expect_int64((int64_t)mylite_last_insert_id(database), explicit_auto_after_duplicate_id,
                     "explicit auto value leaves last insert id");
    failures += execute_sql(database, "INSERT INTO ai_explicit VALUES (NULL,90)", MYLITE_DONE);
    failures +=
        expect_int64((int64_t)mylite_last_insert_id(database), explicit_auto_after_explicit_id,
                     "explicit auto value advances sequence");
    if (explicit_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, explicit_physical, "id", "WHERE v = 90",
                                                 explicit_auto_after_explicit_id,
                                                 "explicit auto next after explicit");
    }

    failures += execute_sql(database,
                            "CREATE TABLE defaults ("
                            "nn INT NOT NULL, nd INT NOT NULL DEFAULT 9, "
                            "nul INT, txt VARCHAR(10) DEFAULT 'hello')",
                            MYLITE_DONE);
    defaults_physical = expected_physical_table_name("mylite_iv13", "defaults");
    if (defaults_physical == NULL) {
        fprintf(stderr, "out of memory while building defaults physical table name\n");
        failures = 1;
    }
    failures += execute_sql(database,
                            "INSERT INTO defaults (nn, nd, nul, txt) "
                            "VALUES (1, DEFAULT, DEFAULT, DEFAULT)",
                            MYLITE_DONE);
    if (defaults_physical != NULL) {
        failures +=
            expect_sqlite_physical_int64(path, defaults_physical, "nd", "WHERE nn = 1",
                                         defaults_explicit_default_nd, "explicit DEFAULT integer");
        failures += expect_sqlite_physical_null(path, defaults_physical, "nul", "WHERE nn = 1",
                                                "explicit DEFAULT nullable");
        failures += expect_sqlite_physical_text(path, defaults_physical, "txt", "WHERE nn = 1",
                                                "hello", "explicit DEFAULT string");
    }
    failures += execute_sql(database, "INSERT INTO defaults (NN, TXT, ND) VALUES (2, 'case', + 3)",
                            MYLITE_DONE);
    failures +=
        execute_sql(database, "INSERT INTO defaults (nn, nd) VALUES (- 2, + 4)", MYLITE_DONE);
    if (defaults_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, defaults_physical, "nd", "WHERE nn = 2", 3,
                                                 "case-insensitive column list");
        failures += expect_sqlite_physical_text(path, defaults_physical, "txt", "WHERE nn = 2",
                                                "case", "case-insensitive column text");
        failures += expect_sqlite_physical_int64(path, defaults_physical, "nd", "WHERE nn = -2", 4,
                                                 "spaced unary insert value");
    }

    failures +=
        prepare_sql(database, "INSERT INTO defaults (nd) VALUES (DEFAULT)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "missing not null default");
    failures += expect_contains(mylite_error_message(database), "doesn't have a default value",
                                "missing not null default error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "INSERT INTO defaults (nn) VALUES (NULL)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "explicit null not null");
    failures +=
        expect_contains(mylite_error_message(database), "cannot be null", "explicit null error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "INSERT INTO defaults (nn, NN) VALUES (1, 2)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "duplicate insert column");
    failures += expect_contains(mylite_error_message(database), "specified twice",
                                "duplicate insert column error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "INSERT INTO defaults (missing_col) VALUES (1)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "unknown insert column");
    failures += expect_contains(mylite_error_message(database), "Unknown column",
                                "unknown insert column error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "INSERT INTO defaults VALUES (1, 2)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "wrong insert value count");
    failures += expect_contains(mylite_error_message(database),
                                "Column count doesn't match value count at row 1",
                                "wrong insert count error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        execute_sql(database, "CREATE TABLE atomic_insert (a INT NOT NULL, b INT)", MYLITE_DONE);
    atomic_physical = expected_physical_table_name("mylite_iv13", "atomic_insert");
    if (atomic_physical == NULL) {
        fprintf(stderr, "out of memory while building atomic physical table name\n");
        failures = 1;
    }
    failures +=
        prepare_sql(database, "INSERT INTO atomic_insert VALUES (1,1),(NULL,2)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "atomic insert null error");
    mylite_finalize(stmt);
    stmt = NULL;
    if (atomic_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, atomic_physical, "COUNT(*)", "", 0,
                                                 "atomic insert rollback row count");
    }

    failures += execute_sql(database,
                            "CREATE TABLE expr_defaults ("
                            "a INT DEFAULT (1 + 2), "
                            "b TIMESTAMP DEFAULT (CURRENT_TIMESTAMP))",
                            MYLITE_DONE);
    expr_physical = expected_physical_table_name("mylite_iv13", "expr_defaults");
    if (expr_physical == NULL) {
        fprintf(stderr, "out of memory while building expr_defaults physical table name\n");
        failures = 1;
    }
    failures +=
        execute_sql(database, "INSERT INTO expr_defaults (a, b) VALUES (1, DEFAULT)", MYLITE_DONE);
    failures += prepare_sql(database, "INSERT INTO expr_defaults VALUES (DEFAULT, DEFAULT)",
                            MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "unsupported generated default insert");
    failures += expect_contains(mylite_error_message(database), "Unsupported generated default",
                                "unsupported generated default error");
    mylite_finalize(stmt);
    stmt = NULL;
    if (expr_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, expr_physical, "COUNT(*)", "", 1,
                                                 "generated default rollback row count");
        failures += expect_sqlite_physical_not_null(path, expr_physical, "b", "WHERE a = 1",
                                                    "parenthesized current timestamp default");
    }

    free(forms_physical);
    free(unique_physical);
    free(ai_physical);
    free(failed_ai_physical);
    free(failed_pending_ai_physical);
    free(reserve_ai_physical);
    free(explicit_physical);
    free(defaults_physical);
    free(atomic_physical);
    free(expr_physical);
    mylite_close(database);
    remove_runtime_test_files();
    return failures;
}

static int test_insert_set_execution(void)
{
    enum {
        set_forms_row_count = 2,
        qualified_set_value = 8,
        defaults_first_id = 10,
        defaults_second_id = 11,
        defaults_nn = 7,
        assignment_order_late_value = 5,
        auto_ref_null_id = 3,
        auto_ref_zero_id = 4,
        auto_ref_default_id = 5,
        auto_ref_explicit_id = 20,
        auto_ref_forward_id = 30,
        auto_ref_after_explicit_id = 31,
        duplicate_first_id = 20,
        duplicate_after_failure_id = 22,
    };
    const char *path = MYLITE_RUNTIME_TEST_FILE_PATH;
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    char *forms_physical = NULL;
    char *qualified_physical = NULL;
    char *defaults_physical = NULL;
    char *ao_default_physical = NULL;
    char *ao_nullable_physical = NULL;
    char *ao_required_physical = NULL;
    char *auto_ref_physical = NULL;
    char *diag_physical = NULL;
    char *expr_fail_physical = NULL;
    char *duplicate_physical = NULL;
    int failures = 0;

    remove_runtime_test_files();
    failures += expect_status(mylite_open(path, &database), MYLITE_OK, "open insert set file");

    failures += prepare_sql(database, "INSERT INTO no_default_table SET a = 1", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert set no database");
    failures += expect_contains(mylite_error_message(database), "No database selected",
                                "insert set no database error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "CREATE DATABASE mylite_is14", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_is14", MYLITE_DONE);

    failures += prepare_sql(database, "INSERT INTO missing_schema.t SET a = 1", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert set missing schema");
    failures += expect_contains(mylite_error_message(database), "Unknown database",
                                "insert set missing schema error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "INSERT INTO mylite_is14.missing SET a = 1", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert set missing table");
    failures +=
        expect_contains(mylite_error_message(database), "Table 'mylite_is14.missing' doesn't exist",
                        "insert set missing table error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "INSERT INTO information_schema.tables SET a = 1", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert set system schema");
    failures += expect_contains(mylite_error_message(database), "system schema",
                                "insert set system schema error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(
        database, "CREATE TABLE set_forms (v VARCHAR(20), nn INT NOT NULL DEFAULT 0)", MYLITE_DONE);
    forms_physical = expected_physical_table_name("mylite_is14", "set_forms");
    if (forms_physical == NULL) {
        fprintf(stderr, "out of memory while building set_forms physical table name\n");
        failures = 1;
    }
    failures +=
        execute_sql(database, "INSERT set_forms SET v = 'without_into', nn = 1", MYLITE_DONE);
    failures += prepare_sql(database, "INSERT INTO set_forms SET v = 'with_into', nn = 2",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "insert set with INTO");
    failures += expect_int64(mylite_affected_rows(stmt), 1, "insert set affected rows");
    mylite_finalize(stmt);
    stmt = NULL;
    if (forms_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, forms_physical, "COUNT(*)", "",
                                                 set_forms_row_count, "insert set form row count");
        failures += expect_sqlite_physical_text(path, forms_physical, "v", "WHERE nn = 1",
                                                "without_into", "insert set optional into");
        failures += expect_sqlite_physical_text(path, forms_physical, "v", "WHERE nn = 2",
                                                "with_into", "insert set required into");
    }

    failures += execute_sql(database, "CREATE TABLE qualified_set (v INT)", MYLITE_DONE);
    qualified_physical = expected_physical_table_name("mylite_is14", "qualified_set");
    if (qualified_physical == NULL) {
        fprintf(stderr, "out of memory while building qualified_set physical table name\n");
        failures = 1;
    }
    failures +=
        execute_sql(database, "INSERT INTO mylite_is14.qualified_set SET v = 8", MYLITE_DONE);
    if (qualified_physical != NULL) {
        failures +=
            expect_sqlite_physical_int64(path, qualified_physical, "v", "", qualified_set_value,
                                         "insert set schema-qualified target");
    }

    failures += execute_sql(database,
                            "CREATE TABLE defaults_set ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
                            "must INT NOT NULL, "
                            "v VARCHAR(10) DEFAULT 'd', "
                            "nn INT NOT NULL DEFAULT 7, "
                            "nul INT, "
                            "ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP) "
                            "AUTO_INCREMENT=10",
                            MYLITE_DONE);
    defaults_physical = expected_physical_table_name("mylite_is14", "defaults_set");
    if (defaults_physical == NULL) {
        fprintf(stderr, "out of memory while building defaults_set physical table name\n");
        failures = 1;
    }
    failures +=
        prepare_sql(database,
                    "INSERT INTO defaults_set SET must = 1, v = DEFAULT, nn = DEFAULT, nul = NULL, "
                    "ts = CURRENT_TIMESTAMP",
                    MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "insert set defaults explicit");
    failures += expect_int64(mylite_affected_rows(stmt), 1, "insert set defaults affected rows");
    failures += expect_int64((int64_t)mylite_last_insert_id(database), defaults_first_id,
                             "insert set defaults last insert id");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "INSERT INTO defaults_set SET must = 2", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), defaults_second_id,
                             "insert set omitted defaults last insert id");
    if (defaults_physical != NULL) {
        failures += expect_sqlite_physical_text(path, defaults_physical, "v", "WHERE must = 1", "d",
                                                "insert set explicit DEFAULT text");
        failures += expect_sqlite_physical_int64(path, defaults_physical, "nn", "WHERE must = 1",
                                                 defaults_nn, "insert set explicit DEFAULT int");
        failures += expect_sqlite_physical_null(path, defaults_physical, "nul", "WHERE must = 1",
                                                "insert set explicit NULL");
        failures += expect_sqlite_physical_not_null(path, defaults_physical, "ts", "WHERE must = 1",
                                                    "insert set current timestamp");
        failures += expect_sqlite_physical_text(path, defaults_physical, "v", "WHERE must = 2", "d",
                                                "insert set omitted text default");
        failures += expect_sqlite_physical_null(path, defaults_physical, "nul", "WHERE must = 2",
                                                "insert set omitted nullable");
    }

    failures += prepare_sql(database, "INSERT INTO defaults_set SET v = 'missing_required'",
                            MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert set missing required default");
    failures += expect_contains(mylite_error_message(database), "doesn't have a default value",
                                "insert set missing required default error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "INSERT INTO defaults_set SET must = NULL", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert set null not null");
    failures += expect_contains(mylite_error_message(database), "cannot be null",
                                "insert set null not null error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "INSERT INTO defaults_set SET must = DEFAULT", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert set default missing");
    failures += expect_contains(mylite_error_message(database), "doesn't have a default value",
                                "insert set default missing error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "CREATE TABLE ao_default (a INT DEFAULT 3, b INT DEFAULT 4)",
                            MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE ao_nullable (a INT, b INT)", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE ao_required (a INT NOT NULL, b INT DEFAULT 4)",
                            MYLITE_DONE);
    ao_default_physical = expected_physical_table_name("mylite_is14", "ao_default");
    ao_nullable_physical = expected_physical_table_name("mylite_is14", "ao_nullable");
    ao_required_physical = expected_physical_table_name("mylite_is14", "ao_required");
    if (ao_default_physical == NULL || ao_nullable_physical == NULL ||
        ao_required_physical == NULL) {
        fprintf(stderr, "out of memory while building assignment-order physical table names\n");
        failures = 1;
    }
    failures += execute_sql(database, "INSERT INTO ao_default SET b = a + 1, a = 5", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO ao_nullable SET b = a + 1, a = 5", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO ao_required SET b = a + 1, a = 5", MYLITE_DONE);
    if (ao_default_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, ao_default_physical, "a", "",
                                                 assignment_order_late_value,
                                                 "insert set assignment order default a");
        failures += expect_sqlite_physical_int64(path, ao_default_physical, "b", "", 4,
                                                 "insert set assignment order default b");
    }
    if (ao_nullable_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, ao_nullable_physical, "a", "",
                                                 assignment_order_late_value,
                                                 "insert set assignment order nullable a");
        failures += expect_sqlite_physical_null(path, ao_nullable_physical, "b", "",
                                                "insert set assignment order nullable b");
    }
    if (ao_required_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, ao_required_physical, "a", "",
                                                 assignment_order_late_value,
                                                 "insert set assignment order required a");
        failures += expect_sqlite_physical_int64(path, ao_required_physical, "b", "", 1,
                                                 "insert set assignment order required b");
    }

    failures += execute_sql(database,
                            "CREATE TABLE auto_ref ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, a INT) "
                            "AUTO_INCREMENT=3",
                            MYLITE_DONE);
    auto_ref_physical = expected_physical_table_name("mylite_is14", "auto_ref");
    if (auto_ref_physical == NULL) {
        fprintf(stderr, "out of memory while building auto_ref physical table name\n");
        failures = 1;
    }
    failures += execute_sql(database, "INSERT INTO auto_ref SET id = NULL, a = id", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), auto_ref_null_id,
                             "insert set auto null last insert id");
    failures += execute_sql(database, "INSERT INTO auto_ref SET id = 0, a = id", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), auto_ref_zero_id,
                             "insert set auto zero last insert id");
    failures += execute_sql(database, "INSERT INTO auto_ref SET id = DEFAULT, a = id", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), auto_ref_default_id,
                             "insert set auto default last insert id");
    failures += execute_sql(database, "INSERT INTO auto_ref SET id = 20, a = id", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), auto_ref_default_id,
                             "insert set explicit auto leaves last insert id");
    failures += execute_sql(database, "INSERT INTO auto_ref SET a = id, id = 30", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), auto_ref_default_id,
                             "insert set forward auto leaves last insert id");
    failures += execute_sql(database, "INSERT INTO auto_ref SET a = 99", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), auto_ref_after_explicit_id,
                             "insert set explicit high advances sequence");
    if (auto_ref_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, auto_ref_physical, "a", "WHERE id = 3", 0,
                                                 "insert set auto null reference");
        failures += expect_sqlite_physical_int64(path, auto_ref_physical, "a", "WHERE id = 4", 0,
                                                 "insert set auto zero reference");
        failures += expect_sqlite_physical_int64(path, auto_ref_physical, "a", "WHERE id = 5", 0,
                                                 "insert set auto default reference");
        failures += expect_sqlite_physical_int64(path, auto_ref_physical, "a", "WHERE id = 20",
                                                 auto_ref_explicit_id,
                                                 "insert set explicit auto reference");
        failures += expect_sqlite_physical_int64(path, auto_ref_physical, "a", "WHERE id = 30", 0,
                                                 "insert set forward auto reference");
        failures += expect_sqlite_physical_int64(path, auto_ref_physical, "id", "WHERE a = 99",
                                                 auto_ref_after_explicit_id,
                                                 "insert set generated after explicit high");
    }

    failures += execute_sql(database, "CREATE TABLE diag_set (a INT, b INT)", MYLITE_DONE);
    diag_physical = expected_physical_table_name("mylite_is14", "diag_set");
    if (diag_physical == NULL) {
        fprintf(stderr, "out of memory while building diag_set physical table name\n");
        failures = 1;
    }
    failures +=
        execute_sql(database, "INSERT INTO diag_set SET diag_set.a = 1, b = 2", MYLITE_DONE);
    failures +=
        execute_sql(database, "INSERT INTO diag_set SET mylite_is14.diag_set.a = 3", MYLITE_DONE);
    failures += prepare_sql(database, "INSERT INTO diag_set SET a = 1, A = 2", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert set duplicate target");
    failures += expect_contains(mylite_error_message(database), "specified twice",
                                "insert set duplicate target error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "INSERT INTO diag_set SET a = 1, `a` = 2", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert set quoted duplicate target");
    failures += expect_contains(mylite_error_message(database), "specified twice",
                                "insert set quoted duplicate target error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "INSERT INTO diag_set SET missing_col = 1", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert set unknown target");
    failures += expect_contains(mylite_error_message(database), "Unknown column",
                                "insert set unknown target error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "INSERT INTO diag_set SET a = 1, A = 2, missing_col = 3",
                            MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert set unknown beats duplicate");
    failures += expect_contains(mylite_error_message(database), "Unknown column",
                                "insert set unknown beats duplicate error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "INSERT INTO diag_set SET other.a = 1", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert set mismatched qualifier");
    failures += expect_contains(mylite_error_message(database), "Unknown column",
                                "insert set mismatched qualifier error");
    mylite_finalize(stmt);
    stmt = NULL;
    if (diag_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, diag_physical, "COUNT(*)", "", 2,
                                                 "insert set qualified diagnostics row count");
    }

    failures += execute_sql(database, "CREATE TABLE quoted_diag (CamelCase INT)", MYLITE_DONE);
    failures += prepare_sql(database, "INSERT INTO quoted_diag SET CamelCase = 1, `camelcase` = 2",
                            MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert set quoted case duplicate");
    failures += expect_contains(mylite_error_message(database), "specified twice",
                                "insert set quoted case duplicate error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "CREATE TABLE expr_fail (a INT, b INT)", MYLITE_DONE);
    expr_fail_physical = expected_physical_table_name("mylite_is14", "expr_fail");
    if (expr_fail_physical == NULL) {
        fprintf(stderr, "out of memory while building expr_fail physical table name\n");
        failures = 1;
    }
    failures += prepare_sql(database, "INSERT INTO expr_fail SET a = 'text' + 1", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "insert set unsupported expression");
    failures += expect_contains(mylite_error_message(database), "Unsupported INSERT value",
                                "insert set unsupported expression error");
    mylite_finalize(stmt);
    stmt = NULL;
    if (expr_fail_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, expr_fail_physical, "COUNT(*)", "", 0,
                                                 "insert set unsupported expression rollback");
    }

    failures += execute_sql(database,
                            "CREATE TABLE duplicate_set ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, u INT UNIQUE) "
                            "AUTO_INCREMENT=20",
                            MYLITE_DONE);
    duplicate_physical = expected_physical_table_name("mylite_is14", "duplicate_set");
    if (duplicate_physical == NULL) {
        fprintf(stderr, "out of memory while building duplicate_set physical table name\n");
        failures = 1;
    }
    failures += execute_sql(database, "INSERT INTO duplicate_set SET u = 1", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), duplicate_first_id,
                             "insert set duplicate setup last insert id");
    failures += prepare_sql(database, "INSERT INTO duplicate_set SET u = 1", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR,
                              "insert set duplicate consumes sequence");
    failures += expect_contains(mylite_error_message(database), "Duplicate entry '1'",
                                "insert set duplicate consumes sequence error");
    failures += expect_int64((int64_t)mylite_last_insert_id(database), duplicate_first_id,
                             "insert set duplicate leaves last insert id");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "INSERT INTO duplicate_set SET u = 2", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), duplicate_after_failure_id,
                             "insert set duplicate consumed next id");
    if (duplicate_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, duplicate_physical, "COUNT(*)", "", 2,
                                                 "insert set duplicate rollback row count");
        failures += expect_sqlite_physical_int64(path, duplicate_physical, "id", "WHERE u = 2",
                                                 duplicate_after_failure_id,
                                                 "insert set duplicate consumed sequence");
    }

    free(forms_physical);
    free(qualified_physical);
    free(defaults_physical);
    free(ao_default_physical);
    free(ao_nullable_physical);
    free(ao_required_physical);
    free(auto_ref_physical);
    free(diag_physical);
    free(expr_fail_physical);
    free(duplicate_physical);
    mylite_close(database);
    remove_runtime_test_files();
    return failures;
}

static int test_replace_execution(void)
{
    enum {
        replace_forms_row_count = 5,
        replace_default_b = 7,
        replace_set_assigned_value = 5,
        replace_set_default_b = 4,
        replace_required_must = 5,
        replace_ai_seed_id = 10,
        replace_ai_generated_id = 11,
        replace_ai_explicit_id = 20,
        replace_ai_failed_first_id = 11,
        replace_ai_failed_next_id = 13,
    };
    const char *path = MYLITE_RUNTIME_TEST_FILE_PATH;
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    char *forms_physical = NULL;
    char *conflict_physical = NULL;
    char *order_physical = NULL;
    char *set_physical = NULL;
    char *required_physical = NULL;
    char *atomic_physical = NULL;
    char *null_unique_physical = NULL;
    char *ai_physical = NULL;
    char *ai_fail_physical = NULL;
    int failures = 0;

    remove_runtime_test_files();
    failures += expect_status(mylite_open(path, &database), MYLITE_OK, "open replace file");

    failures += prepare_sql(database, "REPLACE INTO no_database VALUES (1)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "replace no database");
    failures += expect_contains(mylite_error_message(database), "No database selected",
                                "replace no database error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "REPLACE DELAYED INTO no_database VALUES (1)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "replace delayed no database");
    failures +=
        expect_int(mylite_warning_count(database), 2, "replace delayed error warning count");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_legacy_syntax_converted,
                   "replace delayed error warning code");
    failures += expect_string(
        mylite_warning_message(database, 0),
        "REPLACE DELAYED is no longer supported. The statement was converted to REPLACE.",
        "replace delayed error warning message");
    failures += expect_int((int)mylite_warning_code(database, 1), mysql_warning_no_database,
                           "replace delayed no database error code");
    failures += expect_string(mylite_warning_message(database, 1), "No database selected",
                              "replace delayed no database error message");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "CREATE DATABASE mylite_r33", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_r33", MYLITE_DONE);

    failures +=
        execute_sql(database, "CREATE TABLE replace_forms (a INT, b INT DEFAULT 7)", MYLITE_DONE);
    forms_physical = expected_physical_table_name("mylite_r33", "replace_forms");
    if (forms_physical == NULL) {
        fprintf(stderr, "out of memory while building replace_forms physical table name\n");
        failures = 1;
    }
    failures += execute_sql_expect_done_affected(
        database, "REPLACE replace_forms VALUE (1, DEFAULT)", 1, "replace singular value affected");
    failures += execute_sql_expect_done_affected(
        database, "REPLACE INTO replace_forms VALUES ROW(2, 2), ROW(3, 3)", 2,
        "replace row constructors affected");
    failures += execute_sql_expect_done_affected(database, "REPLACE INTO replace_forms VALUES ()",
                                                 1, "replace all-default affected");
    failures +=
        execute_sql_expect_done_affected(database, "REPLACE INTO replace_forms () VALUES ()", 1,
                                         "replace empty column list affected");
    if (forms_physical != NULL) {
        failures +=
            expect_sqlite_physical_int64(path, forms_physical, "COUNT(*)", "",
                                         replace_forms_row_count, "replace forms row count");
        failures += expect_sqlite_physical_int64(path, forms_physical, "b", "WHERE a = 1",
                                                 replace_default_b, "replace DEFAULT value");
        failures += expect_sqlite_physical_null(
            path, forms_physical, "a", "WHERE a IS NULL AND b = 7", "replace all-default row");
    }
    failures += execute_sql(database, "START TRANSACTION READ ONLY", MYLITE_DONE);
    failures += prepare_sql(database, "REPLACE INTO replace_forms VALUES (9, 9)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR,
                              "read only transaction rejects replace");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "read only replace affected rows");
    failures += expect_contains(mylite_error_message(database),
                                "Cannot execute statement in a READ ONLY transaction",
                                "read only replace error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "COMMIT", MYLITE_DONE);
    if (forms_physical != NULL) {
        failures +=
            expect_sqlite_physical_int64(path, forms_physical, "COUNT(*)", "WHERE a = 9 AND b = 9",
                                         0, "read only rejected replace absent");
    }

    failures += execute_sql(database,
                            "CREATE TABLE replace_conflict ("
                            "id INT PRIMARY KEY, u INT UNIQUE, n INT UNIQUE, v VARCHAR(20))",
                            MYLITE_DONE);
    conflict_physical = expected_physical_table_name("mylite_r33", "replace_conflict");
    if (conflict_physical == NULL) {
        fprintf(stderr, "out of memory while building replace_conflict physical table name\n");
        failures = 1;
    }
    failures += execute_sql(database,
                            "INSERT INTO replace_conflict VALUES "
                            "(1, 10, 100, 'one'), (2, 20, 200, 'two')",
                            MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database, "REPLACE INTO replace_conflict VALUES (3, 10, 200, 'new')", 3,
        "replace deletes multiple conflicts affected");
    if (conflict_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, conflict_physical, "COUNT(*)", "", 1,
                                                 "replace multiple conflict row count");
        failures += expect_sqlite_physical_text(path, conflict_physical, "v", "WHERE id = 3", "new",
                                                "replace multiple conflict value");
    }

    failures += execute_sql(database,
                            "CREATE TABLE replace_order ("
                            "id INT PRIMARY KEY, u INT UNIQUE, v INT)",
                            MYLITE_DONE);
    order_physical = expected_physical_table_name("mylite_r33", "replace_order");
    if (order_physical == NULL) {
        fprintf(stderr, "out of memory while building replace_order physical table name\n");
        failures = 1;
    }
    failures += execute_sql_expect_done_affected(
        database, "REPLACE INTO replace_order VALUES (1, 10, 1), (2, 10, 2)", 3,
        "replace same statement order affected");
    if (order_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, order_physical, "COUNT(*)", "", 1,
                                                 "replace same statement row count");
        failures += expect_sqlite_physical_int64(path, order_physical, "id", "WHERE u = 10", 2,
                                                 "replace later row survives");
    }

    failures += execute_sql(database,
                            "CREATE TABLE replace_set ("
                            "id INT PRIMARY KEY, a INT DEFAULT 3, b INT DEFAULT 4, c INT, n INT)",
                            MYLITE_DONE);
    set_physical = expected_physical_table_name("mylite_r33", "replace_set");
    if (set_physical == NULL) {
        fprintf(stderr, "out of memory while building replace_set physical table name\n");
        failures = 1;
    }
    failures += execute_sql(database, "INSERT INTO replace_set VALUES (1, 100, 100, 100, 100)",
                            MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database, "REPLACE INTO replace_set SET id = 1, b = a + 1, a = 5, c = b + 1, n = n + 1", 2,
        "replace set assignment order affected");
    if (set_physical != NULL) {
        failures +=
            expect_sqlite_physical_int64(path, set_physical, "a", "WHERE id = 1",
                                         replace_set_assigned_value, "replace set assigned value");
        failures += expect_sqlite_physical_int64(path, set_physical, "b", "WHERE id = 1",
                                                 replace_set_default_b,
                                                 "replace set reads candidate default");
        failures += expect_sqlite_physical_int64(path, set_physical, "c", "WHERE id = 1",
                                                 replace_set_assigned_value,
                                                 "replace set reads earlier assignment");
        failures += expect_sqlite_physical_null(path, set_physical, "n", "WHERE id = 1",
                                                "replace set reads candidate nullable default");
    }

    failures += execute_sql(database,
                            "CREATE TABLE replace_required ("
                            "id INT PRIMARY KEY, u INT UNIQUE, must INT NOT NULL)",
                            MYLITE_DONE);
    required_physical = expected_physical_table_name("mylite_r33", "replace_required");
    if (required_physical == NULL) {
        fprintf(stderr, "out of memory while building replace_required physical table name\n");
        failures = 1;
    }
    failures += execute_sql(database, "INSERT INTO replace_required VALUES (1, 1, 5)", MYLITE_DONE);
    failures += prepare_sql(database, "REPLACE INTO replace_required (id, u) VALUES (1, 1)",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR,
                              "replace required validation before delete");
    failures += expect_contains(mylite_error_message(database), "doesn't have a default value",
                                "replace required validation error");
    mylite_finalize(stmt);
    stmt = NULL;
    if (required_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, required_physical, "COUNT(*)", "", 1,
                                                 "replace required keeps row count");
        failures +=
            expect_sqlite_physical_int64(path, required_physical, "must", "WHERE id = 1",
                                         replace_required_must, "replace required keeps old row");
    }

    failures += execute_sql(database,
                            "CREATE TABLE replace_atomic ("
                            "id INT PRIMARY KEY, u INT UNIQUE, must INT NOT NULL)",
                            MYLITE_DONE);
    atomic_physical = expected_physical_table_name("mylite_r33", "replace_atomic");
    if (atomic_physical == NULL) {
        fprintf(stderr, "out of memory while building replace_atomic physical table name\n");
        failures = 1;
    }
    failures += execute_sql(database, "INSERT INTO replace_atomic VALUES (1, 1, 1), (2, 2, 2)",
                            MYLITE_DONE);
    failures += prepare_sql(database, "REPLACE INTO replace_atomic VALUES (1, 1, 10), (2, 2, NULL)",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "replace atomic failure");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "replace atomic affected rows");
    mylite_finalize(stmt);
    stmt = NULL;
    if (atomic_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, atomic_physical, "must", "WHERE id = 1", 1,
                                                 "replace atomic rolls back first row");
        failures += expect_sqlite_physical_int64(path, atomic_physical, "COUNT(*)", "", 2,
                                                 "replace atomic row count");
    }

    failures +=
        execute_sql(database, "CREATE TABLE replace_null_unique (id INT PRIMARY KEY, u INT UNIQUE)",
                    MYLITE_DONE);
    null_unique_physical = expected_physical_table_name("mylite_r33", "replace_null_unique");
    if (null_unique_physical == NULL) {
        fprintf(stderr, "out of memory while building replace_null_unique physical table name\n");
        failures = 1;
    }
    failures +=
        execute_sql(database, "INSERT INTO replace_null_unique VALUES (1, NULL)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database, "REPLACE INTO replace_null_unique VALUES (2, NULL)", 1,
        "replace nullable unique null affected");
    if (null_unique_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, null_unique_physical, "COUNT(*)", "", 2,
                                                 "replace nullable unique null row count");
    }

    failures += execute_sql(database,
                            "CREATE TABLE replace_ai ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, u INT UNIQUE, v INT) "
                            "AUTO_INCREMENT=10",
                            MYLITE_DONE);
    ai_physical = expected_physical_table_name("mylite_r33", "replace_ai");
    if (ai_physical == NULL) {
        fprintf(stderr, "out of memory while building replace_ai physical table name\n");
        failures = 1;
    }
    failures += execute_sql(database, "INSERT INTO replace_ai (u, v) VALUES (1, 1)", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), replace_ai_seed_id,
                             "replace auto seed last insert id");
    failures +=
        execute_sql_expect_done_affected(database, "REPLACE INTO replace_ai (u, v) VALUES (1, 2)",
                                         2, "replace auto generated affected");
    failures += expect_int64((int64_t)mylite_last_insert_id(database), replace_ai_generated_id,
                             "replace auto generated last insert id");
    failures += execute_sql_expect_done_affected(
        database, "REPLACE INTO replace_ai VALUES (20, 1, 3)", 2, "replace explicit auto affected");
    failures += expect_int64((int64_t)mylite_last_insert_id(database), replace_ai_generated_id,
                             "replace explicit auto leaves last insert id");
    if (ai_physical != NULL) {
        failures +=
            expect_sqlite_physical_int64(path, ai_physical, "id", "WHERE u = 1",
                                         replace_ai_explicit_id, "replace explicit auto row id");
    }

    failures += execute_sql(database,
                            "CREATE TABLE replace_ai_fail ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, u INT UNIQUE, "
                            "v INT NOT NULL) AUTO_INCREMENT=10",
                            MYLITE_DONE);
    ai_fail_physical = expected_physical_table_name("mylite_r33", "replace_ai_fail");
    if (ai_fail_physical == NULL) {
        fprintf(stderr, "out of memory while building replace_ai_fail physical table name\n");
        failures = 1;
    }
    failures +=
        execute_sql(database, "INSERT INTO replace_ai_fail (u, v) VALUES (1, 1)", MYLITE_DONE);
    failures += prepare_sql(
        database, "REPLACE INTO replace_ai_fail (u, v) VALUES (2, 2), (2, NULL)", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "replace failed auto statement");
    failures += expect_int64((int64_t)mylite_last_insert_id(database), replace_ai_failed_first_id,
                             "replace failed auto last insert id");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        execute_sql(database, "INSERT INTO replace_ai_fail (u, v) VALUES (3, 3)", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), replace_ai_failed_next_id,
                             "replace failed auto consumed sequence");
    if (ai_fail_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, ai_fail_physical, "COUNT(*)", "", 2,
                                                 "replace failed auto rollback row count");
        failures += expect_sqlite_physical_int64(path, ai_fail_physical, "id", "WHERE u = 3",
                                                 replace_ai_failed_next_id,
                                                 "replace failed auto next row id");
    }

    failures +=
        prepare_sql(database, "REPLACE DELAYED INTO replace_forms VALUES (8, 8)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "replace delayed warning step");
    failures += expect_int(mylite_warning_count(database), 1, "replace delayed warning count");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_legacy_syntax_converted, "replace delayed warning code");
    failures += expect_string(
        mylite_warning_message(database, 0),
        "REPLACE DELAYED is no longer supported. The statement was converted to REPLACE.",
        "replace delayed warning message");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "REPLACE LOW_PRIORITY INTO replace_forms VALUES (9, 9)",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "replace low priority step");
    failures += expect_int(mylite_warning_count(database), 0, "replace low priority warning count");
    mylite_finalize(stmt);
    stmt = NULL;

    free(forms_physical);
    free(conflict_physical);
    free(order_physical);
    free(set_physical);
    free(required_physical);
    free(atomic_physical);
    free(null_unique_physical);
    free(ai_physical);
    free(ai_fail_physical);
    mylite_close(database);
    remove_runtime_test_files();
    return failures;
}

static int test_insert_ignore_execution(void)
{
    static const char *const pk_null_columns[] = {"id"};
    static const char *const pk_null_values[] = {"0"};
    enum {
        duplicate_seed_id = 10,
        duplicate_first_accepted_id = 12,
        duplicate_second_accepted_id = 14,
        duplicate_after_values_id = 15,
        duplicate_after_set_id = 17,
        same_statement_first_accepted_id = 18,
        same_statement_second_accepted_id = 20,
        same_statement_after_id = 21,
        required_inserted_row_count = 8,
    };
    const char *path = MYLITE_RUNTIME_TEST_FILE_PATH;
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    char *duplicate_physical = NULL;
    char *required_physical = NULL;
    int failures = 0;

    remove_runtime_test_files();
    failures += expect_status(mylite_open(path, &database), MYLITE_OK, "open insert ignore file");
    failures += execute_sql(database, "CREATE DATABASE mylite_ii31", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_ii31", MYLITE_DONE);

    failures += execute_sql(database,
                            "CREATE TABLE ign_dup ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
                            "v INT UNIQUE, nn INT NOT NULL, s VARCHAR(10) NOT NULL DEFAULT 'ok') "
                            "AUTO_INCREMENT=10",
                            MYLITE_DONE);
    duplicate_physical = expected_physical_table_name("mylite_ii31", "ign_dup");
    if (duplicate_physical == NULL) {
        fprintf(stderr, "out of memory while building ign_dup physical table name\n");
        failures = 1;
    }

    failures +=
        execute_sql(database, "INSERT INTO ign_dup(v, nn, s) VALUES (1, 10, 'one')", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), duplicate_seed_id,
                             "insert ignore duplicate seed id");
    failures += prepare_sql(database,
                            "INSERT IGNORE INTO ign_dup(v, nn, s) VALUES "
                            "(1, 11, 'dup'), (2, 20, 'two'), "
                            "(1, 12, 'dup'), (3, 30, 'tri')",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "insert ignore values duplicates");
    failures += expect_int64(mylite_affected_rows(stmt), 2, "insert ignore values affected rows");
    failures += expect_int(mylite_warning_count(database), 2,
                           "insert ignore values duplicate warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_duplicate_entry,
                           "insert ignore values first duplicate warning");
    failures += expect_contains(mylite_warning_message(database, 0), "Duplicate entry '1'",
                                "insert ignore values first duplicate message");
    failures += expect_int((int)mylite_warning_code(database, 1), mysql_warning_duplicate_entry,
                           "insert ignore values second duplicate warning");
    failures += expect_int64((int64_t)mylite_last_insert_id(database), duplicate_first_accepted_id,
                             "insert ignore values last insert id");
    mylite_finalize(stmt);
    stmt = NULL;
    if (duplicate_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, duplicate_physical, "COUNT(*)", "", 3,
                                                 "insert ignore duplicate row count");
        failures += expect_sqlite_physical_int64(path, duplicate_physical, "id", "WHERE v = 2",
                                                 duplicate_first_accepted_id,
                                                 "insert ignore first accepted generated id");
        failures += expect_sqlite_physical_int64(path, duplicate_physical, "id", "WHERE v = 3",
                                                 duplicate_second_accepted_id,
                                                 "insert ignore second accepted generated id");
    }

    failures +=
        execute_sql(database, "INSERT INTO ign_dup(v, nn, s) VALUES (4, 40, 'four')", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), duplicate_after_values_id,
                             "insert ignore values consumed ignored ids");

    failures += prepare_sql(database, "INSERT IGNORE INTO ign_dup SET v = 2, nn = 99, s = 'set'",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "insert ignore set duplicate");
    failures += expect_int64(mylite_affected_rows(stmt), 0, "insert ignore set affected rows");
    failures +=
        expect_int(mylite_warning_count(database), 1, "insert ignore set duplicate warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_duplicate_entry,
                           "insert ignore set duplicate warning");
    failures += expect_int64((int64_t)mylite_last_insert_id(database), duplicate_after_values_id,
                             "insert ignore set leaves last insert id");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        execute_sql(database, "INSERT INTO ign_dup(v, nn, s) VALUES (5, 50, 'five')", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), duplicate_after_set_id,
                             "insert ignore set consumed ignored id");
    failures += prepare_sql(database,
                            "INSERT IGNORE INTO ign_dup(v, nn, s) VALUES "
                            "(6, 60, 'six'), (6, 61, 'dup'), (7, 70, 'seven')",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE,
                              "insert ignore duplicate against accepted row");
    failures +=
        expect_int64(mylite_affected_rows(stmt), 2, "insert ignore same statement affected rows");
    failures +=
        expect_int(mylite_warning_count(database), 1, "insert ignore same statement warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_duplicate_entry,
                           "insert ignore same statement duplicate warning");
    failures +=
        expect_int64((int64_t)mylite_last_insert_id(database), same_statement_first_accepted_id,
                     "insert ignore same statement last insert id");
    mylite_finalize(stmt);
    stmt = NULL;
    if (duplicate_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, duplicate_physical, "id", "WHERE v = 6",
                                                 same_statement_first_accepted_id,
                                                 "insert ignore same statement first id");
        failures += expect_sqlite_physical_int64(path, duplicate_physical, "id", "WHERE v = 7",
                                                 same_statement_second_accepted_id,
                                                 "insert ignore same statement second id");
    }
    failures +=
        execute_sql(database, "INSERT INTO ign_dup(v, nn, s) VALUES (8, 80, 'eight')", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database), same_statement_after_id,
                             "insert ignore same statement consumed ignored id");

    failures += execute_sql(database,
                            "CREATE TABLE ign_req ("
                            "id INT NOT NULL, s VARCHAR(4) NOT NULL, "
                            "d DATE NOT NULL, opt INT)",
                            MYLITE_DONE);
    required_physical = expected_physical_table_name("mylite_ii31", "ign_req");
    if (required_physical == NULL) {
        fprintf(stderr, "out of memory while building ign_req physical table name\n");
        failures = 1;
    }

    failures += prepare_sql(database, "INSERT IGNORE INTO ign_req(opt) VALUES (10), (20)",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "insert ignore omitted required");
    failures +=
        expect_int64(mylite_affected_rows(stmt), 2, "insert ignore omitted required affected rows");
    failures += expect_int(mylite_warning_count(database), 3,
                           "insert ignore omitted required warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_no_default,
                           "insert ignore omitted id warning");
    failures += expect_contains(mylite_warning_message(database, 0), "Field 'id'",
                                "insert ignore omitted id message");
    failures += expect_int((int)mylite_warning_code(database, 1), mysql_warning_no_default,
                           "insert ignore omitted string warning");
    failures += expect_int((int)mylite_warning_code(database, 2), mysql_warning_no_default,
                           "insert ignore omitted date warning");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "INSERT IGNORE INTO ign_req(id, s, d) "
                            "VALUES (DEFAULT, DEFAULT, DEFAULT)",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "insert ignore default required");
    failures += expect_int(mylite_warning_count(database), 3,
                           "insert ignore default required warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_no_default,
                           "insert ignore explicit default id warning");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "INSERT IGNORE INTO ign_req(id, s, d) VALUES (NULL, NULL, NULL)",
                    MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "insert ignore null required");
    failures +=
        expect_int(mylite_warning_count(database), 3, "insert ignore null required warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_bad_null,
                           "insert ignore null id warning");
    failures += expect_contains(mylite_warning_message(database, 0), "Column 'id'",
                                "insert ignore null id message");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "INSERT IGNORE INTO ign_req(id, s, d, opt) "
                            "VALUES (NULL, NULL, NULL, 40), (NULL, NULL, NULL, 50)",
                            MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_DONE, "insert ignore repeated null required");
    failures +=
        expect_int64(mylite_affected_rows(stmt), 2, "insert ignore repeated null affected rows");
    failures +=
        expect_int(mylite_warning_count(database), 3, "insert ignore repeated null warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_bad_null,
                           "insert ignore repeated null id warning");
    failures += expect_int((int)mylite_warning_code(database, 1), mysql_warning_bad_null,
                           "insert ignore repeated null string warning");
    failures += expect_int((int)mylite_warning_code(database, 2), mysql_warning_bad_null,
                           "insert ignore repeated null date warning");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "INSERT IGNORE INTO ign_req SET "
                            "id = DEFAULT, s = DEFAULT, d = DEFAULT",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "insert ignore set default required");
    failures +=
        expect_int(mylite_warning_count(database), 3, "insert ignore set default warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_no_default,
                           "insert ignore set default warning");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "INSERT IGNORE INTO ign_req SET opt = 30", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "insert ignore set omitted required");
    failures +=
        expect_int(mylite_warning_count(database), 3, "insert ignore set omitted warning count");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "CREATE TABLE ign_pk_null (id INT NOT NULL PRIMARY KEY)",
                            MYLITE_DONE);
    failures += prepare_sql(database, "INSERT IGNORE INTO ign_pk_null(id) VALUES (NULL), (NULL)",
                            MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_DONE, "insert ignore primary null duplicate");
    failures += expect_int64(mylite_affected_rows(stmt), 1,
                             "insert ignore primary null duplicate affected rows");
    failures += expect_int(mylite_warning_count(database), 2,
                           "insert ignore primary null duplicate warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_bad_null,
                           "insert ignore primary null duplicate null warning");
    failures += expect_int((int)mylite_warning_code(database, 1), mysql_warning_duplicate_entry,
                           "insert ignore primary null duplicate duplicate warning");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT id FROM ign_pk_null", pk_null_columns, 1,
                                   pk_null_values, 1, "insert ignore primary null stored row");

    if (required_physical != NULL) {
        failures += expect_sqlite_physical_int64(path, required_physical, "COUNT(*)", "",
                                                 required_inserted_row_count,
                                                 "insert ignore required row count");
        failures += expect_sqlite_physical_int64(path, required_physical, "id", "WHERE opt = 10", 0,
                                                 "insert ignore omitted numeric default");
        failures += expect_sqlite_physical_text(path, required_physical, "s", "WHERE opt = 10", "",
                                                "insert ignore omitted string default");
        failures += expect_sqlite_physical_text(path, required_physical, "d", "WHERE opt = 10",
                                                "0000-00-00", "insert ignore omitted date default");
        failures += expect_sqlite_physical_text(path, required_physical, "d", "WHERE opt = 30",
                                                "0000-00-00", "insert ignore set date default");
    }

    free(duplicate_physical);
    free(required_physical);
    mylite_close(database);
    remove_runtime_test_files();
    return failures;
}

static int test_insert_on_duplicate_key_update_execution(void)
{
    // NOLINTBEGIN(readability-function-cognitive-complexity,readability-magic-numbers)
    static const char *const id_a_b_c_columns[] = {"id", "a", "b", "c"};
    static const char *const du_seed_values[] = {"1", "10", "100", "1", "2", "20", "200", "2"};
    static const char *const du_ignore_values[] = {"1",   "10", "100", "1",  "2",   "20",
                                                   "200", "2",  "4",   "40", "400", "4"};
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    uint64_t last_insert_id = 0U;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open ODKU database");
    failures += execute_sql(database, "CREATE DATABASE mylite_odku32", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_odku32", MYLITE_DONE);

    failures += execute_sql(database,
                            "CREATE TABLE odku_base ("
                            "id INT PRIMARY KEY, a INT UNIQUE, b INT UNIQUE, "
                            "c INT DEFAULT 9, d INT DEFAULT 5)",
                            MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO odku_base VALUES (1,10,100,1,1),(2,20,200,2,2)",
                            MYLITE_DONE);
    failures +=
        execute_sql_expect_done_affected(database,
                                         "INSERT INTO odku_base VALUES (3,30,300,3,DEFAULT) "
                                         "ON DUPLICATE KEY UPDATE c = VALUES(c)",
                                         1, "ODKU no-conflict insert affected rows");
    failures += expect_int(mylite_warning_count(database), 1, "ODKU insert path warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_deprecated_syntax,
                           "ODKU insert path VALUES warning code");

    failures +=
        execute_sql_expect_done_affected(database,
                                         "INSERT INTO odku_base VALUES (4,10,400,4,7) "
                                         "ON DUPLICATE KEY UPDATE c = VALUES(c), d = VALUES(d)",
                                         2, "ODKU changed duplicate affected rows");
    failures += expect_int(mylite_warning_count(database), 2, "ODKU changed VALUES warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_deprecated_syntax,
                           "ODKU first VALUES warning code");
    failures += execute_sql_expect_done_affected(database,
                                                 "INSERT INTO odku_base VALUES (5,10,500,4,7) "
                                                 "ON DUPLICATE KEY UPDATE c = c, d = d",
                                                 0, "ODKU no-op duplicate affected rows");
    failures += expect_select_rows(
        database, "SELECT id, a, b, c FROM odku_base ORDER BY id", id_a_b_c_columns, 4,
        (const char *[]){"1", "10", "100", "4", "2", "20", "200", "2", "3", "30", "300", "3"}, 3,
        "ODKU base rows");

    failures += execute_sql(
        database, "CREATE TABLE odku_mix (id INT PRIMARY KEY, u INT UNIQUE, v INT)", MYLITE_DONE);
    failures +=
        execute_sql(database, "INSERT INTO odku_mix VALUES (1,10,100),(2,20,200)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database,
        "INSERT INTO odku_mix VALUES (3,10,101),(4,40,400),(5,20,200) "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        3, "ODKU mixed insert update no-op affected rows");
    failures += expect_int(mylite_warning_count(database), 1, "ODKU mixed VALUES warnings");
    failures += expect_select_rows(
        database, "SELECT id, u, v FROM odku_mix ORDER BY id", (const char *[]){"id", "u", "v"}, 3,
        (const char *[]){"1", "10", "101", "2", "20", "200", "4", "40", "400"}, 3,
        "ODKU mixed rows");

    failures += execute_sql(database,
                            "CREATE TABLE odku_order ("
                            "id INT PRIMARY KEY, a INT UNIQUE, b INT, c INT)",
                            MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO odku_order VALUES (1,10,100,1000)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(database,
                                                 "INSERT INTO odku_order VALUES (2,10,40,400) "
                                                 "ON DUPLICATE KEY UPDATE b = a + 1, c = b + 1",
                                                 2, "ODKU assignment order affected rows");
    failures += expect_select_rows(
        database, "SELECT a, b, c FROM odku_order WHERE id = 1", (const char *[]){"a", "b", "c"}, 3,
        (const char *[]){"10", "11", "12"}, 1, "ODKU assignment order row");
    failures +=
        execute_sql_expect_done_affected(database,
                                         "INSERT INTO odku_order VALUES (3,10,50,500) "
                                         "ON DUPLICATE KEY UPDATE b = 100, b = b + 1, c = b + 1",
                                         2, "ODKU repeated target affected rows");
    failures += expect_select_rows(database, "SELECT b, c FROM odku_order WHERE id = 1",
                                   (const char *[]){"b", "c"}, 2, (const char *[]){"101", "102"}, 1,
                                   "ODKU repeated target row");

    failures += execute_sql(database,
                            "CREATE TABLE odku_default ("
                            "id INT PRIMARY KEY, u INT UNIQUE, d INT DEFAULT 7, n INT)",
                            MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO odku_default VALUES (1,1,1,2)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(database,
                                                 "INSERT INTO odku_default VALUES (2,1,3,4) "
                                                 "ON DUPLICATE KEY UPDATE d = DEFAULT, n = DEFAULT",
                                                 2, "ODKU DEFAULT affected rows");
    failures += expect_select_rows(database, "SELECT d, n FROM odku_default WHERE id = 1",
                                   (const char *[]){"d", "n"}, 2, (const char *[]){"7", NULL}, 1,
                                   "ODKU DEFAULT row");

    failures +=
        execute_sql(database, "CREATE TABLE ali (a INT PRIMARY KEY, b INT, c INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO ali VALUES (1,2,3)", MYLITE_DONE);
    failures += prepare_sql(database,
                            "INSERT INTO ali VALUES (1,4,5) AS n(a,b,c) "
                            "ON DUPLICATE KEY UPDATE b = b, c = n.b",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Column 'b' in field list is ambiguous",
                                  "ODKU ambiguous alias");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        expect_select_rows(database, "SELECT a, b, c FROM ali", (const char *[]){"a", "b", "c"}, 3,
                           (const char *[]){"1", "2", "3"}, 1, "ODKU ambiguous alias rollback");
    failures += prepare_sql(database,
                            "INSERT INTO ali VALUES (1,4,5) AS n(x,y,z) "
                            "ON DUPLICATE KEY UPDATE c = q",
                            MYLITE_OK, &stmt);
    failures +=
        expect_exec_error(stmt, database, "Unknown column 'q'", "ODKU unknown alias reference");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql_expect_done_affected(database,
                                                 "INSERT INTO ali VALUES (1,6,7) AS n(x,y,z) "
                                                 "ON DUPLICATE KEY UPDATE b = y, c = y + 1",
                                                 2, "ODKU non-conflicting aliases affected rows");
    failures +=
        expect_select_rows(database, "SELECT a, b, c FROM ali", (const char *[]){"a", "b", "c"}, 3,
                           (const char *[]){"1", "6", "7"}, 1, "ODKU non-conflicting aliases row");
    failures += prepare_sql(database,
                            "INSERT INTO ali(a,b) VALUES (1,8) AS n(x) "
                            "ON DUPLICATE KEY UPDATE c = x",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "column names list have different column counts",
                                  "ODKU alias column count mismatch");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(
        database, "CREATE TABLE odku_set (id INT PRIMARY KEY, u INT UNIQUE, v INT)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO odku_set VALUES (1,1,10)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database,
        "INSERT INTO odku_set SET id = 2, u = 1, v = 20 AS n(x,y,z) "
        "ON DUPLICATE KEY UPDATE v = z",
        2, "ODKU SET alias affected rows");
    failures += expect_select_rows(database, "SELECT id, u, v FROM odku_set",
                                   (const char *[]){"id", "u", "v"}, 3,
                                   (const char *[]){"1", "1", "20"}, 1, "ODKU SET alias row");
    failures +=
        execute_sql_expect_done_affected(database,
                                         "INSERT INTO odku_set SET v = 30, id = 1 AS n(x,y) "
                                         "ON DUPLICATE KEY UPDATE v = x",
                                         2, "ODKU SET alias assignment-order affected rows");
    failures += expect_select_rows(
        database, "SELECT id, u, v FROM odku_set", (const char *[]){"id", "u", "v"}, 3,
        (const char *[]){"1", "1", "30"}, 1, "ODKU SET alias assignment-order row");

    failures += execute_sql(database,
                            "CREATE TABLE odku_multi ("
                            "id INT PRIMARY KEY, a INT, b INT, c INT, "
                            "UNIQUE KEY zb(a), UNIQUE KEY aa(b))",
                            MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO odku_multi VALUES (1,1,10,100),(2,2,20,200)",
                            MYLITE_DONE);
    failures += execute_sql_expect_done_affected(database,
                                                 "INSERT INTO odku_multi VALUES (3,1,20,300) "
                                                 "ON DUPLICATE KEY UPDATE c = VALUES(c)",
                                                 2, "ODKU multiple unique affected rows");
    failures += expect_select_rows(
        database, "SELECT id, c FROM odku_multi ORDER BY id", (const char *[]){"id", "c"}, 2,
        (const char *[]){"1", "300", "2", "200"}, 2, "ODKU multiple unique catalog order");

    failures += execute_sql(
        database, "CREATE TABLE odku_null (id INT PRIMARY KEY, u INT UNIQUE, v INT)", MYLITE_DONE);
    failures +=
        execute_sql_expect_done_affected(database,
                                         "INSERT INTO odku_null VALUES (1,NULL,10),(2,NULL,20) "
                                         "ON DUPLICATE KEY UPDATE v = 99",
                                         2, "ODKU nullable unique NULL affected rows");
    failures += expect_select_rows(
        database, "SELECT id, u, v FROM odku_null ORDER BY id", (const char *[]){"id", "u", "v"}, 3,
        (const char *[]){"1", NULL, "10", "2", NULL, "20"}, 2, "ODKU nullable unique NULL rows");

    failures += execute_sql(database,
                            "CREATE TABLE du ("
                            "id INT PRIMARY KEY, a INT UNIQUE, b INT UNIQUE, c INT)",
                            MYLITE_DONE);
    failures +=
        execute_sql(database, "INSERT INTO du VALUES (1,10,100,1),(2,20,200,2)", MYLITE_DONE);
    failures += prepare_sql(database,
                            "INSERT INTO du VALUES (3,10,300,3) "
                            "ON DUPLICATE KEY UPDATE b = 200, c = VALUES(c)",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry '200' for key 'du.b'",
                                  "ODKU update-branch duplicate rollback");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        expect_select_rows(database, "SELECT id, a, b, c FROM du ORDER BY id", id_a_b_c_columns, 4,
                           du_seed_values, 2, "ODKU duplicate rollback rows");

    failures += execute_sql(database,
                            "CREATE TABLE du_ignore ("
                            "id INT PRIMARY KEY, a INT UNIQUE, b INT UNIQUE, c INT)",
                            MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO du_ignore VALUES (1,10,100,1),(2,20,200,2)",
                            MYLITE_DONE);
    failures += execute_sql_expect_done_affected(
        database,
        "INSERT IGNORE INTO du_ignore VALUES (3,10,300,3),(4,40,400,4) "
        "ON DUPLICATE KEY UPDATE b = 200, c = VALUES(c)",
        1, "ODKU IGNORE update duplicate affected rows");
    failures +=
        expect_int(mylite_warning_count(database), 2, "ODKU IGNORE update duplicate warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_deprecated_syntax,
                           "ODKU IGNORE first warning code");
    failures += expect_int((int)mylite_warning_code(database, 1), mysql_warning_duplicate_entry,
                           "ODKU IGNORE second warning code");
    failures += expect_contains(mylite_warning_message(database, 1),
                                "Duplicate entry '200' for key 'du_ignore.b'",
                                "ODKU IGNORE duplicate warning message");
    failures += expect_select_rows(database, "SELECT id, a, b, c FROM du_ignore ORDER BY id",
                                   id_a_b_c_columns, 4, du_ignore_values, 3,
                                   "ODKU IGNORE continuation rows");

    failures += execute_sql(database,
                            "CREATE TABLE ai_odku ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, u INT UNIQUE, v INT) "
                            "AUTO_INCREMENT=30",
                            MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO ai_odku VALUES (5,1,10)", MYLITE_DONE);
    last_insert_id = mylite_last_insert_id(database);
    failures += execute_sql_expect_done_affected(
        database, "INSERT INTO ai_odku(u,v) VALUES (1,11) ON DUPLICATE KEY UPDATE v = VALUES(v)", 2,
        "ODKU auto-increment duplicate affected rows");
    failures += expect_int64((int64_t)mylite_last_insert_id(database), (int64_t)last_insert_id,
                             "ODKU duplicate leaves last insert id");
    failures += execute_sql_expect_done_affected(database, "INSERT INTO ai_odku(u,v) VALUES (2,12)",
                                                 1, "ODKU auto-increment next affected rows");
    failures += expect_int64((int64_t)mylite_last_insert_id(database), 31,
                             "ODKU duplicate consumed generated id");
    failures += execute_sql_expect_done_affected(
        database, "INSERT INTO ai_odku(u,v) VALUES (3,13) ON DUPLICATE KEY UPDATE v = VALUES(v)", 1,
        "ODKU generated insert affected rows");
    failures += expect_int64((int64_t)mylite_last_insert_id(database), 32,
                             "ODKU insert path sets last insert id");
    failures += expect_select_rows(
        database, "SELECT id, u, v FROM ai_odku ORDER BY u", (const char *[]){"id", "u", "v"}, 3,
        (const char *[]){"5", "1", "11", "31", "2", "12", "32", "3", "13"}, 3,
        "ODKU auto-increment rows");

    mylite_close(database);
    // NOLINTEND(readability-function-cognitive-complexity,readability-magic-numbers)
    return failures;
}

static int test_select_table_core_execution(void)
{
    static const char *const visible_columns[] = {"a", "b", "CamelCase"};
    static const char *const visible_values[] = {"1", "one", "7", "2", "two", "8"};
    static const char *const hidden_columns[] = {"hidden"};
    static const char *const hidden_values[] = {"99", "88"};
    static const char *const qualified_columns[] = {"a", "b"};
    static const char *const qualified_values[] = {"1", "one", "2", "two"};
    static const char *const alias_columns[] = {"x", "label b", "hidden alias", "cc"};
    static const char *const alias_values[] = {"1", "one", "99", "7", "2", "two", "88", "8"};
    static const char *const duplicate_columns[] = {"x", "x"};
    static const char *const duplicate_values[] = {"1", "one", "2", "two"};
    static const char *const case_columns[] = {"camelcase", "CAMELCASE", "CamelCase"};
    static const char *const case_values[] = {"7", "7", "7", "8", "8", "8"};
    static const char *const mixed_columns[] = {"a", "a", "b", "CamelCase", "hidden"};
    static const char *const mixed_values[] = {"1", "1", "one", "7", "99",
                                               "2", "2", "two", "8", "88"};
    static const char *const expression_columns[] = {"a + 1"};
    static const char *const expression_values[] = {"2", "3"};
    static const char *const literal_columns[] = {"1"};
    static const char *const literal_values[] = {"1", "1"};
    static const struct expected_column_metadata alias_metadata[] = {
        {"x", "mylite_select15", "alias", "t", "a"},
        {"a", "mylite_select15", "alias", "t", "a"},
        {"b", "mylite_select15", "alias", "t", "b"},
        {"CamelCase", "mylite_select15", "alias", "t", "CamelCase"},
    };
    static const struct expected_column_metadata case_metadata[] = {
        {"camelcase", "mylite_select15", "t", "t", "CamelCase"},
        {"CAMELCASE", "mylite_select15", "t", "t", "CamelCase"},
        {"CamelCase", "mylite_select15", "t", "t", "CamelCase"},
    };
    static const int mixed_column_count = 5;
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    uint64_t last_insert_id = 0U;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open select database");

    failures += expect_prepare_error(database, "SELECT * FROM t", MYLITE_EXEC_ERROR,
                                     "No database selected", "select no database");

    failures += execute_sql(database, "CREATE DATABASE mylite_select15", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE mylite_select15.t ("
                            "a INT, b VARCHAR(10), hidden INT INVISIBLE, CamelCase INT)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO mylite_select15.t (a, b, hidden, CamelCase) VALUES "
                            "(1, 'one', 99, 7), (2, 'two', 88, 8)",
                            MYLITE_DONE);
    last_insert_id = mylite_last_insert_id(database);

    failures += expect_select_rows(database, "SELECT * FROM mylite_select15.t", visible_columns, 3,
                                   visible_values, 2, "schema-qualified select");
    failures += expect_select_rows(database, "SELECT t.* FROM mylite_select15.t", visible_columns,
                                   3, visible_values, 2, "table wildcard select");

    failures += execute_sql(database, "USE mylite_select15", MYLITE_DONE);
    failures += expect_select_rows(database, "SELECT * FROM t", visible_columns, 3, visible_values,
                                   2, "selected schema select");
    failures +=
        expect_select_rows(database, "SELECT mylite_select15.t.a, t.b FROM t", qualified_columns, 2,
                           qualified_values, 2, "qualified column select");
    failures +=
        expect_select_rows(database, "SELECT alias.a, alias.b FROM t AS alias", qualified_columns,
                           2, qualified_values, 2, "alias-qualified column select");
    failures += expect_select_rows(database, "SELECT mylite_select15.t.* FROM t", visible_columns,
                                   3, visible_values, 2, "schema wildcard over selected table");
    failures += expect_select_rows(database, "SELECT alias.* FROM t AS alias", visible_columns, 3,
                                   visible_values, 2, "AS alias wildcard select");
    failures += expect_select_rows(database, "SELECT alias.* FROM t alias", visible_columns, 3,
                                   visible_values, 2, "bare alias wildcard select");
    failures += expect_select_rows(database, "SELECT hidden FROM t", hidden_columns, 1,
                                   hidden_values, 2, "explicit invisible column");
    failures += expect_select_rows(database,
                                   "SELECT a AS x, b `label b`, hidden AS `hidden alias`, "
                                   "CamelCase AS 'cc' FROM t",
                                   alias_columns, 4, alias_values, 2, "projection aliases");
    failures += expect_select_rows(database, "SELECT a AS x, b AS x FROM t", duplicate_columns, 2,
                                   duplicate_values, 2, "duplicate projection labels");
    failures +=
        expect_select_rows(database, "SELECT camelcase, CAMELCASE, `CamelCase` FROM t",
                           case_columns, 3, case_values, 2, "case-insensitive column lookup");
    failures +=
        expect_select_rows(database, "SELECT a, t.*, hidden FROM t", mixed_columns,
                           mixed_column_count, mixed_values, 2, "mixed qualified wildcard select");

    failures +=
        prepare_sql(database, "SELECT alias.a AS x, alias.* FROM t AS alias", MYLITE_OK, &stmt);
    failures += expect_column_metadata(stmt, alias_metadata, 4, "alias result metadata");
    failures += expect_null_text(mylite_column_schema_name(stmt, -1), "negative metadata column");
    failures += expect_null_text(mylite_column_table_name(stmt, 4), "past-end metadata column");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "SELECT camelcase, CAMELCASE, `CamelCase` FROM t", MYLITE_OK, &stmt);
    failures += expect_column_metadata(stmt, case_metadata, 3, "case result metadata");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        expect_prepare_error(database, "SELECT * FROM missing_select15.t", MYLITE_EXEC_ERROR,
                             "Unknown database 'missing_select15'", "select missing schema");
    failures += expect_prepare_error(
        database, "SELECT * FROM mylite_select15.missing_t", MYLITE_EXEC_ERROR,
        "Table 'mylite_select15.missing_t' doesn't exist", "select missing table");
    failures += expect_prepare_error(database, "SELECT missing_col FROM t", MYLITE_EXEC_ERROR,
                                     "Unknown column 'missing_col' in 'field list'",
                                     "select missing column");
    failures += expect_prepare_error(database, "SELECT missing_alias.a FROM t", MYLITE_EXEC_ERROR,
                                     "Unknown column 'missing_alias.a' in 'field list'",
                                     "select missing qualifier");
    failures += expect_prepare_error(database, "SELECT t.a FROM t AS alias", MYLITE_EXEC_ERROR,
                                     "Unknown column 't.a' in 'field list'",
                                     "select alias hides base column qualifier");
    failures += expect_prepare_error(database, "SELECT mylite_select15.t.a FROM t AS alias",
                                     MYLITE_EXEC_ERROR,
                                     "Unknown column 'mylite_select15.t.a' in 'field list'",
                                     "select alias hides schema qualifier");
    failures += expect_prepare_error(database, "SELECT T.a FROM t", MYLITE_EXEC_ERROR,
                                     "Unknown column 'T.a' in 'field list'",
                                     "select qualifier case sensitivity");
    failures += expect_prepare_error(database, "SELECT ALIAS.a FROM t AS alias", MYLITE_EXEC_ERROR,
                                     "Unknown column 'ALIAS.a' in 'field list'",
                                     "select alias case sensitivity");
    failures +=
        expect_prepare_error(database, "SELECT missing_alias.* FROM t", MYLITE_EXEC_ERROR,
                             "Unknown table 'missing_alias'", "select missing wildcard qualifier");
    failures +=
        expect_prepare_error(database, "SELECT t.* FROM t AS alias", MYLITE_EXEC_ERROR,
                             "Unknown table 't'", "select alias hides base wildcard qualifier");
    failures += expect_prepare_error(database, "SELECT missing_select15.t.* FROM t",
                                     MYLITE_EXEC_ERROR, "Unknown table 'missing_select15.t'",
                                     "select missing schema wildcard qualifier");
    failures += expect_select_rows(database, "SELECT a + 1 FROM t", expression_columns, 1,
                                   expression_values, 2, "select expression projection");
    failures += expect_select_rows(database, "SELECT 1 FROM t", literal_columns, 1, literal_values,
                                   2, "select literal projection");
    failures +=
        expect_prepare_error(database, "SELECT x'0a' FROM t", MYLITE_UNSUPPORTED,
                             "Unsupported SELECT projection", "unsupported hex literal projection");
    failures +=
        expect_prepare_error(database, "SELECT b'1010' FROM t", MYLITE_UNSUPPORTED,
                             "Unsupported SELECT projection", "unsupported bit literal projection");

    failures += prepare_sql(database, "SELECT * FROM t", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "select side effect first row");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "select side effect second row");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "select side effect done");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "select affected rows");
    failures += expect_int64((int64_t)mylite_last_insert_id(database), (int64_t)last_insert_id,
                             "select last insert id unchanged");
    mylite_finalize(stmt);

    mylite_close(database);
    return failures;
}

static int test_inner_join_execution(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    static const char *const join_columns[] = {"left_id", "right_id", "label"};
    static const char *const join_values[] = {"1", "10", "right-a", "1", "11", "right-b"};
    static const char *const comma_columns[] = {"id", "id"};
    static const char *const comma_values[] = {"1", "10", "1", "11"};
    static const char *const star_columns[] = {"id",      "shared", "label", "id",
                                               "left_id", "shared", "label"};
    static const char *const star_values[] = {"1", "100", "left-one", "10", "1", "100", "right-a",
                                              "1", "100", "left-one", "11", "1", "100", "right-b"};
    static const char *const qualified_star_columns[] = {"id", "shared", "label", "label"};
    static const char *const qualified_star_values[] = {"1", "100", "left-one", "right-a",
                                                        "1", "100", "left-one", "right-b"};
    static const char *const using_star_columns[] = {"shared", "id",      "label",
                                                     "id",     "left_id", "label"};
    static const char *const using_star_values[] = {"100", "1", "left-one", "10", "1", "right-a",
                                                    "100", "1", "left-one", "11", "1", "right-b"};
    static const char *const using_multi_star_columns[] = {"id", "shared", "label", "left_id",
                                                           "label"};
    static const char *const comma_using_star_columns[] = {"id",      "shared", "label", "id",
                                                           "left_id", "shared", "label", "note"};
    static const char *const shared_column[] = {"shared"};
    static const char *const shared_values[] = {"100", "100"};
    static const char *const left_id_column[] = {"left_id"};
    static const char *const count_column[] = {"c"};
    static const char *const cartesian_count[] = {"6"};
    static const char *const zero_count[] = {"0"};
    static const char *const chained_columns[] = {"left_id", "right_id", "note"};
    static const char *const chained_values[] = {"1", "10", "e1", "1", "10", "e2",
                                                 "1", "11", "e1", "1", "11", "e2"};
    static const struct expected_column_metadata metadata[] = {
        {"left_id", "mylite_inner_join", "l", "left_t", "id"},
        {"label", "mylite_inner_join", "r", "right_t", "label"},
    };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open join database");
    failures += execute_sql(database, "CREATE DATABASE mylite_inner_join", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_inner_join", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE left_t ("
                            "id INT PRIMARY KEY, shared INT, label VARCHAR(20))",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE right_t ("
                            "id INT PRIMARY KEY, left_id INT, shared INT, label VARCHAR(20))",
                            MYLITE_DONE);
    failures +=
        execute_sql(database, "CREATE TABLE extra_t (id INT, note VARCHAR(20))", MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO left_t VALUES "
                            "(1,100,'left-one'),(2,200,'left-two')",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO right_t VALUES "
                            "(10,1,100,'right-a'),(11,1,100,'right-b'),(20,2,999,'right-c')",
                            MYLITE_DONE);
    failures +=
        execute_sql(database, "INSERT INTO extra_t VALUES (1,'e1'),(1,'e2'),(2,'e3')", MYLITE_DONE);

    failures += expect_select_rows(database,
                                   "SELECT l.id AS left_id, r.id AS right_id, r.label "
                                   "FROM left_t AS l JOIN right_t AS r ON l.id = r.left_id "
                                   "WHERE r.id < 20 ORDER BY r.id",
                                   join_columns, 3, join_values, 2, "inner join on rows");
    failures += expect_select_rows(database,
                                   "SELECT left_t.id, right_t.id "
                                   "FROM left_t, right_t "
                                   "WHERE left_t.id = right_t.left_id AND right_t.id < 20 "
                                   "ORDER BY right_t.id",
                                   comma_columns, 2, comma_values, 2, "comma join where rows");
    failures +=
        expect_select_rows(database,
                           "SELECT * FROM left_t JOIN right_t ON left_t.id = right_t.left_id "
                           "WHERE right_t.id < 20 ORDER BY right_t.id",
                           star_columns, 7, star_values, 2, "inner join wildcard rows");
    failures += expect_select_rows(database,
                                   "SELECT l.*, r.label "
                                   "FROM left_t AS l INNER JOIN right_t AS r ON l.id = r.left_id "
                                   "WHERE r.id < 20 ORDER BY r.id",
                                   qualified_star_columns, 4, qualified_star_values, 2,
                                   "inner join qualified wildcard rows");
    failures += expect_select_rows(database,
                                   "SELECT * FROM left_t JOIN right_t USING (shared) "
                                   "ORDER BY right_t.id",
                                   using_star_columns, 6, using_star_values, 2,
                                   "inner join using wildcard rows");
    failures += expect_select_rows(database,
                                   "SELECT * FROM left_t JOIN right_t USING (shared, id) "
                                   "ORDER BY right_t.id",
                                   using_multi_star_columns, 5, NULL, 0,
                                   "inner join multi using wildcard order");
    failures +=
        expect_select_rows(database,
                           "SELECT * FROM left_t AS a, "
                           "right_t AS r JOIN extra_t AS e USING (id) LIMIT 0",
                           comma_using_star_columns, 8, NULL, 0, "comma join using wildcard order");
    failures += expect_select_rows(database,
                                   "SELECT shared FROM left_t JOIN right_t USING (shared) "
                                   "ORDER BY right_t.id",
                                   shared_column, 1, shared_values, 2,
                                   "inner join using unqualified column");
    failures += expect_select_rows(database,
                                   "SELECT l.id AS left_id FROM left_t AS l JOIN right_t AS r "
                                   "ON NULL ORDER BY l.id",
                                   left_id_column, 1, NULL, 0, "inner join null on predicate");
    failures += expect_select_rows(database, "SELECT COUNT(*) AS c FROM left_t CROSS JOIN right_t",
                                   count_column, 1, cartesian_count, 1, "cross join cartesian");
    failures +=
        expect_select_rows(database, "SELECT COUNT(*) AS c FROM left_t JOIN right_t", count_column,
                           1, cartesian_count, 1, "conditionless inner join cartesian");
    failures +=
        expect_select_rows(database,
                           "SELECT shared FROM left_t JOIN right_t "
                           "USING (shared, shared) ORDER BY right_t.id",
                           shared_column, 1, shared_values, 2, "inner join duplicate using column");
    failures +=
        expect_prepare_error(database,
                             "SELECT shared FROM left_t AS l1 JOIN right_t AS r1 USING (shared), "
                             "left_t AS l2 JOIN right_t AS r2 USING (shared)",
                             MYLITE_EXEC_ERROR, "Column 'shared' in field list is ambiguous",
                             "inner join independent using ambiguity");
    failures +=
        expect_int(mylite_warning_count(database), 1, "inner join using ambiguity warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_ambiguous_column,
                           "inner join using ambiguity warning code");
    failures += expect_prepare_error(database,
                                     "SELECT * FROM left_t AS l JOIN right_t AS r USING (shared) "
                                     "JOIN extra_t AS e USING (id)",
                                     MYLITE_EXEC_ERROR, "Column 'id' in from clause is ambiguous",
                                     "inner join using left operand ambiguity");
    failures += expect_int(mylite_warning_count(database), 1,
                           "inner join using left operand ambiguity warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_ambiguous_column,
                           "inner join using left operand ambiguity warning code");
    failures += expect_select_rows(database,
                                   "SELECT l.id AS left_id, r.id AS right_id, e.note "
                                   "FROM left_t AS l JOIN right_t AS r ON l.id = r.left_id "
                                   "JOIN extra_t AS e ON e.id = l.id "
                                   "WHERE r.id < 20 ORDER BY r.id, e.note",
                                   chained_columns, 3, chained_values, 4, "chained inner join");
    failures += expect_select_rows(database,
                                   "SELECT COUNT(*) AS c "
                                   "FROM left_t JOIN right_t "
                                   "ON left_t.id = right_t.left_id AND (right_t.id / 0) "
                                   "JOIN extra_t ON extra_t.id = left_t.id",
                                   count_column, 1, zero_count, 1, "staged on warning count rows");
    failures += expect_int(mylite_warning_count(database), 3, "staged on warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_division_by_zero,
                           "staged on warning code 0");
    failures += expect_int((int)mylite_warning_code(database, 1), mysql_warning_division_by_zero,
                           "staged on warning code 1");
    failures += expect_int((int)mylite_warning_code(database, 2), mysql_warning_division_by_zero,
                           "staged on warning code 2");
    failures +=
        expect_select_rows(database,
                           "SELECT COUNT(*) AS c FROM left_t AS a, "
                           "right_t AS r JOIN extra_t AS e ON e.id / 0",
                           count_column, 1, zero_count, 1, "comma-left on warning count rows");
    failures += expect_int(mylite_warning_count(database), 3, "comma-left on warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_division_by_zero,
                           "comma-left on warning code 0");
    failures += expect_int((int)mylite_warning_code(database, 1), mysql_warning_division_by_zero,
                           "comma-left on warning code 1");
    failures += expect_int((int)mylite_warning_code(database, 2), mysql_warning_division_by_zero,
                           "comma-left on warning code 2");

    failures += prepare_sql(database,
                            "SELECT l.id AS left_id, r.label "
                            "FROM left_t AS l JOIN right_t AS r ON l.id = r.left_id LIMIT 0",
                            MYLITE_OK, &stmt);
    failures += expect_column_metadata(stmt, metadata, 2, "inner join metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "inner join metadata done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += expect_prepare_error(database,
                                     "SELECT id FROM left_t JOIN right_t "
                                     "ON left_t.id = right_t.left_id",
                                     MYLITE_EXEC_ERROR, "Column 'id' in field list is ambiguous",
                                     "inner join ambiguous column");
    failures += expect_int(mylite_warning_count(database), 1, "inner join ambiguous warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_ambiguous_column,
                           "inner join ambiguous warning code");
    failures +=
        expect_prepare_error(database,
                             "SELECT left_t.id FROM left_t AS l JOIN right_t AS r "
                             "ON l.id = r.left_id",
                             MYLITE_EXEC_ERROR, "Unknown column 'left_t.id' in 'field list'",
                             "inner join alias hides base table");
    failures += expect_int(mylite_warning_count(database), 1, "inner join alias warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown_column,
                           "inner join alias warning code");
    failures += expect_prepare_error(database,
                                     "SELECT l.id FROM left_t AS l JOIN right_t AS r "
                                     "ON left_t.id = r.left_id",
                                     MYLITE_EXEC_ERROR, "Unknown column 'left_t.id' in 'on clause'",
                                     "inner join alias hides base table in on");
    failures += expect_int(mylite_warning_count(database), 1, "inner join on alias warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown_column,
                           "inner join on alias warning code");
    failures +=
        expect_prepare_error(database,
                             "SELECT l.id FROM left_t AS l JOIN right_t AS r "
                             "ON l.id = r.left_id WHERE left_t.id = 1",
                             MYLITE_EXEC_ERROR, "Unknown column 'left_t.id' in 'where clause'",
                             "inner join alias hides base table in where");
    failures +=
        expect_int(mylite_warning_count(database), 1, "inner join where alias warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown_column,
                           "inner join where alias warning code");
    failures +=
        expect_prepare_error(database,
                             "SELECT l.id FROM left_t AS l JOIN right_t AS r "
                             "ON l.id = r.left_id ORDER BY left_t.id",
                             MYLITE_EXEC_ERROR, "Unknown column 'left_t.id' in 'order clause'",
                             "inner join alias hides base table in order");
    failures +=
        expect_int(mylite_warning_count(database), 1, "inner join order alias warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown_column,
                           "inner join order alias warning code");
    failures += expect_prepare_error(database, "SELECT * FROM left_t AS same JOIN right_t AS same",
                                     MYLITE_EXEC_ERROR, "Not unique table/alias: 'same'",
                                     "inner join duplicate alias");
    failures += expect_int(mylite_warning_count(database), 1, "inner join duplicate warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_nonunique_table,
                           "inner join duplicate warning code");
    failures += expect_prepare_error(
        database, "SELECT * FROM left_t JOIN right_t USING (missing_col)", MYLITE_EXEC_ERROR,
        "Unknown column 'missing_col' in 'from clause'", "inner join using missing column");
    failures += expect_int(mylite_warning_count(database), 1, "inner join using warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown_column,
                           "inner join using warning code");
    failures +=
        expect_prepare_error(database,
                             "SELECT l.id FROM left_t AS l JOIN right_t AS r "
                             "ON missing_col = r.left_id",
                             MYLITE_EXEC_ERROR, "Unknown column 'missing_col' in 'on clause'",
                             "inner join on missing column");
    failures += expect_int(mylite_warning_count(database), 1, "inner join on warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown_column,
                           "inner join on warning code");
    failures += expect_prepare_error(database,
                                     "SELECT l.id FROM left_t AS l, right_t AS r "
                                     "JOIN left_t AS p ON l.id = p.id",
                                     MYLITE_EXEC_ERROR, "Unknown column 'l.id' in 'on clause'",
                                     "inner join comma precedence on scope");
    failures +=
        expect_int(mylite_warning_count(database), 1, "inner join precedence warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown_column,
                           "inner join precedence warning code");
    failures += expect_prepare_error(database,
                                     "SELECT l.id FROM left_t AS l JOIN right_t AS r "
                                     "ON l.id = r.left_id WHERE id = 1",
                                     MYLITE_EXEC_ERROR, "Column 'id' in where clause is ambiguous",
                                     "inner join ambiguous where");
    failures +=
        expect_int(mylite_warning_count(database), 1, "inner join ambiguous where warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_ambiguous_column,
                           "inner join ambiguous where warning code");
    failures += expect_prepare_error(database,
                                     "SELECT l.label FROM left_t AS l JOIN right_t AS r "
                                     "ON l.id = r.left_id ORDER BY id",
                                     MYLITE_EXEC_ERROR, "Column 'id' in order clause is ambiguous",
                                     "inner join ambiguous order");
    failures +=
        expect_int(mylite_warning_count(database), 1, "inner join ambiguous order warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_ambiguous_column,
                           "inner join ambiguous order warning code");
    failures +=
        expect_prepare_error(database,
                             "SELECT left_t.id, COUNT(*) "
                             "FROM left_t JOIN right_t ON left_t.id = right_t.left_id "
                             "GROUP BY left_t.id",
                             MYLITE_EXEC_ERROR, "Unsupported GROUP BY or HAVING over joined tables",
                             "inner join grouped query deferred");

    mylite_close(database);
    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_outer_join_execution(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    static const char *const outer_columns[] = {"left_id", "right_id", "label"};
    static const char *const left_join_values[] = {"1", "10", "right-a", "2", "20", "right-b",
                                                   "2", "21", "right-c", "3", NULL, NULL};
    static const char *const right_columns[] = {"left_id", "nullable", "right_id"};
    static const char *const right_join_values[] = {"1",  NULL, "10", "2",  "5",  "20", "2", "5",
                                                    "21", NULL, NULL, "40", NULL, NULL, "50"};
    static const char *const id_pair_columns[] = {"left_id", "right_id"};
    static const char *const left_on_filter_values[] = {"1", "10", "2", NULL, "3", NULL};
    static const char *const left_where_filter_values[] = {"1", "10"};
    static const char *const right_on_filter_values[] = {"1",  "10", NULL, "20", NULL,
                                                         "21", NULL, "40", NULL, "50"};
    static const char *const right_where_filter_values[] = {"1", "10"};
    static const char *const using_columns[] = {"shared", "left_shared", "right_shared", "left_id",
                                                "right_id"};
    static const char *const left_using_values[] = {"100", "100", "100", "1", "10",
                                                    "200", "200", "200", "2", "21",
                                                    "300", "300", NULL,  "3", NULL};
    static const char *const right_using_values[] = {
        "100", "100", "100", "1",  "10",  "999", NULL, "999", NULL, "20", "200", "200", "200",
        "2",   "21",  "400", NULL, "400", NULL,  "40", NULL,  NULL, NULL, NULL,  "50"};
    static const char *const left_using_star_columns[] = {"shared", "id",      "label", "nullable",
                                                          "id",     "left_id", "label", "flag"};
    static const char *const right_using_star_columns[] = {"shared", "id", "left_id", "label",
                                                           "flag",   "id", "label",   "nullable"};
    static const char *const right_using_multi_star_columns[] = {
        "id", "shared", "left_id", "label", "flag", "label", "nullable"};
    static const char *const count_column[] = {"c"};
    static const char *const duplicate_left_using_count[] = {"3"};
    static const char *const left_outer_count[] = {"4"};
    static const char *const right_outer_count[] = {"5"};
    static const char *const warning_count[] = {"2"};
    static const struct expected_result_metadata left_metadata[] = {
        {"lid", "mylite_outer_join", "l", "mylite_outer_join", "left_t", "id", 11U,
         MYLITE_FIELD_TYPE_LONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_PRI_KEY | MYLITE_FIELD_FLAG_PART_KEY |
             MYLITE_FIELD_FLAG_NUM,
         0U, 0},
        {"rid", "mylite_outer_join", "r", "mylite_outer_join", "right_t", "id", 11U,
         MYLITE_FIELD_TYPE_LONG, 0U, 63U,
         MYLITE_FIELD_FLAG_PRI_KEY | MYLITE_FIELD_FLAG_PART_KEY | MYLITE_FIELD_FLAG_NUM,
         MYLITE_FIELD_FLAG_NOT_NULL, 1},
    };
    static const struct expected_result_metadata right_metadata[] = {
        {"lid", "mylite_outer_join", "l", "mylite_outer_join", "left_t", "id", 11U,
         MYLITE_FIELD_TYPE_LONG, 0U, 63U,
         MYLITE_FIELD_FLAG_PRI_KEY | MYLITE_FIELD_FLAG_PART_KEY | MYLITE_FIELD_FLAG_NUM,
         MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"rid", "mylite_outer_join", "r", "mylite_outer_join", "right_t", "id", 11U,
         MYLITE_FIELD_TYPE_LONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_PRI_KEY | MYLITE_FIELD_FLAG_PART_KEY |
             MYLITE_FIELD_FLAG_NUM,
         0U, 0},
    };
    static const struct expected_result_metadata right_using_metadata[] = {
        {"shared", "mylite_outer_join", "r", "mylite_outer_join", "right_t", "shared", 11U,
         MYLITE_FIELD_TYPE_LONG, 0U, 63U, MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
    };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open outer database");
    failures += execute_sql(database, "CREATE DATABASE mylite_outer_join", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_outer_join", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE left_t ("
                            "id INT PRIMARY KEY, shared INT, label VARCHAR(20) NOT NULL, "
                            "nullable INT)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE right_t ("
                            "id INT PRIMARY KEY, left_id INT, shared INT, "
                            "label VARCHAR(20) NOT NULL, flag INT)",
                            MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE warn_l (id INT PRIMARY KEY)", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE warn_r (s VARCHAR(10))", MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO left_t VALUES "
                            "(1,100,'left-one',NULL),"
                            "(2,200,'left-two',5),"
                            "(3,300,'left-three',NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO right_t VALUES "
                            "(10,1,100,'right-a',NULL),"
                            "(20,2,999,'right-b',5),"
                            "(21,2,200,'right-c',NULL),"
                            "(40,4,400,'right-orphan',5),"
                            "(50,NULL,NULL,'right-null',NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO warn_l VALUES (1),(2)", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO warn_r VALUES ('bad'),('2x')", MYLITE_DONE);

    failures += expect_select_rows(database,
                                   "SELECT l.id AS left_id, r.id AS right_id, r.label "
                                   "FROM left_t AS l LEFT JOIN right_t AS r "
                                   "ON l.id = r.left_id ORDER BY l.id, r.id",
                                   outer_columns, 3, left_join_values, 4, "left join rows");
    failures += expect_select_rows(database,
                                   "SELECT l.id AS left_id, l.nullable, r.id AS right_id "
                                   "FROM left_t AS l RIGHT JOIN right_t AS r "
                                   "ON l.id = r.left_id ORDER BY r.id, l.id",
                                   right_columns, 3, right_join_values, 5, "right join rows");
    failures += expect_select_rows(database,
                                   "SELECT COUNT(*) AS c FROM left_t LEFT OUTER JOIN right_t "
                                   "ON left_t.id = right_t.left_id",
                                   count_column, 1, left_outer_count, 1, "left outer count");
    failures += expect_select_rows(database,
                                   "SELECT COUNT(*) AS c FROM left_t RIGHT OUTER JOIN right_t "
                                   "ON left_t.id = right_t.left_id",
                                   count_column, 1, right_outer_count, 1, "right outer count");
    failures +=
        expect_select_rows(database,
                           "SELECT l.id AS left_id, r.id AS right_id "
                           "FROM left_t AS l LEFT JOIN right_t AS r "
                           "ON l.id = r.left_id AND r.id < 20 ORDER BY l.id, r.id",
                           id_pair_columns, 2, left_on_filter_values, 3, "left join on filter");
    failures += expect_select_rows(database,
                                   "SELECT l.id AS left_id, r.id AS right_id "
                                   "FROM left_t AS l LEFT JOIN right_t AS r "
                                   "ON l.id = r.left_id WHERE r.id < 20 ORDER BY l.id, r.id",
                                   id_pair_columns, 2, left_where_filter_values, 1,
                                   "left join where filter");
    failures +=
        expect_select_rows(database,
                           "SELECT l.id AS left_id, r.id AS right_id "
                           "FROM left_t AS l RIGHT JOIN right_t AS r "
                           "ON l.id = r.left_id AND l.id < 2 ORDER BY r.id, l.id",
                           id_pair_columns, 2, right_on_filter_values, 5, "right join on filter");
    failures += expect_select_rows(database,
                                   "SELECT l.id AS left_id, r.id AS right_id "
                                   "FROM left_t AS l RIGHT JOIN right_t AS r "
                                   "ON l.id = r.left_id WHERE l.id < 2 ORDER BY r.id, l.id",
                                   id_pair_columns, 2, right_where_filter_values, 1,
                                   "right join where filter");
    failures += expect_select_rows(database,
                                   "SELECT shared, l.shared AS left_shared, "
                                   "r.shared AS right_shared, l.id AS left_id, r.id AS right_id "
                                   "FROM left_t AS l LEFT JOIN right_t AS r USING (shared) "
                                   "ORDER BY l.id, r.id",
                                   using_columns, 5, left_using_values, 3, "left join using rows");
    failures +=
        expect_select_rows(database,
                           "SELECT shared, l.shared AS left_shared, "
                           "r.shared AS right_shared, l.id AS left_id, r.id AS right_id "
                           "FROM left_t AS l RIGHT JOIN right_t AS r USING (shared) "
                           "ORDER BY r.id",
                           using_columns, 5, right_using_values, 5, "right join using rows");
    failures += expect_select_rows(database,
                                   "SELECT COUNT(*) AS c FROM left_t LEFT JOIN right_t "
                                   "USING (shared, shared)",
                                   count_column, 1, duplicate_left_using_count, 1,
                                   "left join duplicate using count");
    failures += expect_select_rows(database,
                                   "SELECT COUNT(*) AS c FROM left_t RIGHT JOIN right_t "
                                   "USING (shared, shared)",
                                   count_column, 1, right_outer_count, 1,
                                   "right join duplicate using count");
    failures +=
        expect_select_rows(database,
                           "SELECT * FROM left_t LEFT JOIN right_t "
                           "USING (shared) LIMIT 0",
                           left_using_star_columns, 8, NULL, 0, "left join using wildcard columns");
    failures += expect_select_rows(database,
                                   "SELECT * FROM left_t RIGHT JOIN right_t "
                                   "USING (shared) LIMIT 0",
                                   right_using_star_columns, 8, NULL, 0,
                                   "right join using wildcard columns");
    failures += expect_select_rows(database,
                                   "SELECT * FROM left_t RIGHT JOIN right_t "
                                   "USING (shared, id) LIMIT 0",
                                   right_using_multi_star_columns, 7, NULL, 0,
                                   "right join multi using wildcard columns");
    failures += expect_select_rows(
        database, "SELECT COUNT(*) AS c FROM warn_l LEFT JOIN warn_r ON warn_r.s = 1", count_column,
        1, warning_count, 1, "left join warning count rows");
    failures += expect_int(mylite_warning_count(database), 2, "left join warning count");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_truncated_wrong_value, "left join warning code 0");
    failures += expect_int((int)mylite_warning_code(database, 1),
                           mysql_warning_truncated_wrong_value, "left join warning code 1");

    failures += prepare_sql(database,
                            "SELECT l.id AS lid, r.id AS rid "
                            "FROM left_t AS l LEFT JOIN right_t AS r "
                            "ON l.id = r.left_id LIMIT 0",
                            MYLITE_OK, &stmt);
    failures += expect_result_metadata(stmt, left_metadata, 2, "left join nullable metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "left join metadata done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "SELECT l.id AS lid, r.id AS rid "
                            "FROM left_t AS l RIGHT JOIN right_t AS r "
                            "ON l.id = r.left_id LIMIT 0",
                            MYLITE_OK, &stmt);
    failures += expect_result_metadata(stmt, right_metadata, 2, "right join nullable metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "right join metadata done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "SELECT shared FROM left_t AS l RIGHT JOIN right_t AS r "
                            "USING (shared) LIMIT 0",
                            MYLITE_OK, &stmt);
    failures += expect_result_metadata(stmt, right_using_metadata, 1, "right join using metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "right join using metadata done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "SELECT * FROM left_t LEFT JOIN right_t", MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "left join missing condition parse error");
    failures += expect_prepare_error(database,
                                     "SELECT id FROM left_t LEFT JOIN right_t "
                                     "ON left_t.id = right_t.left_id",
                                     MYLITE_EXEC_ERROR, "Column 'id' in field list is ambiguous",
                                     "left join ambiguous field");
    failures += expect_prepare_error(database,
                                     "SELECT l.id FROM left_t AS l LEFT JOIN right_t AS r "
                                     "ON left_t.id = r.left_id",
                                     MYLITE_EXEC_ERROR, "Unknown column 'left_t.id' in 'on clause'",
                                     "left join alias hides base table in on");
    failures += expect_prepare_error(database,
                                     "SELECT * FROM left_t AS same LEFT JOIN right_t AS same "
                                     "ON same.id = same.left_id",
                                     MYLITE_EXEC_ERROR, "Not unique table/alias: 'same'",
                                     "left join duplicate alias");
    failures +=
        expect_prepare_error(database,
                             "SELECT * FROM left_t LEFT JOIN right_t "
                             "USING (missing_col)",
                             MYLITE_EXEC_ERROR, "Unknown column 'missing_col' in 'from clause'",
                             "left join missing using column");
    failures += expect_prepare_error(database,
                                     "SELECT l.id FROM left_t AS l, right_t AS r "
                                     "LEFT JOIN left_t AS p ON l.id = p.id",
                                     MYLITE_EXEC_ERROR, "Unknown column 'l.id' in 'on clause'",
                                     "left join comma precedence on scope");
    failures +=
        expect_prepare_error(database,
                             "SELECT left_t.id, COUNT(*) "
                             "FROM left_t LEFT JOIN right_t ON left_t.id = right_t.left_id "
                             "GROUP BY left_t.id",
                             MYLITE_EXEC_ERROR, "Unsupported GROUP BY or HAVING over joined tables",
                             "left join grouped query deferred");

    mylite_close(database);
    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_select_distinct_execution(void)
{
    enum {
        distinct_a_row_count = 5,
        distinct_a_b_row_count = 5,
        grouped_row_count = 5,
        wildcard_column_count = 5,
    };
    static const char *const one_column[] = {"one"};
    static const char *const one_value[] = {"1"};
    static const char *const a_column[] = {"a"};
    static const char *const alias_b_column[] = {"b"};
    static const char *const distinct_a_values[] = {"1", "2", "3", "4", NULL};
    static const char *const distinct_a_null_first_values[] = {NULL, "1", "2", "3", "4"};
    static const char *const distinct_a_desc_limit_values[] = {"4", "3", "2"};
    static const char *const all_a_values[] = {"1", "1", "2"};
    static const char *const a_b_columns[] = {"a", "b"};
    static const char *const distinct_a_b_values[] = {
        "1", "x", "2", "y", "3", "A", "4", NULL, NULL, "n",
    };
    static const char *const b_column[] = {"b"};
    static const char *const b_ai_values[] = {"A"};
    static const char *const c_column[] = {"c"};
    static const char *const c_bin_values[] = {"A", "a"};
    static const char *const plus_one_column[] = {"plus_one"};
    static const char *const plus_one_values[] = {"2", "3", "4", "5"};
    static const char *const distinct_limit_values[] = {NULL, "1", "2"};
    static const char *const join_columns[] = {"a", "tag"};
    static const char *const join_values[] = {
        NULL, "nil", "1", "one", "1", "two", "2", "two",
    };
    static const char *const count_column[] = {"n"};
    static const char *const count_value[] = {"9"};
    static const char *const grouped_columns[] = {"a", "n"};
    static const char *const grouped_values[] = {
        "1", "2", "2", "1", "3", "2", "4", "2", NULL, "2",
    };
    static const char *const grouped_count_values[] = {"1", "2"};
    static const char *const wildcard_columns[] = {"id", "a", "b", "c", "sort_key"};
    static const char *const wildcard_values[] = {
        "1", "1", "x", "x", "30", "2", "1", "x", "x", "10", "3", "2", "y", "y", "20",
    };
    static const struct expected_result_metadata metadata[] = {
        {"alias_a", "mylite_task28_distinct", "d", "mylite_task28_distinct", "d", "a", 11U,
         MYLITE_FIELD_TYPE_LONG, 0U, 63U, MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"label_b", "mylite_task28_distinct", "d", "mylite_task28_distinct", "d", "b", 40U,
         MYLITE_FIELD_TYPE_VAR_STRING, 0U, 255U, 0U, MYLITE_FIELD_FLAG_BINARY, 1},
        {"plus_one", NULL, NULL, NULL, NULL, NULL, 12U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
    };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open distinct database");
    failures += execute_sql(database,
                            "CREATE DATABASE mylite_task28_distinct DEFAULT CHARACTER SET utf8mb4 "
                            "COLLATE utf8mb4_0900_ai_ci",
                            MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_task28_distinct", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE d ("
                            "id INT PRIMARY KEY AUTO_INCREMENT, "
                            "a INT NULL, "
                            "b VARCHAR(10) COLLATE utf8mb4_0900_ai_ci NULL, "
                            "c VARCHAR(10) COLLATE utf8mb4_bin NULL, "
                            "sort_key INT NOT NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO d (a,b,c,sort_key) VALUES "
                            "(1,'x','x',30), "
                            "(1,'x','x',10), "
                            "(2,'y','y',20), "
                            "(NULL,'n','n',40), "
                            "(NULL,'n','n',50), "
                            "(3,'A','A',60), "
                            "(3,'a','a',70), "
                            "(4,NULL,NULL,80), "
                            "(4,NULL,NULL,90)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE e ("
                            "id INT PRIMARY KEY AUTO_INCREMENT, "
                            "d_a INT NULL, "
                            "tag VARCHAR(10))",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO e (d_a, tag) VALUES "
                            "(1,'one'), (1,'one'), (1,'two'), (2,'two'), (NULL,'nil')",
                            MYLITE_DONE);

    failures += expect_select_rows(database, "SELECT DISTINCT 1 AS one", one_column, 1, one_value,
                                   1, "scalar distinct row");
    failures +=
        expect_select_rows(database, "SELECT DISTINCT a FROM d ORDER BY a IS NULL, a", a_column, 1,
                           distinct_a_values, distinct_a_row_count, "distinct single column rows");
    failures += expect_select_rows(database, "SELECT DISTINCT a FROM d ORDER BY a + 0", a_column, 1,
                                   distinct_a_null_first_values, distinct_a_row_count,
                                   "distinct selected expression order");
    failures += expect_select_rows(database, "SELECT DISTINCT a AS b FROM d ORDER BY (b)",
                                   alias_b_column, 1, distinct_a_null_first_values,
                                   distinct_a_row_count, "distinct parenthesized alias order");
    failures += expect_select_rows(database, "SELECT DISTINCTROW a FROM d ORDER BY a IS NULL, a",
                                   a_column, 1, distinct_a_values, distinct_a_row_count,
                                   "distinctrow single column rows");
    failures += expect_select_rows(database, "SELECT ALL a FROM d ORDER BY id LIMIT 3", a_column, 1,
                                   all_a_values, 3, "explicit all rows");
    failures += expect_select_rows(
        database, "SELECT DISTINCT DISTINCT a FROM d ORDER BY a IS NULL, a", a_column, 1,
        distinct_a_values, distinct_a_row_count, "repeated distinct rows");
    failures += expect_select_rows(database, "SELECT ALL ALL a FROM d ORDER BY id LIMIT 3",
                                   a_column, 1, all_a_values, 3, "repeated all rows");
    failures += expect_select_rows(database,
                                   "SELECT DISTINCT a,b FROM d "
                                   "ORDER BY a IS NULL, a, b IS NULL, b",
                                   a_b_columns, 2, distinct_a_b_values, distinct_a_b_row_count,
                                   "distinct multi-column rows");
    failures +=
        expect_select_rows(database, "SELECT DISTINCT b FROM d WHERE a=3 ORDER BY b", b_column, 1,
                           b_ai_values, 1, "case-insensitive distinct collation");
    failures += expect_select_rows(database, "SELECT DISTINCT c FROM d WHERE a=3 ORDER BY c",
                                   c_column, 1, c_bin_values, 2, "binary distinct collation");
    failures += expect_select_rows(
        database, "SELECT DISTINCT a + 1 AS plus_one FROM d WHERE a IS NOT NULL ORDER BY plus_one",
        plus_one_column, 1, plus_one_values, 4, "distinct expression output");
    failures += expect_select_rows(database, "SELECT DISTINCT a FROM d ORDER BY a LIMIT 3",
                                   a_column, 1, distinct_limit_values, 3, "distinct ordered limit");
    failures +=
        expect_select_rows(database, "SELECT DISTINCT a FROM d ORDER BY a DESC LIMIT 3", a_column,
                           1, distinct_a_desc_limit_values, 3, "distinct descending limit");
    failures += expect_select_rows(
        database, "SELECT DISTINCT d.a, e.tag FROM d JOIN e ON d.a <=> e.d_a ORDER BY d.a, e.tag",
        join_columns, 2, join_values, 4, "distinct join rows");
    failures += expect_select_rows(
        database,
        "SELECT DISTINCT d.a FROM d LEFT JOIN e ON d.a <=> e.d_a ORDER BY d.a IS NULL, d.a",
        a_column, 1, distinct_a_values, distinct_a_row_count, "distinct outer join rows");
    failures += expect_select_rows(database, "SELECT DISTINCT COUNT(*) AS n FROM d", count_column,
                                   1, count_value, 1, "distinct aggregate count");
    failures += expect_select_rows(
        database, "SELECT DISTINCT COUNT(*) AS n FROM d GROUP BY b ORDER BY n", count_column, 1,
        grouped_count_values, 2, "distinct collapsed aggregate grouping");
    failures += expect_select_rows(
        database, "SELECT DISTINCT a, COUNT(*) AS n FROM d GROUP BY a ORDER BY a IS NULL, a",
        grouped_columns, 2, grouped_values, grouped_row_count, "distinct aggregate grouping");
    failures += expect_select_rows(
        database, "SELECT DISTINCT * FROM d WHERE a IN (1,2) ORDER BY id", wildcard_columns,
        wildcard_column_count, wildcard_values, 3, "distinct wildcard rows");
    failures += expect_select_rows(database,
                                   "SELECT DISTINCT d.* FROM d JOIN e ON d.a <=> e.d_a "
                                   "WHERE d.a IN (1,2) ORDER BY d.id",
                                   wildcard_columns, wildcard_column_count, wildcard_values, 3,
                                   "distinct qualified wildcard join rows");

    failures += prepare_sql(database,
                            "SELECT DISTINCT a AS alias_a, b AS label_b, a + 1 AS plus_one "
                            "FROM d ORDER BY alias_a IS NULL, alias_a, label_b LIMIT 0",
                            MYLITE_OK, &stmt);
    failures += expect_result_metadata(stmt, metadata, 3, "distinct metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "distinct metadata done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        expect_prepare_error(database, "SELECT ALL DISTINCT a FROM d", MYLITE_EXEC_ERROR,
                             "Incorrect usage of ALL and DISTINCT", "mixed all distinct mode");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_wrong_usage,
                           "mixed all distinct warning code");
    failures +=
        expect_prepare_error(database, "SELECT DISTINCT ALL a FROM d", MYLITE_EXEC_ERROR,
                             "Incorrect usage of ALL and DISTINCT", "mixed distinct all mode");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_wrong_usage,
                           "mixed distinct all warning code");
    failures +=
        expect_prepare_error(database, "SELECT ALL DISTINCTROW a FROM d", MYLITE_EXEC_ERROR,
                             "Incorrect usage of ALL and DISTINCT", "mixed all distinctrow mode");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_wrong_usage,
                           "mixed all distinctrow warning code");
    failures += expect_prepare_error(
        database, "SELECT DISTINCT a FROM d ORDER BY sort_key", MYLITE_EXEC_ERROR,
        "Expression #1 of ORDER BY clause is not in SELECT list", "distinct hidden order column");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_field_in_order_not_select,
                   "distinct hidden order warning code");
    failures += expect_prepare_error(database, "SELECT DISTINCT a FROM d ORDER BY sort_key + 0",
                                     MYLITE_EXEC_ERROR,
                                     "Expression #1 of ORDER BY clause is not in SELECT list",
                                     "distinct hidden order expression");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_field_in_order_not_select,
                   "distinct hidden order expression warning code");
    failures += expect_prepare_error(database, "SELECT DISTINCT a AS b FROM d ORDER BY b + 0",
                                     MYLITE_EXEC_ERROR,
                                     "Expression #1 of ORDER BY clause is not in SELECT list",
                                     "distinct alias shadowed order expression");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_field_in_order_not_select,
                   "distinct alias shadowed order warning code");

    mylite_close(database);
    return failures;
}

static int test_union_query_expression_execution(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    static const char *const v_column[] = {"v"};
    static const char *const distinct_null_values[] = {"1", NULL};
    static const char *const one_two_values[] = {"1", "2"};
    static const char *const one_one_values[] = {"1", "1"};
    static const char *const one_value[] = {"1"};
    static const char *const two_value[] = {"2"};
    static const char *const two_decimal_value[] = {"2.0000"};
    static const char *const n_column[] = {"n"};
    static const char *const local_order_values[] = {NULL, "2"};
    static const char *const global_limit_values[] = {"2", "2"};
    static const char *const first_name_column[] = {"first_name"};
    static const char *const first_name_values[] = {"3", "2"};
    static const char *const s_column[] = {"s"};
    static const char *const s_values[] = {"10", "bad"};
    static const char *const bin_column[] = {"bin_s"};
    static const char *const bin_values[] = {"A", "a"};
    static const struct expected_result_metadata metadata[] = {
        {"alias_n", NULL, NULL, NULL, NULL, NULL, 11U, MYLITE_FIELD_TYPE_LONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"label_s", NULL, NULL, NULL, NULL, NULL, 40U, MYLITE_FIELD_TYPE_VAR_STRING, 31U, 255U, 0U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY, 1},
    };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open union database");
    failures += execute_sql(database,
                            "CREATE DATABASE mylite_union_query DEFAULT CHARACTER SET utf8mb4 "
                            "COLLATE utf8mb4_0900_ai_ci",
                            MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_union_query", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE left_t ("
                            "id INT PRIMARY KEY, "
                            "n INT NULL, "
                            "s VARCHAR(10) COLLATE utf8mb4_0900_ai_ci NULL, "
                            "bin_s VARCHAR(10) COLLATE utf8mb4_bin NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE right_t ("
                            "id INT PRIMARY KEY, "
                            "n INT NULL, "
                            "s VARCHAR(10) COLLATE utf8mb4_0900_ai_ci NULL, "
                            "bin_s VARCHAR(10) COLLATE utf8mb4_bin NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO left_t VALUES "
                            "(1,1,'10','A'), "
                            "(2,2,'bad','a'), "
                            "(3,NULL,'30',NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO right_t VALUES "
                            "(10,2,'BAD','A'), "
                            "(11,3,'7','a'), "
                            "(12,NULL,'40',NULL)",
                            MYLITE_DONE);

    failures += expect_select_rows(database,
                                   "SELECT 1 AS v UNION SELECT 1 UNION SELECT NULL UNION "
                                   "SELECT NULL ORDER BY v IS NULL, v",
                                   v_column, 1, distinct_null_values, 2,
                                   "union default distinct null collapsing");
    failures += expect_select_rows(database, "SELECT 1 AS v UNION ALL SELECT 1 ORDER BY v",
                                   v_column, 1, one_one_values, 2, "union all retains duplicate");
    failures +=
        expect_select_rows(database, "SELECT 1 AS v UNION DISTINCT SELECT 1 ORDER BY v", v_column,
                           1, one_value, 1, "union explicit distinct removes duplicate");
    failures += expect_select_rows(
        database, "SELECT 1 AS v UNION ALL SELECT 1 UNION DISTINCT SELECT 1 ORDER BY v", v_column,
        1, one_value, 1, "union all then distinct left association");
    failures += expect_select_rows(
        database, "SELECT 1 AS v UNION DISTINCT SELECT 1 UNION ALL SELECT 1 ORDER BY v", v_column,
        1, one_one_values, 2, "union distinct then all left association");
    failures +=
        expect_select_rows(database, "(SELECT 1 AS v LIMIT 0) UNION ALL SELECT 2 ORDER BY v",
                           v_column, 1, two_value, 1, "parenthesized scalar operand local limit");
    failures += expect_select_rows(
        database, "(SELECT 1/0 AS v LIMIT 0) UNION ALL SELECT 2/1 ORDER BY v", v_column, 1,
        two_decimal_value, 1, "parenthesized empty operand skips warnings");
    failures +=
        expect_int(mylite_warning_count(database), 0, "parenthesized empty operand warning count");
    failures += expect_select_rows(
        database, "(SELECT 2 AS v ORDER BY v LIMIT 1) UNION ALL SELECT 1 ORDER BY v", v_column, 1,
        one_two_values, 2, "parenthesized scalar operand local order");

    failures += expect_select_rows(database,
                                   "(SELECT n FROM left_t ORDER BY n DESC LIMIT 1) UNION ALL "
                                   "(SELECT n FROM right_t ORDER BY n LIMIT 1) ORDER BY n",
                                   n_column, 1, local_order_values, 2,
                                   "parenthesized table operands local order limit");
    failures +=
        expect_select_rows(database,
                           "SELECT n FROM left_t UNION ALL SELECT n FROM right_t "
                           "ORDER BY n DESC LIMIT 2 OFFSET 1",
                           n_column, 1, global_limit_values, 2, "union global order limit offset");
    failures += expect_select_rows(database,
                                   "SELECT n AS first_name FROM left_t WHERE id IN (1,2) UNION ALL "
                                   "SELECT n AS later_name FROM right_t WHERE id IN (10,11) "
                                   "ORDER BY first_name DESC LIMIT 2",
                                   first_name_column, 1, first_name_values, 2,
                                   "union first operand output label");
    failures += expect_select_rows(database,
                                   "SELECT s FROM left_t WHERE id IN (1,2) UNION "
                                   "SELECT s FROM right_t WHERE id = 10 ORDER BY s",
                                   s_column, 1, s_values, 2,
                                   "union case-insensitive collation distinct rows");
    failures +=
        expect_select_rows(database,
                           "SELECT bin_s FROM left_t WHERE id IN (1,2) UNION "
                           "SELECT bin_s FROM right_t WHERE id IN (10,11) ORDER BY bin_s",
                           bin_column, 1, bin_values, 2, "union binary collation distinct rows");

    failures += prepare_sql(database,
                            "SELECT n AS alias_n, s AS label_s FROM left_t UNION ALL "
                            "SELECT n, s FROM right_t LIMIT 0",
                            MYLITE_OK, &stmt);
    failures += expect_result_metadata(stmt, metadata, 2, "union result metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "union metadata done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += expect_select_rows(database,
                                   "SELECT s + 0 AS v FROM left_t WHERE id = 2 UNION ALL "
                                   "SELECT s + 0 FROM right_t WHERE id = 10 ORDER BY v",
                                   v_column, 1, (const char *[]){"0.0000", "0.0000"}, 2,
                                   "union operand warnings rows");
    failures += expect_int(mylite_warning_count(database), 2, "union operand warning count");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_truncated_wrong_value, "union operand warning code");

    failures += expect_prepare_error(database, "SELECT 1 UNION SELECT 1, 2", MYLITE_EXEC_ERROR,
                                     "different number of columns", "union column count error");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_wrong_number_of_columns, "union column count warning");
    failures += expect_prepare_error(
        database, "SELECT n FROM left_t UNION ALL SELECT n FROM right_t ORDER BY left_t.n",
        MYLITE_EXEC_ERROR, "cannot be used in global ORDER clause",
        "union table-qualified global order");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_table_name_not_allowed, "union table order warning");
    failures += expect_prepare_error(
        database, "(SELECT 1 AS v ORDER BY missing LIMIT 0) UNION ALL SELECT 2", MYLITE_EXEC_ERROR,
        "Unknown column 'missing' in 'order clause'", "union scalar local order unknown column");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown_column,
                           "union scalar local order warning");
    failures +=
        expect_prepare_error(database, "SELECT 1 AS v UNION SELECT 2 ORDER BY 2", MYLITE_EXEC_ERROR,
                             "Unknown column '2' in 'order clause'", "union ordinal out of range");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown_column,
                           "union ordinal warning");
    failures += expect_prepare_error(database, "SELECT 1 AS x, 2 AS x UNION SELECT 3, 4 ORDER BY x",
                                     MYLITE_EXEC_ERROR, "Column 'x' in order clause is ambiguous",
                                     "union ambiguous output label");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_ambiguous_column,
                           "union ambiguous warning");
    failures += expect_prepare_error(
        database,
        "SELECT n AS first_name FROM left_t UNION ALL SELECT n AS later_name FROM right_t "
        "ORDER BY later_name",
        MYLITE_EXEC_ERROR, "Unknown column 'later_name' in 'order clause'",
        "union later alias rejected");
    failures += expect_prepare_error(
        database, "SELECT n AS first_name FROM left_t UNION ALL SELECT n FROM right_t ORDER BY n",
        MYLITE_EXEC_ERROR, "Unknown column 'n' in 'order clause'",
        "union first alias hides source name");

    mylite_close(database);
    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_subquery_execution(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    static const char *const scalar_columns[] = {"one", "missing", "exists_one", "not_missing"};
    static const char *const scalar_values[] = {"1", NULL, "1", "1"};
    static const char *const first_val_column[] = {"first_val"};
    static const char *const first_val_value[] = {"10"};
    static const char *const id_txt_columns[] = {"id", "first_txt"};
    static const char *const id_txt_values[] = {"1", "alpha", "2", "alpha", "3", "alpha"};
    static const char *const id_column[] = {"id"};
    static const char *const all_ids[] = {"1", "2", "3"};
    static const char *const second_id[] = {"2"};
    static const char *const desc_ids[] = {"3", "2"};
    static const char *const join_columns[] = {"id", "tag"};
    static const char *const join_values[] = {"1", "one", "2", "two"};
    static const char *const count_column[] = {"c"};
    static const char *const count_value[] = {"3"};
    static const char *const exists_warning_columns[] = {
        "safe_exists", "safe_table_exists", "safe_distinct_exists", "safe_order_exists"};
    static const char *const exists_warning_values[] = {"1", "1", "1", "1"};
    static const char *const exists_limit_column[] = {"exists_limit_zero"};
    static const char *const exists_limit_value[] = {"0"};
    static const char *const scalar_warning_column[] = {"div_null"};
    static const char *const scalar_warning_value[] = {NULL};
    static const char *const in_truth_columns[] = {
        "match_in",     "miss_in_null", "miss_not_in_null", "empty_in",
        "empty_not_in", "null_in_base", "null_empty_in",    "null_empty_not_in"};
    static const char *const in_truth_values[] = {"1", NULL, NULL, "0", "1", NULL, "0", "1"};
    static const char *const in_projection_columns[] = {"id", "in_base", "not_in_base"};
    static const char *const in_projection_values[] = {
        "1", "1", "0", "2", "1", "0", "3", NULL, NULL, "4", NULL, NULL, "5", NULL, NULL,
    };
    static const char *const in_two_ids[] = {"1", "2"};
    static const char *const in_all_ids[] = {"1", "2", "3", "4", "5"};
    static const char *const in_join_columns[] = {"outer_id", "join_id"};
    static const char *const in_join_values[] = {"1", "201", "2", "202"};
    static const char *const in_having_columns[] = {"grp", "c"};
    static const char *const in_having_values[] = {"1", "2"};
    static const char *const in_order_ids[] = {"2", "1", "3", "4", "5"};
    static const char *const in_text_columns[] = {"text_in", "text_not_in"};
    static const char *const in_text_values[] = {"1", "1"};
    static const char *const in_inner_clause_columns[] = {"ordered_in", "distinct_in", "group_in"};
    static const char *const in_inner_clause_values[] = {"1", "1", "1"};
    static const char *const in_order_warning_column[] = {"ignored_order_warning"};
    static const char *const in_order_warning_value[] = {NULL};
    static const char *const in_warning_column[] = {"warn_in"};
    static const char *const in_warning_false[] = {"0"};
    static const char *const in_warning_true[] = {"1"};
    static const char *const in_warning_projection_values[] = {"0", "0", NULL, "0", "0"};
    static const char *const in_metadata_columns[] = {"in_result", "not_in_result", "no_table_in",
                                                      "numeric_text_in"};
    static const struct expected_result_metadata metadata[] = {
        {"sub_val", NULL, NULL, NULL, NULL, NULL, 11U, MYLITE_FIELD_TYPE_LONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"has_one", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
    };
    static const struct expected_result_metadata in_metadata[] = {
        {"in_result", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"not_in_result", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"no_table_in", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"numeric_text_in", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
    };
    static const char *const quantified_truth_columns[] = {
        "eq_any",         "ne_some",        "bang_ne_some", "lt_any",
        "le_all",         "ge_all",         "empty_any",    "empty_all",
        "null_empty_any", "null_empty_all", "gt_any_order", "gt_all_order",
        "lt_any_small",   "lt_all_small",   "eq_all_empty", "eq_any_empty"};
    static const char *const quantified_truth_values[] = {"1", "1", "1",  "1", "1",  "1", "0", "1",
                                                          "0", "1", NULL, "0", NULL, "0", "1", "0"};
    static const char *const quantified_projection_columns[] = {"id", "gt_any_base", "ge_all_base"};
    static const char *const quantified_projection_values[] = {
        "1", NULL, "0", "2", "1", NULL, "3", NULL, NULL, "4", NULL, "0", "5", "1", NULL,
    };
    static const char *const quantified_two_ids[] = {"2", "5"};
    static const char *const quantified_join_values[] = {"1", "201", "2", "202"};
    static const char *const quantified_having_values[] = {"1", "2"};
    static const char *const quantified_order_ids[] = {"1", "4", "5", "2", "3"};
    static const char *const quantified_inner_clause_columns[] = {
        "ordered_any", "distinct_any", "group_all", "order_warning_any", "order_warning_all"};
    static const char *const quantified_inner_clause_values[] = {NULL, "1", "1", NULL, "0"};
    static const char *const quantified_warning_column[] = {"warn_cmp"};
    static const char *const quantified_warning_false[] = {"0"};
    static const char *const quantified_warning_true[] = {"1"};
    static const char *const quantified_warning_projection_values[] = {"1", "1", NULL, "1", "1"};
    static const char *const quantified_metadata_columns[] = {"any_result", "all_result",
                                                              "some_result"};
    static const struct expected_result_metadata quantified_metadata[] = {
        {"any_result", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"all_result", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"some_result", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
    };
    static const char *const row_scalar_columns[] = {
        "eq_match",      "eq_miss",        "eq_null_tail", "eq_null_head", "ne_miss",
        "ne_null_tail",  "lt_same_head",   "lt_next_head", "lt_prev_head", "lt_null_tail",
        "lt_early_true", "lt_early_false", "row_eq",       "nse_match",    "nse_miss",
        "nse_null_tail", "nse_null_head",  "nse_all_null", "empty_eq",     "empty_nse"};
    static const char *const row_scalar_values[] = {
        "1", "0", NULL, NULL, "1", NULL, "1", "1", "0",  NULL,
        "1", "0", "1",  "1",  "0", "1",  "0", "1", NULL, "0",
    };
    static const char *const row_scalar_nse_columns[] = {"nse_head_null", "nse_middle_null",
                                                         "nse_tail_null", "nse_all_null"};
    static const char *const row_scalar_nse_values[] = {"0", "0", "1", "1"};
    static const char *const row_in_truth_columns[] = {
        "match_in",     "match_not_in",  "unknown_in",    "unknown_not_in",
        "left_null_in", "left_null2_in", "miss_in",       "miss_not_in",
        "empty_in",     "empty_not_in",  "null_empty_in", "null_empty_not_in"};
    static const char *const row_in_truth_values[] = {"1", "0", NULL, NULL, NULL, NULL,
                                                      "0", "1", "0",  "1",  "0",  "1"};
    static const char *const row_projection_columns[] = {"id", "row_in", "row_not_in"};
    static const char *const row_projection_values[] = {
        "1", "1", "0", "2", "1", "0", "3", NULL, NULL, "4", NULL, NULL, "5", "1", "0",
    };
    static const char *const row_match_ids[] = {"1", "2", "5"};
    static const char *const row_join_values[] = {"1", "301", "2", "302", "5", "305"};
    static const char *const row_having_values[] = {"1", "2", "2", "1", "3", "1"};
    static const char *const row_order_ids[] = {"1", "2", "5", "3", "4"};
    static const char *const row_inner_clause_columns[] = {"ordered_row_in", "distinct_row_in",
                                                           "group_row_in"};
    static const char *const row_inner_clause_values[] = {"1", "1", "1"};
    static const char *const row_order_warning_column[] = {"row_order_warning"};
    static const char *const row_warning_column[] = {"row_warn"};
    static const char *const row_warning_false[] = {"0"};
    static const char *const row_warning_null[] = {NULL};
    static const char *const row_warning_true[] = {"1"};
    static const char *const row_scalar_warning_column[] = {"row_scalar_warn"};
    static const char *const row_metadata_columns[] = {
        "row_in_result", "row_not_in_result", "row_eq_result", "row_nse_result", "no_table_row_in"};
    static const struct expected_result_metadata row_metadata[] = {
        {"row_in_result", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"row_not_in_result", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"row_eq_result", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"row_nse_result", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
        {"no_table_row_in", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
    };
    static const char *const row_quant_truth_columns[] = {
        "eq_any",      "eq_some",  "row_eq_any",     "ne_all",      "bang_ne_all", "row_ne_all",
        "unknown_any", "miss_any", "unknown_ne_all", "miss_ne_all", "empty_any",   "empty_ne_all"};
    static const char *const row_quant_truth_values[] = {
        "1", "1", "1", "0", "0", "0", NULL, "0", NULL, "1", "0", "1",
    };
    static const char *const row_quant_projection_columns[] = {"id", "row_eq_any", "row_ne_all"};
    static const char *const row_quant_projection_values[] = {
        "1", "1", "0", "2", "1", "0", "3", NULL, NULL, "4", NULL, NULL,
        "5", "1", "0", "6", "0", "1", "7", NULL, NULL, "8", NULL, NULL,
    };
    static const char *const row_quant_match_ids[] = {"1", "2", "5"};
    static const char *const row_quant_not_all_ids[] = {"6"};
    static const char *const row_quant_join_values[] = {"1", "201", "2", "202", "5", "205"};
    static const char *const row_quant_having_values[] = {"1", "2", "3", "1"};
    static const char *const row_quant_order_ids[] = {"1", "2", "5", "6", "3", "4", "7", "8"};
    static const char *const row_quant_no_table_columns[] = {"row_eq_any", "row_ne_all"};
    static const char *const row_quant_no_table_values[] = {"1", "0"};
    static const char *const row_quant_inner_clause_columns[] = {
        "ordered_row_any", "distinct_row_any", "group_row_any"};
    static const char *const row_quant_inner_clause_values[] = {"1", "1", "1"};
    static const char *const row_quant_order_warning_column[] = {"order_warning_row_any"};
    static const char *const row_quant_warning_column[] = {"row_quant_warn"};
    static const char *const row_quant_warning_false[] = {"0"};
    static const char *const row_quant_warning_null[] = {NULL};
    static const char *const row_quant_warning_true[] = {"1"};
    static const char *const row_quant_warning_projection_values[] = {
        "0", "0", NULL, "0", "0", "0", "0", "0",
    };
    static const char *const row_quant_metadata_columns[] = {"row_eq_any", "row_eq_some",
                                                             "row_ne_all", "row_bang_ne_all"};
    static const struct expected_result_metadata row_quant_metadata[] = {
        {"row_eq_any", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"row_eq_some", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"row_ne_all", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"row_bang_ne_all", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
    };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open subquery database");
    failures += execute_sql(database, "CREATE DATABASE mylite_task29_subquery", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_task29_subquery", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE outer_t ("
                            "id INT PRIMARY KEY, val INT NULL, txt VARCHAR(10) NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE other_t (id INT PRIMARY KEY, outer_id INT, tag "
                            "VARCHAR(10))",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO outer_t VALUES "
                            "(1,10,'alpha'),(2,20,'beta'),(3,NULL,NULL)",
                            MYLITE_DONE);
    failures += execute_sql(
        database, "INSERT INTO other_t VALUES (10,1,'one'),(20,2,'two'),(30,99,'x')", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE outer_in_t ("
                            "id INT PRIMARY KEY, grp INT NULL, val INT NULL, txt VARCHAR(16) NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE set_in_t ("
                            "set_name VARCHAR(16) NOT NULL, n INT NULL, txt VARCHAR(16) NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE join_in_t ("
                            "id INT PRIMARY KEY, outer_id INT NULL, marker INT NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE pair_t ("
                            "a INT NULL, b INT NULL, label VARCHAR(16) NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE join_pair_t ("
                            "id INT PRIMARY KEY, outer_id INT NULL, marker_a INT NULL, "
                            "marker_b INT NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE warn_pair_t ("
                            "x VARCHAR(16) NULL, y VARCHAR(16) NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE row_quant_outer_t ("
                            "id INT PRIMARY KEY, grp INT NULL, val INT NULL, txt VARCHAR(16) NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE row_quant_join_t ("
                            "id INT PRIMARY KEY, outer_id INT NULL, marker_a INT NULL, "
                            "marker_b INT NULL)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO outer_in_t VALUES "
                            "(1,1,10,'alpha'),"
                            "(2,1,20,'beta'),"
                            "(3,2,NULL,'gamma'),"
                            "(4,NULL,5,NULL),"
                            "(5,3,30,'delta')",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO set_in_t VALUES "
                            "('base',10,'10'),"
                            "('base',20,'20'),"
                            "('base',NULL,NULL),"
                            "('small',1,'1'),"
                            "('small',5,'5'),"
                            "('small',NULL,NULL),"
                            "('order',20,'20'),"
                            "('order',NULL,NULL),"
                            "('having',2,'2'),"
                            "('having',3,'3'),"
                            "('text',NULL,'alpha'),"
                            "('text',NULL,'beta'),"
                            "('warn',NULL,'1x'),"
                            "('warn',NULL,'abc')",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO join_in_t VALUES "
                            "(201,1,10),"
                            "(202,2,20),"
                            "(203,3,30),"
                            "(204,4,NULL),"
                            "(205,5,5)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO pair_t VALUES "
                            "(10,1,'match10'),"
                            "(20,1,'match20'),"
                            "(NULL,2,'null_a'),"
                            "(30,3,'match30'),"
                            "(5,NULL,'null_b'),"
                            "(10,NULL,'null_b_for_10')",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO join_pair_t VALUES "
                            "(301,1,10,1),"
                            "(302,2,20,1),"
                            "(303,3,NULL,2),"
                            "(304,4,5,NULL),"
                            "(305,5,30,3)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO warn_pair_t VALUES "
                            "('1x','2x'),"
                            "('1','bad'),"
                            "('9','bad')",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO row_quant_outer_t VALUES "
                            "(1,1,10,'alpha'),"
                            "(2,1,20,'beta'),"
                            "(3,2,NULL,'gamma'),"
                            "(4,NULL,5,NULL),"
                            "(5,3,30,'delta'),"
                            "(6,9,7,'epsilon'),"
                            "(7,2,7,'zeta'),"
                            "(8,NULL,10,'eta')",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO row_quant_join_t VALUES "
                            "(201,1,10,1),"
                            "(202,2,20,1),"
                            "(203,3,NULL,2),"
                            "(204,4,5,NULL),"
                            "(205,5,30,3),"
                            "(206,6,7,9),"
                            "(207,7,NULL,2),"
                            "(208,8,10,NULL)",
                            MYLITE_DONE);

    failures +=
        expect_select_rows(database,
                           "SELECT (SELECT 1) AS one, "
                           "(SELECT val FROM outer_t WHERE id=99) AS missing, "
                           "EXISTS (SELECT 1) AS exists_one, "
                           "NOT EXISTS (SELECT 1 FROM outer_t WHERE id=99) AS not_missing",
                           scalar_columns, 4, scalar_values, 1, "scalar and exists subqueries");
    failures +=
        expect_select_rows(database,
                           "SELECT (SELECT val FROM outer_t ORDER BY id LIMIT 1) "
                           "AS first_val",
                           first_val_column, 1, first_val_value, 1, "scalar subquery order limit");
    failures +=
        expect_select_rows(database,
                           "SELECT id, (SELECT txt FROM outer_t WHERE id=1) AS first_txt "
                           "FROM outer_t ORDER BY id",
                           id_txt_columns, 2, id_txt_values, 3, "scalar subquery projection");
    failures += expect_select_rows(database, "SELECT id FROM outer_t WHERE (SELECT 1) ORDER BY id",
                                   id_column, 1, all_ids, 3, "scalar subquery where true");
    failures +=
        expect_select_rows(database, "SELECT id FROM outer_t WHERE (SELECT NULL) ORDER BY id",
                           id_column, 1, NULL, 0, "scalar subquery where null");
    failures +=
        expect_select_rows(database, "SELECT id FROM outer_t WHERE val = (SELECT 20) ORDER BY id",
                           id_column, 1, second_id, 1, "scalar subquery comparison");
    failures += expect_select_rows(
        database,
        "SELECT id FROM outer_t WHERE EXISTS (SELECT 1 FROM outer_t WHERE id=2) ORDER BY id",
        id_column, 1, all_ids, 3, "exists subquery where true");
    failures += expect_select_rows(
        database,
        "SELECT id FROM outer_t WHERE EXISTS (SELECT 1 FROM outer_t WHERE id=99) ORDER BY id",
        id_column, 1, NULL, 0, "exists subquery where false");
    failures +=
        expect_select_rows(database, "SELECT id FROM outer_t ORDER BY (SELECT 1), id DESC LIMIT 2",
                           id_column, 1, desc_ids, 2, "scalar subquery order");
    failures +=
        expect_select_rows(database,
                           "SELECT o.id, x.tag FROM outer_t AS o JOIN other_t AS x "
                           "ON EXISTS (SELECT 1 FROM outer_t WHERE id=1) AND o.id = x.outer_id "
                           "ORDER BY o.id",
                           join_columns, 2, join_values, 2, "exists subquery join on");
    failures += expect_select_rows(database,
                                   "SELECT COUNT(*) AS c FROM outer_t "
                                   "HAVING EXISTS (SELECT 1 FROM outer_t WHERE id=1)",
                                   count_column, 1, count_value, 1, "exists subquery having");
    failures += expect_select_rows(database,
                                   "SELECT EXISTS (SELECT 1/0) AS safe_exists, "
                                   "EXISTS (SELECT 1/0 FROM outer_t) AS safe_table_exists, "
                                   "EXISTS (SELECT DISTINCT 1/0 FROM outer_t) "
                                   "AS safe_distinct_exists, "
                                   "EXISTS (SELECT 1/0 FROM outer_t ORDER BY 1/0) "
                                   "AS safe_order_exists",
                                   exists_warning_columns, 4, exists_warning_values, 1,
                                   "exists select list not evaluated");
    failures += expect_int(mylite_warning_count(database), 0, "exists warning count");
    failures += expect_select_rows(database,
                                   "SELECT EXISTS (SELECT 1 FROM outer_t LIMIT 0) "
                                   "AS exists_limit_zero",
                                   exists_limit_column, 1, exists_limit_value, 1,
                                   "exists respects subquery limit zero");
    failures += expect_select_rows(
        database, "SELECT (SELECT 1/0 FROM outer_t WHERE id=1) AS div_null", scalar_warning_column,
        1, scalar_warning_value, 1, "scalar subquery warning propagation");
    failures += expect_int(mylite_warning_count(database), 1, "scalar subquery warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_division_by_zero,
                           "scalar subquery warning code");

    failures += expect_select_rows(
        database,
        "SELECT "
        "10 IN (SELECT n FROM set_in_t WHERE set_name='base') AS match_in, "
        "5 IN (SELECT n FROM set_in_t WHERE set_name='base') AS miss_in_null, "
        "5 NOT IN (SELECT n FROM set_in_t WHERE set_name='base') AS miss_not_in_null, "
        "5 IN (SELECT n FROM set_in_t WHERE set_name='missing') AS empty_in, "
        "5 NOT IN (SELECT n FROM set_in_t WHERE set_name='missing') AS empty_not_in, "
        "NULL IN (SELECT n FROM set_in_t WHERE set_name='base') AS null_in_base, "
        "NULL IN (SELECT n FROM set_in_t WHERE set_name='missing') AS null_empty_in, "
        "NULL NOT IN (SELECT n FROM set_in_t WHERE set_name='missing') AS null_empty_not_in",
        in_truth_columns, 8, in_truth_values, 1, "in subquery truth table");
    failures += expect_select_rows(
        database,
        "SELECT id, "
        "val IN (SELECT n FROM set_in_t WHERE set_name='base') AS in_base, "
        "val NOT IN (SELECT n FROM set_in_t WHERE set_name='base') AS not_in_base "
        "FROM outer_in_t ORDER BY id",
        in_projection_columns, 3, in_projection_values, 5, "in subquery projection");
    failures += expect_select_rows(
        database,
        "SELECT id FROM outer_in_t "
        "WHERE val IN (SELECT n FROM set_in_t WHERE set_name='base') ORDER BY id",
        id_column, 1, in_two_ids, 2, "in subquery where");
    failures += expect_select_rows(
        database,
        "SELECT id FROM outer_in_t "
        "WHERE val NOT IN (SELECT n FROM set_in_t WHERE set_name='missing') ORDER BY id",
        id_column, 1, in_all_ids, 5, "not in empty subquery where");
    failures += expect_select_rows(database,
                                   "SELECT o.id AS outer_id, j.id AS join_id FROM outer_in_t AS o "
                                   "JOIN join_in_t AS j "
                                   "ON j.outer_id=o.id AND j.marker IN "
                                   "(SELECT n FROM set_in_t WHERE set_name='base') "
                                   "ORDER BY o.id, j.id",
                                   in_join_columns, 2, in_join_values, 2, "in subquery join on");
    failures +=
        expect_select_rows(database,
                           "SELECT grp, COUNT(*) AS c FROM outer_in_t GROUP BY grp "
                           "HAVING COUNT(*) IN (SELECT n FROM set_in_t WHERE set_name='having') "
                           "ORDER BY grp",
                           in_having_columns, 2, in_having_values, 1, "in subquery having");
    failures += expect_select_rows(
        database,
        "SELECT id FROM outer_in_t "
        "ORDER BY val IN (SELECT n FROM set_in_t WHERE set_name='order') DESC, id",
        id_column, 1, in_order_ids, 5, "in subquery hidden order");
    failures += expect_select_rows(
        database,
        "SELECT "
        "'alpha' IN (SELECT txt FROM set_in_t WHERE set_name='text') AS text_in, "
        "'zeta' NOT IN (SELECT txt FROM set_in_t WHERE set_name='text') AS text_not_in",
        in_text_columns, 2, in_text_values, 1, "text in subquery");
    failures += expect_select_rows(
        database,
        "SELECT "
        "10 IN (SELECT n FROM set_in_t WHERE set_name='base' ORDER BY n DESC) "
        "AS ordered_in, "
        "10 IN (SELECT DISTINCT n FROM set_in_t WHERE set_name IN ('base','order')) "
        "AS distinct_in, "
        "2 IN (SELECT COUNT(*) FROM outer_in_t GROUP BY grp HAVING COUNT(*) >= 2) "
        "AS group_in",
        in_inner_clause_columns, 3, in_inner_clause_values, 1, "in subquery inner clauses");
    failures += expect_select_rows(
        database,
        "SELECT 5 IN (SELECT n FROM set_in_t WHERE set_name='base' ORDER BY 1/0) "
        "AS ignored_order_warning",
        in_order_warning_column, 1, in_order_warning_value, 1,
        "in subquery ignored order warnings");
    failures +=
        expect_int(mylite_warning_count(database), 0, "in subquery ignored order warning count");

    failures += expect_select_rows(
        database, "SELECT 2 IN (SELECT txt FROM set_in_t WHERE set_name='warn') AS warn_in",
        in_warning_column, 1, in_warning_false, 1, "in subquery warnings false");
    failures += expect_int(mylite_warning_count(database), 2, "in subquery warning count false");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_truncated_wrong_value, "in subquery warning false code 0");
    failures += expect_int((int)mylite_warning_code(database, 1),
                           mysql_warning_truncated_wrong_value, "in subquery warning false code 1");
    failures += expect_select_rows(
        database, "SELECT 1 IN (SELECT txt FROM set_in_t WHERE set_name='warn') AS warn_in",
        in_warning_column, 1, in_warning_true, 1, "in subquery warnings true");
    failures += expect_int(mylite_warning_count(database), 1, "in subquery warning count true");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_truncated_wrong_value, "in subquery warning true code");
    failures += expect_select_rows(
        database, "SELECT 2 NOT IN (SELECT txt FROM set_in_t WHERE set_name='warn') AS warn_in",
        in_warning_column, 1, in_warning_true, 1, "not in subquery warnings true");
    failures += expect_int(mylite_warning_count(database), 2, "not in subquery warning count true");
    failures += expect_select_rows(
        database,
        "SELECT val IN (SELECT txt FROM set_in_t WHERE set_name='warn') AS warn_in "
        "FROM outer_in_t ORDER BY id",
        in_warning_column, 1, in_warning_projection_values, 5,
        "in subquery projection warning rows");
    failures +=
        expect_int(mylite_warning_count(database), 8, "in subquery projection warning count");

    failures += prepare_sql(database,
                            "SELECT (SELECT val FROM outer_t WHERE id=1) AS sub_val, "
                            "EXISTS (SELECT 1 FROM outer_t WHERE id=1) AS has_one "
                            "FROM outer_t LIMIT 0",
                            MYLITE_OK, &stmt);
    failures += expect_result_metadata(stmt, metadata, 2, "subquery metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "subquery metadata done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "SELECT "
                            "val IN (SELECT n FROM set_in_t WHERE set_name='base') AS in_result, "
                            "val NOT IN (SELECT n FROM set_in_t WHERE set_name='base') "
                            "AS not_in_result, "
                            "5 IN (SELECT n FROM set_in_t WHERE set_name='missing') "
                            "AS no_table_in, "
                            "2 IN (SELECT txt FROM set_in_t WHERE set_name='warn') "
                            "AS numeric_text_in "
                            "FROM outer_in_t LIMIT 0",
                            MYLITE_OK, &stmt);
    failures += expect_column_names(stmt, in_metadata_columns, 4, "in subquery metadata names");
    failures += expect_result_metadata(stmt, in_metadata, 4, "in subquery metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "in subquery metadata done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += expect_select_rows(
        database,
        "SELECT "
        "10 = ANY (SELECT n FROM set_in_t WHERE set_name='base') AS eq_any, "
        "10 <> SOME (SELECT n FROM set_in_t WHERE set_name='base') AS ne_some, "
        "10 != SOME (SELECT n FROM set_in_t WHERE set_name='base') AS bang_ne_some, "
        "10 < ANY (SELECT n FROM set_in_t WHERE set_name='base') AS lt_any, "
        "1 <= ALL (SELECT n FROM set_in_t WHERE set_name='small' AND n IS NOT NULL) "
        "AS le_all, "
        "20 >= ALL (SELECT n FROM set_in_t WHERE set_name='base' AND n IS NOT NULL) "
        "AS ge_all, "
        "7 > ANY (SELECT n FROM set_in_t WHERE set_name='missing') AS empty_any, "
        "7 > ALL (SELECT n FROM set_in_t WHERE set_name='missing') AS empty_all, "
        "NULL > ANY (SELECT n FROM set_in_t WHERE set_name='missing') AS null_empty_any, "
        "NULL > ALL (SELECT n FROM set_in_t WHERE set_name='missing') AS null_empty_all, "
        "7 > ANY (SELECT n FROM set_in_t WHERE set_name='order') AS gt_any_order, "
        "7 > ALL (SELECT n FROM set_in_t WHERE set_name='order') AS gt_all_order, "
        "7 < ANY (SELECT n FROM set_in_t WHERE set_name='small') AS lt_any_small, "
        "7 < ALL (SELECT n FROM set_in_t WHERE set_name='small') AS lt_all_small, "
        "10 = ALL (SELECT n FROM set_in_t WHERE set_name='missing') AS eq_all_empty, "
        "10 = ANY (SELECT n FROM set_in_t WHERE set_name='missing') AS eq_any_empty",
        quantified_truth_columns, 16, quantified_truth_values, 1,
        "quantified subquery truth table");
    failures += expect_select_rows(
        database,
        "SELECT id, "
        "val > ANY (SELECT n FROM set_in_t WHERE set_name='base') AS gt_any_base, "
        "val >= ALL (SELECT n FROM set_in_t WHERE set_name='base') AS ge_all_base "
        "FROM outer_in_t ORDER BY id",
        quantified_projection_columns, 3, quantified_projection_values, 5,
        "quantified subquery projection");
    failures += expect_select_rows(
        database,
        "SELECT id FROM outer_in_t "
        "WHERE val > ANY (SELECT n FROM set_in_t WHERE set_name='base') ORDER BY id",
        id_column, 1, quantified_two_ids, 2, "quantified subquery where any");
    failures += expect_select_rows(
        database,
        "SELECT id FROM outer_in_t "
        "WHERE val >= ALL (SELECT n FROM set_in_t WHERE set_name='base' AND n IS NOT NULL) "
        "ORDER BY id",
        id_column, 1, quantified_two_ids, 2, "quantified subquery where all");
    failures += expect_select_rows(database,
                                   "SELECT o.id AS outer_id, j.id AS join_id FROM outer_in_t AS o "
                                   "JOIN join_in_t AS j "
                                   "ON j.outer_id=o.id AND j.marker = ANY "
                                   "(SELECT n FROM set_in_t WHERE set_name='base') "
                                   "ORDER BY o.id, j.id",
                                   in_join_columns, 2, quantified_join_values, 2,
                                   "quantified subquery join on");
    failures += expect_select_rows(
        database,
        "SELECT grp, COUNT(*) AS c FROM outer_in_t GROUP BY grp "
        "HAVING COUNT(*) = ANY (SELECT n FROM set_in_t WHERE set_name='having') "
        "ORDER BY grp",
        in_having_columns, 2, quantified_having_values, 1, "quantified subquery having");
    failures += expect_select_rows(
        database,
        "SELECT id FROM outer_in_t "
        "ORDER BY val <> SOME (SELECT n FROM set_in_t WHERE set_name='order') DESC, id",
        id_column, 1, quantified_order_ids, 5, "quantified subquery hidden order");
    failures += expect_select_rows(
        database,
        "SELECT "
        "10 > ANY (SELECT n FROM set_in_t WHERE set_name='base' ORDER BY n DESC) "
        "AS ordered_any, "
        "10 = ANY (SELECT DISTINCT n FROM set_in_t WHERE set_name IN ('base','order')) "
        "AS distinct_any, "
        "2 = ALL (SELECT COUNT(*) FROM outer_in_t GROUP BY grp HAVING COUNT(*) >= 2) "
        "AS group_all, "
        "10 > ANY (SELECT n FROM set_in_t WHERE set_name='base' ORDER BY 1/0) "
        "AS order_warning_any, "
        "10 > ALL (SELECT n FROM set_in_t WHERE set_name='base' ORDER BY 1/0) "
        "AS order_warning_all",
        quantified_inner_clause_columns, 5, quantified_inner_clause_values, 1,
        "quantified subquery inner clauses");
    failures += expect_int(mylite_warning_count(database), 0,
                           "quantified subquery ignored order warning count");

    failures += expect_select_rows(
        database, "SELECT 0 > ANY (SELECT txt FROM set_in_t WHERE set_name='warn') AS warn_cmp",
        quantified_warning_column, 1, quantified_warning_false, 1,
        "quantified subquery any warnings false");
    failures += expect_int(mylite_warning_count(database), 2, "quantified any warning count false");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_truncated_wrong_value,
                   "quantified any warning false code 0");
    failures +=
        expect_int((int)mylite_warning_code(database, 1), mysql_warning_truncated_wrong_value,
                   "quantified any warning false code 1");
    failures += expect_select_rows(
        database, "SELECT 1 = ANY (SELECT txt FROM set_in_t WHERE set_name='warn') AS warn_cmp",
        quantified_warning_column, 1, quantified_warning_true, 1,
        "quantified subquery any warnings true");
    failures += expect_int(mylite_warning_count(database), 1, "quantified any warning count true");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_truncated_wrong_value, "quantified any warning true code");
    failures += expect_select_rows(
        database, "SELECT 0 > ALL (SELECT txt FROM set_in_t WHERE set_name='warn') AS warn_cmp",
        quantified_warning_column, 1, quantified_warning_false, 1,
        "quantified subquery all warnings false");
    failures += expect_int(mylite_warning_count(database), 1, "quantified all warning count false");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_truncated_wrong_value,
                   "quantified all warning false code");
    failures += expect_select_rows(
        database, "SELECT 2 > ALL (SELECT txt FROM set_in_t WHERE set_name='warn') AS warn_cmp",
        quantified_warning_column, 1, quantified_warning_true, 1,
        "quantified subquery all warnings true");
    failures += expect_int(mylite_warning_count(database), 2, "quantified all warning count true");
    failures += expect_select_rows(
        database,
        "SELECT val > ANY (SELECT txt FROM set_in_t WHERE set_name='warn') AS warn_cmp "
        "FROM outer_in_t ORDER BY id",
        quantified_warning_column, 1, quantified_warning_projection_values, 5,
        "quantified subquery projection warning rows");
    failures += expect_int(mylite_warning_count(database), 4,
                           "quantified subquery projection warning count");

    failures +=
        prepare_sql(database,
                    "SELECT "
                    "val > ANY (SELECT n FROM set_in_t WHERE set_name='base') AS any_result, "
                    "val >= ALL (SELECT n FROM set_in_t WHERE set_name='base') AS all_result, "
                    "val <> SOME (SELECT n FROM set_in_t WHERE set_name='base') AS some_result "
                    "FROM outer_in_t LIMIT 0",
                    MYLITE_OK, &stmt);
    failures += expect_column_names(stmt, quantified_metadata_columns, 3,
                                    "quantified subquery metadata names");
    failures +=
        expect_result_metadata(stmt, quantified_metadata, 3, "quantified subquery metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "quantified subquery metadata done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += expect_select_rows(database,
                                   "SELECT "
                                   "(1,2) = (SELECT 1,2) AS eq_match, "
                                   "(1,2) = (SELECT 1,3) AS eq_miss, "
                                   "(1,NULL) = (SELECT 1,NULL) AS eq_null_tail, "
                                   "(NULL,2) = (SELECT NULL,2) AS eq_null_head, "
                                   "(1,2) <> (SELECT 1,3) AS ne_miss, "
                                   "(1,NULL) <> (SELECT 1,NULL) AS ne_null_tail, "
                                   "(1,2) < (SELECT 1,3) AS lt_same_head, "
                                   "(1,4) < (SELECT 2,0) AS lt_next_head, "
                                   "(2,0) < (SELECT 1,99) AS lt_prev_head, "
                                   "(1,NULL) < (SELECT 1,3) AS lt_null_tail, "
                                   "(0,NULL) < (SELECT 1,3) AS lt_early_true, "
                                   "(2,NULL) < (SELECT 1,3) AS lt_early_false, "
                                   "ROW(1,2) = (SELECT 1,2) AS row_eq, "
                                   "(1,2) <=> (SELECT 1,2) AS nse_match, "
                                   "(1,2) <=> (SELECT 1,3) AS nse_miss, "
                                   "(1,NULL) <=> (SELECT 1,NULL) AS nse_null_tail, "
                                   "(NULL,2) <=> (SELECT NULL,2) AS nse_null_head, "
                                   "(NULL,NULL) <=> (SELECT NULL,NULL) AS nse_all_null, "
                                   "(1,2) = (SELECT a,b FROM pair_t WHERE a=999) AS empty_eq, "
                                   "(1,2) <=> (SELECT a,b FROM pair_t WHERE a=999) AS empty_nse",
                                   row_scalar_columns, 20, row_scalar_values, 1,
                                   "row scalar subquery truth table");
    failures += expect_select_rows(database,
                                   "SELECT "
                                   "(NULL,2,3) <=> (SELECT NULL,2,3) AS nse_head_null, "
                                   "(1,NULL,3) <=> (SELECT 1,NULL,3) AS nse_middle_null, "
                                   "(1,2,NULL) <=> (SELECT 1,2,NULL) AS nse_tail_null, "
                                   "(NULL,NULL,NULL) <=> (SELECT NULL,NULL,NULL) "
                                   "AS nse_all_null",
                                   row_scalar_nse_columns, 4, row_scalar_nse_values, 1,
                                   "row scalar null-safe multi-column subquery");
    failures += expect_select_rows(
        database,
        "SELECT "
        "(10,1) IN (SELECT a,b FROM pair_t) AS match_in, "
        "(10,1) NOT IN (SELECT a,b FROM pair_t) AS match_not_in, "
        "(7,2) IN (SELECT a,b FROM pair_t) AS unknown_in, "
        "(7,2) NOT IN (SELECT a,b FROM pair_t) AS unknown_not_in, "
        "(NULL,2) IN (SELECT a,b FROM pair_t) AS left_null_in, "
        "(10,NULL) IN (SELECT a,b FROM pair_t) AS left_null2_in, "
        "(7,9) IN (SELECT a,b FROM pair_t) AS miss_in, "
        "(7,9) NOT IN (SELECT a,b FROM pair_t) AS miss_not_in, "
        "(10,1) IN (SELECT a,b FROM pair_t WHERE a=999) AS empty_in, "
        "(10,1) NOT IN (SELECT a,b FROM pair_t WHERE a=999) AS empty_not_in, "
        "(NULL,2) IN (SELECT a,b FROM pair_t WHERE a=999) AS null_empty_in, "
        "(NULL,2) NOT IN (SELECT a,b FROM pair_t WHERE a=999) AS null_empty_not_in",
        row_in_truth_columns, 12, row_in_truth_values, 1, "row in subquery truth table");
    failures += expect_select_rows(database,
                                   "SELECT id, "
                                   "(val,grp) IN (SELECT a,b FROM pair_t) AS row_in, "
                                   "(val,grp) NOT IN (SELECT a,b FROM pair_t) AS row_not_in "
                                   "FROM outer_in_t ORDER BY id",
                                   row_projection_columns, 3, row_projection_values, 5,
                                   "row in subquery projection");
    failures += expect_select_rows(database,
                                   "SELECT id FROM outer_in_t "
                                   "WHERE (val,grp) IN (SELECT a,b FROM pair_t) ORDER BY id",
                                   id_column, 1, row_match_ids, 3, "row in subquery where");
    failures += expect_select_rows(
        database,
        "SELECT o.id AS outer_id, j.id AS join_id FROM outer_in_t AS o "
        "JOIN join_pair_t AS j "
        "ON j.outer_id=o.id AND (j.marker_a,j.marker_b) IN (SELECT a,b FROM pair_t) "
        "ORDER BY o.id, j.id",
        in_join_columns, 2, row_join_values, 3, "row in subquery join on");
    failures +=
        expect_select_rows(database,
                           "SELECT grp, COUNT(*) AS c FROM outer_in_t GROUP BY grp "
                           "HAVING (grp, COUNT(*)) IN "
                           "(SELECT b, COUNT(*) FROM pair_t WHERE b IS NOT NULL GROUP BY b) "
                           "ORDER BY grp",
                           in_having_columns, 2, row_having_values, 3, "row in subquery having");
    failures += expect_select_rows(database,
                                   "SELECT id FROM outer_in_t "
                                   "ORDER BY (val,grp) IN (SELECT a,b FROM pair_t) DESC, id",
                                   id_column, 1, row_order_ids, 5, "row in subquery hidden order");
    failures += expect_select_rows(
        database,
        "SELECT "
        "(10,1) IN (SELECT a,b FROM pair_t ORDER BY label DESC) AS ordered_row_in, "
        "(10,1) IN (SELECT DISTINCT a,b FROM pair_t) AS distinct_row_in, "
        "(1,2) IN "
        "(SELECT b, COUNT(*) FROM pair_t WHERE b IS NOT NULL GROUP BY b HAVING COUNT(*) >= 1) "
        "AS group_row_in",
        row_inner_clause_columns, 3, row_inner_clause_values, 1, "row in subquery inner clauses");
    failures += expect_select_rows(
        database, "SELECT (7,9) IN (SELECT a,b FROM pair_t ORDER BY 1/0) AS row_order_warning",
        row_order_warning_column, 1, row_warning_false, 1, "row in ignored order warnings");
    failures += expect_int(mylite_warning_count(database), 0, "row in ignored order warning count");
    failures +=
        expect_select_rows(database, "SELECT (1,2) IN (SELECT x,y FROM warn_pair_t) AS row_warn",
                           row_warning_column, 1, row_warning_true, 1, "row in warning true");
    failures += expect_int(mylite_warning_count(database), 2, "row in warning true count");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_truncated_wrong_value, "row in warning true code 0");
    failures += expect_int((int)mylite_warning_code(database, 1),
                           mysql_warning_truncated_wrong_value, "row in warning true code 1");
    failures +=
        expect_select_rows(database, "SELECT (1,3) IN (SELECT x,y FROM warn_pair_t) AS row_warn",
                           row_warning_column, 1, row_warning_false, 1, "row in warning false");
    failures += expect_int(mylite_warning_count(database), 3, "row in warning false count");
    failures += expect_select_rows(
        database, "SELECT (2,3) IN (SELECT x,y FROM warn_pair_t) AS row_warn", row_warning_column,
        1, row_warning_false, 1, "row in warning first element false");
    failures += expect_int(mylite_warning_count(database), 1, "row in first element warning count");
    failures +=
        expect_select_rows(database, "SELECT (NULL,2) IN (SELECT x,y FROM warn_pair_t) AS row_warn",
                           row_warning_column, 1, row_warning_null, 1, "row in left null warning");
    failures += expect_int(mylite_warning_count(database), 1, "row in left null warning count");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_truncated_wrong_value, "row in left null warning code");
    failures += expect_select_rows(
        database, "SELECT (1,3) NOT IN (SELECT x,y FROM warn_pair_t) AS row_warn",
        row_warning_column, 1, row_warning_true, 1, "row not in warning true");
    failures += expect_int(mylite_warning_count(database), 3, "row not in warning true count");
    failures += expect_select_rows(
        database, "SELECT (1,2) = (SELECT x,y FROM warn_pair_t LIMIT 1) AS row_scalar_warn",
        row_scalar_warning_column, 1, row_warning_true, 1, "row scalar warning suppression");
    failures += expect_int(mylite_warning_count(database), 0, "row scalar warning count");

    failures += prepare_sql(database,
                            "SELECT "
                            "(val,grp) IN (SELECT a,b FROM pair_t) AS row_in_result, "
                            "(val,grp) NOT IN (SELECT a,b FROM pair_t) AS row_not_in_result, "
                            "(val,grp) = (SELECT a,b FROM pair_t WHERE label='match10') "
                            "AS row_eq_result, "
                            "(val,grp) <=> (SELECT a,b FROM pair_t WHERE label='match10') "
                            "AS row_nse_result, "
                            "(10,1) IN (SELECT a,b FROM pair_t WHERE a=999) AS no_table_row_in "
                            "FROM outer_in_t LIMIT 0",
                            MYLITE_OK, &stmt);
    failures += expect_column_names(stmt, row_metadata_columns, 5, "row subquery metadata names");
    failures += expect_result_metadata(stmt, row_metadata, 5, "row subquery metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "row subquery metadata done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += expect_select_rows(
        database,
        "SELECT "
        "(10,1) = ANY (SELECT a,b FROM pair_t) AS eq_any, "
        "(10,1) = SOME (SELECT a,b FROM pair_t) AS eq_some, "
        "ROW(10,1) = ANY (SELECT a,b FROM pair_t) AS row_eq_any, "
        "(10,1) <> ALL (SELECT a,b FROM pair_t) AS ne_all, "
        "(10,1) != ALL (SELECT a,b FROM pair_t) AS bang_ne_all, "
        "ROW(10,1) <> ALL (SELECT a,b FROM pair_t) AS row_ne_all, "
        "(7,2) = ANY (SELECT a,b FROM pair_t) AS unknown_any, "
        "(7,9) = ANY (SELECT a,b FROM pair_t) AS miss_any, "
        "(7,2) <> ALL (SELECT a,b FROM pair_t) AS unknown_ne_all, "
        "(7,9) <> ALL (SELECT a,b FROM pair_t) AS miss_ne_all, "
        "(NULL,2) = ANY (SELECT a,b FROM pair_t WHERE a=999) AS empty_any, "
        "(NULL,2) <> ALL (SELECT a,b FROM pair_t WHERE a=999) AS empty_ne_all",
        row_quant_truth_columns, 12, row_quant_truth_values, 1, "row quantified truth table");
    failures += expect_select_rows(database,
                                   "SELECT id, "
                                   "(val,grp) = ANY (SELECT a,b FROM pair_t) AS row_eq_any, "
                                   "(val,grp) <> ALL (SELECT a,b FROM pair_t) AS row_ne_all "
                                   "FROM row_quant_outer_t ORDER BY id",
                                   row_quant_projection_columns, 3, row_quant_projection_values, 8,
                                   "row quantified projection");
    failures +=
        expect_select_rows(database,
                           "SELECT id FROM row_quant_outer_t "
                           "WHERE (val,grp) = ANY (SELECT a,b FROM pair_t) ORDER BY id",
                           id_column, 1, row_quant_match_ids, 3, "row quantified where any");
    failures +=
        expect_select_rows(database,
                           "SELECT id FROM row_quant_outer_t "
                           "WHERE (val,grp) <> ALL (SELECT a,b FROM pair_t) ORDER BY id",
                           id_column, 1, row_quant_not_all_ids, 1, "row quantified where all");
    failures += expect_select_rows(
        database,
        "SELECT o.id AS outer_id, j.id AS join_id FROM row_quant_outer_t AS o "
        "JOIN row_quant_join_t AS j "
        "ON j.outer_id=o.id AND (j.marker_a,j.marker_b) = ANY (SELECT a,b FROM pair_t) "
        "ORDER BY o.id, j.id",
        in_join_columns, 2, row_quant_join_values, 3, "row quantified join on");
    failures += expect_select_rows(
        database,
        "SELECT grp, COUNT(*) AS c FROM row_quant_outer_t GROUP BY grp "
        "HAVING (grp, COUNT(*)) = ANY "
        "(SELECT b, COUNT(*) FROM pair_t WHERE b IS NOT NULL GROUP BY b) "
        "ORDER BY grp",
        in_having_columns, 2, row_quant_having_values, 2, "row quantified having");
    failures +=
        expect_select_rows(database,
                           "SELECT id FROM row_quant_outer_t "
                           "ORDER BY (val,grp) = ANY (SELECT a,b FROM pair_t) DESC, id",
                           id_column, 1, row_quant_order_ids, 8, "row quantified hidden order");
    failures += expect_select_rows(database,
                                   "SELECT "
                                   "(10,1) = ANY (SELECT a,b FROM pair_t) AS row_eq_any, "
                                   "(10,1) <> ALL (SELECT a,b FROM pair_t) AS row_ne_all",
                                   row_quant_no_table_columns, 2, row_quant_no_table_values, 1,
                                   "row quantified no-table select");
    failures += expect_select_rows(
        database,
        "SELECT "
        "(10,1) = ANY (SELECT a,b FROM pair_t ORDER BY label DESC) AS ordered_row_any, "
        "(10,1) = ANY (SELECT DISTINCT a,b FROM pair_t) AS distinct_row_any, "
        "(1,2) = ANY "
        "(SELECT b, COUNT(*) FROM pair_t WHERE b IS NOT NULL GROUP BY b HAVING COUNT(*) >= 1) "
        "AS group_row_any",
        row_quant_inner_clause_columns, 3, row_quant_inner_clause_values, 1,
        "row quantified inner clauses");
    failures += expect_select_rows(
        database,
        "SELECT (10,1) = ANY (SELECT a,b FROM pair_t ORDER BY 1/0) AS order_warning_row_any",
        row_quant_order_warning_column, 1, row_quant_warning_true, 1,
        "row quantified ignored order warnings");
    failures +=
        expect_int(mylite_warning_count(database), 0, "row quantified ignored order warning count");
    failures += expect_select_rows(
        database, "SELECT (1,2) = ANY (SELECT x,y FROM warn_pair_t) AS row_quant_warn",
        row_quant_warning_column, 1, row_quant_warning_true, 1, "row quantified warning true");
    failures += expect_int(mylite_warning_count(database), 2, "row quantified warning true count");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_truncated_wrong_value,
                   "row quantified warning true code 0");
    failures +=
        expect_int((int)mylite_warning_code(database, 1), mysql_warning_truncated_wrong_value,
                   "row quantified warning true code 1");
    failures += expect_select_rows(
        database, "SELECT (1,3) = ANY (SELECT x,y FROM warn_pair_t) AS row_quant_warn",
        row_quant_warning_column, 1, row_quant_warning_false, 1, "row quantified warning false");
    failures += expect_int(mylite_warning_count(database), 3, "row quantified warning false count");
    failures += expect_select_rows(
        database, "SELECT (2,3) = ANY (SELECT x,y FROM warn_pair_t) AS row_quant_warn",
        row_quant_warning_column, 1, row_quant_warning_false, 1,
        "row quantified warning first element false");
    failures +=
        expect_int(mylite_warning_count(database), 1, "row quantified first element warning count");
    failures += expect_select_rows(
        database, "SELECT (NULL,2) = ANY (SELECT x,y FROM warn_pair_t) AS row_quant_warn",
        row_quant_warning_column, 1, row_quant_warning_null, 1, "row quantified left null warning");
    failures +=
        expect_int(mylite_warning_count(database), 1, "row quantified left null warning count");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_truncated_wrong_value,
                   "row quantified left null warning code");
    failures += expect_select_rows(
        database, "SELECT (1,3) <> ALL (SELECT x,y FROM warn_pair_t) AS row_quant_warn",
        row_quant_warning_column, 1, row_quant_warning_true, 1,
        "row quantified not all warning true");
    failures +=
        expect_int(mylite_warning_count(database), 3, "row quantified not all warning true count");
    failures +=
        expect_select_rows(database,
                           "SELECT (val,grp) = ANY (SELECT x,y FROM warn_pair_t) AS row_quant_warn "
                           "FROM row_quant_outer_t ORDER BY id",
                           row_quant_warning_column, 1, row_quant_warning_projection_values, 8,
                           "row quantified projection warning rows");
    failures +=
        expect_int(mylite_warning_count(database), 8, "row quantified projection warning count");

    failures += prepare_sql(database,
                            "SELECT "
                            "(val,grp) = ANY (SELECT a,b FROM pair_t) AS row_eq_any, "
                            "(val,grp) = SOME (SELECT a,b FROM pair_t) AS row_eq_some, "
                            "(val,grp) <> ALL (SELECT a,b FROM pair_t) AS row_ne_all, "
                            "(val,grp) != ALL (SELECT a,b FROM pair_t) AS row_bang_ne_all "
                            "FROM row_quant_outer_t LIMIT 0",
                            MYLITE_OK, &stmt);
    failures +=
        expect_column_names(stmt, row_quant_metadata_columns, 4, "row quantified metadata names");
    failures += expect_result_metadata(stmt, row_quant_metadata, 4, "row quantified metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "row quantified metadata done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        expect_prepare_error(database, "SELECT (SELECT val FROM outer_t)", MYLITE_EXEC_ERROR,
                             "Subquery returns more than 1 row", "scalar subquery cardinality");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_subquery_no_1_row,
                           "scalar subquery cardinality warning code");
    failures += expect_prepare_error(database, "SELECT (SELECT val, txt FROM outer_t WHERE id=1)",
                                     MYLITE_EXEC_ERROR, "Operand should contain 1 column(s)",
                                     "scalar subquery column count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_operand_columns,
                           "scalar subquery column count warning code");

    failures += prepare_sql(database,
                            "SELECT id, (SELECT val FROM outer_t) AS too_many "
                            "FROM outer_t ORDER BY id",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Subquery returns more than 1 row",
                                  "table scalar subquery cardinality");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_subquery_no_1_row,
                           "table scalar subquery cardinality warning code");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += expect_prepare_error(
        database, "SELECT 1 IN (SELECT n, txt FROM set_in_t WHERE set_name='base')",
        MYLITE_EXEC_ERROR, "Operand should contain 1 column(s)", "in subquery column count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_operand_columns,
                           "in subquery column count warning code");
    failures +=
        expect_prepare_error(database, "SELECT 1 IN (SELECT missing_col FROM set_in_t)",
                             MYLITE_EXEC_ERROR, "Unknown column", "in subquery unknown column");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown_column,
                           "in subquery unknown column warning code");
    failures += expect_prepare_error(
        database, "SELECT 1 IN (SELECT n FROM set_in_t WHERE set_name='base' LIMIT 1)",
        MYLITE_EXEC_ERROR, "LIMIT & IN/ALL/ANY/SOME subquery", "in subquery limit");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_not_supported_yet,
                           "in subquery limit warning code");
    failures += expect_prepare_error(
        database, "SELECT 1 IN (SELECT n, txt FROM set_in_t WHERE set_name='base' LIMIT 1)",
        MYLITE_EXEC_ERROR, "LIMIT & IN/ALL/ANY/SOME subquery",
        "in subquery limit before column count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_not_supported_yet,
                           "in subquery limit before column count warning code");
    failures += expect_prepare_error(
        database, "SELECT 1 = ANY (SELECT n, txt FROM set_in_t WHERE set_name='base')",
        MYLITE_EXEC_ERROR, "Operand should contain 1 column(s)",
        "quantified subquery column count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_operand_columns,
                           "quantified subquery column count warning code");
    failures += expect_prepare_error(database, "SELECT 1 = ANY (SELECT missing_col FROM set_in_t)",
                                     MYLITE_EXEC_ERROR, "Unknown column",
                                     "quantified subquery unknown column");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown_column,
                           "quantified subquery unknown column warning code");
    failures += expect_prepare_error(
        database, "SELECT 1 = ANY (SELECT n FROM set_in_t WHERE set_name='base' LIMIT 1)",
        MYLITE_EXEC_ERROR, "LIMIT & IN/ALL/ANY/SOME subquery", "quantified any subquery limit");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_not_supported_yet,
                           "quantified any subquery limit warning code");
    failures += expect_prepare_error(
        database, "SELECT 1 = ANY (SELECT n, txt FROM set_in_t WHERE set_name='base' LIMIT 1)",
        MYLITE_EXEC_ERROR, "LIMIT & IN/ALL/ANY/SOME subquery",
        "quantified subquery limit before column count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_not_supported_yet,
                           "quantified subquery limit before column count warning code");
    failures += expect_prepare_error(
        database, "SELECT 1 = SOME (SELECT n FROM set_in_t WHERE set_name='base' LIMIT 1)",
        MYLITE_EXEC_ERROR, "LIMIT & IN/ALL/ANY/SOME subquery", "quantified some subquery limit");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_not_supported_yet,
                           "quantified some subquery limit warning code");
    failures += expect_prepare_error(
        database, "SELECT 1 = ALL (SELECT n FROM set_in_t WHERE set_name='base' LIMIT 1)",
        MYLITE_EXEC_ERROR, "LIMIT & IN/ALL/ANY/SOME subquery", "quantified all subquery limit");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_not_supported_yet,
                           "quantified all subquery limit warning code");
    failures +=
        prepare_sql(database, "SELECT 1 <=> ANY (SELECT n FROM set_in_t WHERE set_name='base')",
                    MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "null-safe quantified subquery rejected");
    failures +=
        expect_prepare_error(database, "SELECT (1,2) IN (SELECT a FROM pair_t)", MYLITE_EXEC_ERROR,
                             "Operand should contain 2 column(s)", "row in subquery column count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_operand_columns,
                           "row in subquery column count warning code");
    failures += expect_prepare_error(database, "SELECT (1) IN (SELECT a,b FROM pair_t)",
                                     MYLITE_EXEC_ERROR, "Operand should contain 1 column(s)",
                                     "scalar row-looking in column count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_operand_columns,
                           "scalar row-looking in column count warning code");
    failures +=
        expect_prepare_error(database, "SELECT (1,2) = (SELECT a,b FROM pair_t)", MYLITE_EXEC_ERROR,
                             "Subquery returns more than 1 row", "row scalar subquery cardinality");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_subquery_no_1_row,
                           "row scalar subquery cardinality warning code");
    failures += expect_prepare_error(database, "SELECT (1,2) = (SELECT a FROM pair_t LIMIT 1)",
                                     MYLITE_EXEC_ERROR, "Operand should contain 2 column(s)",
                                     "row scalar subquery column count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_operand_columns,
                           "row scalar subquery column count warning code");
    failures += expect_prepare_error(database, "SELECT (1,2) IN (SELECT a,b FROM pair_t LIMIT 1)",
                                     MYLITE_EXEC_ERROR, "LIMIT & IN/ALL/ANY/SOME subquery",
                                     "row in subquery limit");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_not_supported_yet,
                           "row in subquery limit warning code");
    failures += expect_prepare_error(database, "SELECT (1,2) IN (SELECT a FROM pair_t LIMIT 1)",
                                     MYLITE_EXEC_ERROR, "LIMIT & IN/ALL/ANY/SOME subquery",
                                     "row in subquery limit before column count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_not_supported_yet,
                           "row in subquery limit before column count warning code");
    failures +=
        expect_prepare_error(database, "SELECT (1,2) IN (SELECT missing_col,b FROM pair_t)",
                             MYLITE_EXEC_ERROR, "Unknown column", "row in subquery unknown column");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown_column,
                           "row in subquery unknown column warning code");
    failures += expect_prepare_error(database,
                                     "SELECT DISTINCT id FROM outer_in_t "
                                     "ORDER BY (val,grp) IN (SELECT a,b FROM pair_t)",
                                     MYLITE_EXEC_ERROR,
                                     "Expression #1 of ORDER BY clause is not in SELECT list",
                                     "row in distinct hidden order column");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_field_in_order_not_select,
                   "row in distinct hidden order column warning code");
    failures += expect_prepare_error(database, "SELECT (1,2) = ANY (SELECT a FROM pair_t)",
                                     MYLITE_EXEC_ERROR, "Operand should contain 2 column(s)",
                                     "row quantified narrow column count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_operand_columns,
                           "row quantified narrow column count warning code");
    failures += expect_prepare_error(database, "SELECT (1,2) = ANY (SELECT a,b,label FROM pair_t)",
                                     MYLITE_EXEC_ERROR, "Operand should contain 2 column(s)",
                                     "row quantified wide column count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_operand_columns,
                           "row quantified wide column count warning code");
    failures += expect_prepare_error(database, "SELECT (1) = ANY (SELECT a,b FROM pair_t)",
                                     MYLITE_EXEC_ERROR, "Operand should contain 1 column(s)",
                                     "scalar quantified row-looking column count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_operand_columns,
                           "scalar quantified row-looking warning code");
    failures += prepare_sql(database, "SELECT ROW(1) = ANY (SELECT a FROM pair_t)",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "single element ROW quantified rejected");
    failures += prepare_sql(database, "SELECT (10,1) <=> ANY (SELECT a,b FROM pair_t)",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "row null-safe quantified rejected");
    failures += expect_prepare_error(database, "SELECT (10,1) = ALL (SELECT a,b FROM pair_t)",
                                     MYLITE_EXEC_ERROR, "Operand should contain 1 column(s)",
                                     "row quantified equal all rejected");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_operand_columns,
                           "row quantified equal all warning code");
    failures += expect_prepare_error(database, "SELECT (10,1) <> ANY (SELECT a,b FROM pair_t)",
                                     MYLITE_EXEC_ERROR, "Operand should contain 1 column(s)",
                                     "row quantified not equal any rejected");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_operand_columns,
                           "row quantified not equal any warning code");
    failures += expect_prepare_error(database, "SELECT (10,1) != ANY (SELECT a,b FROM pair_t)",
                                     MYLITE_EXEC_ERROR, "Operand should contain 1 column(s)",
                                     "row quantified bang not equal any rejected");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_operand_columns,
                           "row quantified bang not equal any warning code");
    failures += expect_prepare_error(database, "SELECT (10,1) > ANY (SELECT a,b FROM pair_t)",
                                     MYLITE_EXEC_ERROR, "Operand should contain 1 column(s)",
                                     "row quantified greater any rejected");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_operand_columns,
                           "row quantified greater any warning code");
    failures += expect_prepare_error(database, "SELECT (10,1) >= ALL (SELECT a,b FROM pair_t)",
                                     MYLITE_EXEC_ERROR, "Operand should contain 1 column(s)",
                                     "row quantified greater equal all rejected");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_operand_columns,
                           "row quantified greater equal all warning code");
    failures += expect_prepare_error(database, "SELECT (10,1) < SOME (SELECT a,b FROM pair_t)",
                                     MYLITE_EXEC_ERROR, "Operand should contain 1 column(s)",
                                     "row quantified less some rejected");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_operand_columns,
                           "row quantified less some warning code");
    failures += expect_prepare_error(
        database, "SELECT (10,1) = ANY (SELECT a,b FROM pair_t LIMIT 1)", MYLITE_EXEC_ERROR,
        "LIMIT & IN/ALL/ANY/SOME subquery", "row quantified any limit");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_not_supported_yet,
                           "row quantified any limit warning code");
    failures += expect_prepare_error(database, "SELECT (10,1) = ANY (SELECT a FROM pair_t LIMIT 1)",
                                     MYLITE_EXEC_ERROR, "LIMIT & IN/ALL/ANY/SOME subquery",
                                     "row quantified limit before column count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_not_supported_yet,
                           "row quantified limit before column count warning code");
    failures += expect_prepare_error(
        database, "SELECT (10,1) > ANY (SELECT a,b FROM pair_t LIMIT 1)", MYLITE_EXEC_ERROR,
        "LIMIT & IN/ALL/ANY/SOME subquery", "row quantified unsupported limit precedence");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_not_supported_yet,
                           "row quantified unsupported limit precedence warning code");
    failures += expect_prepare_error(
        database, "SELECT (10,1) <> ALL (SELECT a,b FROM pair_t LIMIT 1)", MYLITE_EXEC_ERROR,
        "LIMIT & IN/ALL/ANY/SOME subquery", "row quantified all limit");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_not_supported_yet,
                           "row quantified all limit warning code");
    failures += expect_prepare_error(database,
                                     "SELECT DISTINCT id FROM row_quant_outer_t "
                                     "ORDER BY (val,grp) = ANY (SELECT a,b FROM pair_t)",
                                     MYLITE_EXEC_ERROR,
                                     "Expression #1 of ORDER BY clause is not in SELECT list",
                                     "row quantified distinct hidden order column");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_field_in_order_not_select,
                   "row quantified distinct hidden order warning code");
    failures += prepare_sql(database,
                            "SELECT id FROM outer_in_t "
                            "WHERE val IN (SELECT n FROM set_in_t WHERE n=outer_in_t.val)",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "correlated in subquery deferred");
    failures += prepare_sql(database,
                            "SELECT id FROM outer_in_t "
                            "WHERE val IN (SELECT n FROM set_in_t WHERE n=val)",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "unqualified correlated in subquery deferred");
    failures += prepare_sql(database,
                            "SELECT id FROM row_quant_outer_t AS o "
                            "WHERE (o.val,o.grp) = ANY "
                            "(SELECT a,b FROM pair_t WHERE b=o.grp)",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "correlated row quantified subquery deferred");
    failures += prepare_sql(database,
                            "SELECT id FROM row_quant_outer_t "
                            "WHERE (val,grp) = ANY (SELECT a,b FROM pair_t WHERE b=grp)",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "unqualified correlated row quantified deferred");
    failures += prepare_sql(database,
                            "SELECT id FROM outer_in_t "
                            "WHERE (val,grp) IN "
                            "(SELECT a,b FROM pair_t WHERE b=outer_in_t.grp)",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "correlated row in subquery deferred");
    failures += prepare_sql(database,
                            "SELECT id FROM outer_in_t "
                            "WHERE (val,grp) IN (SELECT a,b FROM pair_t WHERE b=grp)",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "unqualified correlated row in subquery deferred");
    failures += prepare_sql(database,
                            "SELECT id FROM outer_in_t "
                            "WHERE val > ANY (SELECT n FROM set_in_t WHERE n=outer_in_t.val)",
                            MYLITE_UNSUPPORTED, &stmt);
    failures += expect_no_stmt_handle(&stmt, "correlated quantified subquery deferred");
    failures += prepare_sql(database,
                            "UPDATE outer_in_t SET val = 1 "
                            "WHERE val = ANY (SELECT n FROM set_in_t)",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Unsupported UPDATE clause",
                                  "dml quantified subquery deferred");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database,
                            "UPDATE outer_in_t SET val = 1 "
                            "WHERE (val,grp) IN (SELECT a,b FROM pair_t)",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Unsupported UPDATE clause",
                                  "dml row in subquery deferred");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database,
                            "UPDATE row_quant_outer_t SET val = 1 "
                            "WHERE (val,grp) = ANY (SELECT a,b FROM pair_t)",
                            MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Unsupported UPDATE clause",
                                  "dml row quantified subquery deferred");
    mylite_finalize(stmt);
    stmt = NULL;

    mylite_close(database);
    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_select_where_execution(void)
{
    static const char *const id_column[] = {"id"};
    static const char *const all_ids[] = {"1", "2", "3", "4"};
    static const char *const id_2[] = {"2"};
    static const char *const id_3[] = {"3"};
    static const char *const id_4[] = {"4"};
    static const char *const ids_1_2[] = {"1", "2"};
    static const char *const ids_1_2_3[] = {"1", "2", "3"};
    static const char *const ids_1_3[] = {"1", "3"};
    static const char *const ids_2_3[] = {"2", "3"};
    static const char *const metadata_columns[] = {"x", "s"};
    static const char *const metadata_values[] = {"0", "alpha", "1", "beta"};
    static const struct expected_column_metadata metadata[] = {
        {"x", "mylite_task17_where", "t", "t", "n"},
        {"s", "mylite_task17_where", "t", "t", "s"},
    };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    uint64_t last_insert_id = 0U;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open where database");
    failures += execute_sql(database, "CREATE DATABASE mylite_task17_where", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE mylite_task17_where.t ("
                            "id INT PRIMARY KEY, n INT, s VARCHAR(20), z VARCHAR(20), "
                            "nullable INT NULL, CamelCase INT)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO mylite_task17_where.t VALUES "
                            "(1, 0, 'alpha', '2', NULL, 10), "
                            "(2, 1, 'beta', '2a', 5, 20), "
                            "(3, 2, 'ALPHA', 'a', NULL, 30), "
                            "(4, NULL, 'gamma', '10', 0, 40)",
                            MYLITE_DONE);
    last_insert_id = mylite_last_insert_id(database);
    failures += execute_sql(database, "USE mylite_task17_where", MYLITE_DONE);

    failures += expect_select_rows(database, "SELECT id FROM t WHERE 1", id_column, 1, all_ids, 4,
                                   "where constant true");
    failures += expect_int(mylite_warning_count(database), 0, "where constant true warnings");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE 0", id_column, 1, NULL, 0,
                                   "where constant false");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE NULL", id_column, 1, NULL, 0,
                                   "where constant null");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE 'abc'", id_column, 1, NULL, 0,
                                   "where string truthiness");
    failures += expect_int(mylite_warning_count(database), 1, "where string warning count");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_truncated_wrong_value, "where string warning code");
    failures +=
        expect_contains(mylite_warning_message(database, 0), "abc", "where string warning message");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE n", id_column, 1, ids_2_3, 2,
                                   "where column truthiness");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE nullable", id_column, 1, id_2,
                                   1, "where nullable truthiness");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE nullable IS NULL", id_column,
                                   1, ids_1_3, 2, "where is null");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE nullable <=> NULL", id_column,
                                   1, ids_1_3, 2, "where null-safe equality");

    failures += expect_select_rows(database, "SELECT id FROM t WHERE t.n = 1", id_column, 1, id_2,
                                   1, "where table-qualified column");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE mylite_task17_where.t.n = 2",
                                   id_column, 1, id_3, 1, "where schema-qualified column");
    failures += expect_select_rows(database, "SELECT id FROM t AS tt WHERE tt.n = 1", id_column, 1,
                                   id_2, 1, "where alias-qualified column");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE camelcase = 20", id_column, 1,
                                   id_2, 1, "where case-insensitive column");
    failures += expect_prepare_error(database, "SELECT id FROM t AS tt WHERE t.n = 1",
                                     MYLITE_EXEC_ERROR, "Unknown column 't.n' in 'where clause'",
                                     "where alias hides base table");
    failures += expect_int(mylite_warning_count(database), 1, "where alias error warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown_column,
                           "where alias error warning code");
    failures += expect_prepare_error(database, "SELECT n AS x FROM t WHERE x = 1",
                                     MYLITE_EXEC_ERROR, "Unknown column 'x' in 'where clause'",
                                     "where projection alias invisible");
    failures += expect_prepare_error(
        database, "SELECT id FROM t WHERE missing_col = 1", MYLITE_EXEC_ERROR,
        "Unknown column 'missing_col' in 'where clause'", "where missing column");
    failures += expect_prepare_error(
        database, "SELECT id FROM t WHERE missing_alias.n = 1", MYLITE_EXEC_ERROR,
        "Unknown column 'missing_alias.n' in 'where clause'", "where missing qualifier");

    failures += expect_select_rows(database, "SELECT id FROM t WHERE n BETWEEN 1 AND 2", id_column,
                                   1, ids_2_3, 2, "where between");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE n NOT BETWEEN 3 AND NULL",
                                   id_column, 1, ids_1_2_3, 3, "where not between null");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE s LIKE 'alpha'", id_column, 1,
                                   ids_1_3, 2, "where like");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE nullable NOT LIKE '5'",
                                   id_column, 1, id_4, 1, "where not like");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE n IN (1, 2, NULL)", id_column,
                                   1, ids_2_3, 2, "where in");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE n NOT IN (1, 2, NULL)",
                                   id_column, 1, NULL, 0, "where not in null");
    failures += prepare_sql(database, "SELECT id FROM t WHERE n IN ()", MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "where empty in list");

    failures += expect_select_rows(database, "SELECT id FROM t WHERE z = 2", id_column, 1, ids_1_2,
                                   2, "where conversion equality");
    failures += expect_int(mylite_warning_count(database), 2, "where conversion warning count");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_truncated_wrong_value, "where conversion warning code 0");
    failures += expect_contains(mylite_warning_message(database, 0), "2a",
                                "where conversion warning message 0");
    failures += expect_int((int)mylite_warning_code(database, 1),
                           mysql_warning_truncated_wrong_value, "where conversion warning code 1");
    failures += expect_contains(mylite_warning_message(database, 1), "a",
                                "where conversion warning message 1");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE z < 2", id_column, 1, id_3, 1,
                                   "where conversion less");
    failures += expect_int(mylite_warning_count(database), 2, "where less warning count");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE 1 / 0", id_column, 1, NULL, 0,
                                   "where division by zero");
    failures += expect_int(mylite_warning_count(database), 1, "where division warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_division_by_zero,
                           "where division warning code");
    failures += expect_string(mylite_warning_message(database, 0), "Division by 0",
                              "where division warning message");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE 1 / 0 OR 1", id_column, 1,
                                   all_ids, 4, "where constant division or true");
    failures +=
        expect_int(mylite_warning_count(database), 1, "where constant division or true warnings");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE 1 / 0 AND 0", id_column, 1,
                                   NULL, 0, "where constant division and false");
    failures +=
        expect_int(mylite_warning_count(database), 1, "where constant division and false warnings");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE 1 / 0 OR n = 1", id_column, 1,
                                   id_2, 1, "where cached constant division left or");
    failures += expect_int(mylite_warning_count(database), 1,
                           "where cached constant division left or warnings");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE n = 1 OR 1 / 0", id_column, 1,
                                   id_2, 1, "where cached constant division right or");
    failures += expect_int(mylite_warning_count(database), 1,
                           "where cached constant division right or warnings");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE 1 / 0 AND n = 1", id_column, 1,
                                   NULL, 0, "where cached constant division left and");
    failures += expect_int(mylite_warning_count(database), 1,
                           "where cached constant division left and warnings");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE n = 1 AND 1 / 0", id_column, 1,
                                   NULL, 0, "where cached constant division right and");
    failures += expect_int(mylite_warning_count(database), 1,
                           "where cached constant division right and warnings");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE n = 1 AND z = 2", id_column, 1,
                                   id_2, 1, "where and short circuit");
    failures += expect_int(mylite_warning_count(database), 1, "where and warning count");
    failures +=
        expect_contains(mylite_warning_message(database, 0), "2a", "where and warning message");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE z = 2 OR n = 1", id_column, 1,
                                   ids_1_2, 2, "where left or conversion");
    failures += expect_int(mylite_warning_count(database), 2, "where left or warning count");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE n = 1 OR z = 2", id_column, 1,
                                   ids_1_2, 2, "where right or conversion");
    failures += expect_int(mylite_warning_count(database), 1, "where right or warning count");
    failures +=
        expect_contains(mylite_warning_message(database, 0), "a", "where right or warning message");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE 0 AND z = 2", id_column, 1,
                                   NULL, 0, "where constant false and");
    failures += expect_int(mylite_warning_count(database), 0, "where constant false and warnings");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE z = 2 AND 0", id_column, 1,
                                   NULL, 0, "where right constant false and");
    failures +=
        expect_int(mylite_warning_count(database), 0, "where right constant false and warnings");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE 1 OR z = 2", id_column, 1,
                                   all_ids, 4, "where constant true or");
    failures += expect_int(mylite_warning_count(database), 0, "where constant true or warnings");
    failures += expect_select_rows(database, "SELECT id FROM t WHERE z = 2 OR 1", id_column, 1,
                                   all_ids, 4, "where right constant true or");
    failures +=
        expect_int(mylite_warning_count(database), 0, "where right constant true or warnings");

    failures +=
        prepare_sql(database, "SELECT id FROM t WHERE s LIKE 'a%' ESCAPE 'xx'", MYLITE_OK, &stmt);
    failures += expect_int(mylite_warning_count(database), 0, "where escape before step warnings");
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "where invalid escape step");
    failures += expect_contains(mylite_error_message(database), "Incorrect arguments to ESCAPE",
                                "where invalid escape error");
    failures += expect_int(mylite_warning_count(database), 1, "where escape warning count");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_incorrect_escape_arguments, "where escape warning");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT id FROM t WHERE s LIKE 'a%' ESCAPE 'xx' OR 1",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR,
                              "where invalid escape before constant or");
    failures += expect_contains(mylite_error_message(database), "Incorrect arguments to ESCAPE",
                                "where invalid escape before constant or error");
    failures += expect_int(mylite_warning_count(database), 1,
                           "where invalid escape before constant or warning count");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_incorrect_escape_arguments,
                   "where invalid escape before constant or warning");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT id FROM t WHERE s LIKE 'a%' ESCAPE 'xx' AND 0",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR,
                              "where invalid escape before constant and");
    failures += expect_contains(mylite_error_message(database), "Incorrect arguments to ESCAPE",
                                "where invalid escape before constant and error");
    failures += expect_int(mylite_warning_count(database), 1,
                           "where invalid escape before constant and warning count");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_incorrect_escape_arguments,
                   "where invalid escape before constant and warning");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT id FROM t WHERE 1 OR s LIKE 'a%' ESCAPE 'xx'",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR,
                              "where invalid escape after constant or");
    failures += expect_contains(mylite_error_message(database), "Incorrect arguments to ESCAPE",
                                "where invalid escape after constant or error");
    failures += expect_int(mylite_warning_count(database), 1,
                           "where invalid escape after constant or warning count");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_incorrect_escape_arguments,
                   "where invalid escape after constant or warning");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT id FROM t WHERE 0 AND s LIKE 'a%' ESCAPE 'xx'",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR,
                              "where invalid escape after constant and");
    failures += expect_contains(mylite_error_message(database), "Incorrect arguments to ESCAPE",
                                "where invalid escape after constant and error");
    failures += expect_int(mylite_warning_count(database), 1,
                           "where invalid escape after constant and warning count");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_incorrect_escape_arguments,
                   "where invalid escape after constant and warning");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        expect_select_rows(database, "SELECT id FROM t WHERE s LIKE 'a%' ESCAPE (1 / 0) OR 1",
                           id_column, 1, all_ids, 4, "where escaped null before constant or");
    failures += expect_int(mylite_warning_count(database), 1,
                           "where escaped null before constant or warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_division_by_zero,
                           "where escaped null before constant or warning");
    failures +=
        expect_select_rows(database, "SELECT id FROM t WHERE 1 OR s LIKE 'a%' ESCAPE (1 / 0)",
                           id_column, 1, all_ids, 4, "where escaped null after constant or");
    failures += expect_int(mylite_warning_count(database), 1,
                           "where escaped null after constant or warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_division_by_zero,
                           "where escaped null after constant or warning");
    failures +=
        expect_select_rows(database, "SELECT id FROM t WHERE 0 AND s LIKE 'a%' ESCAPE (1 / 0)",
                           id_column, 1, NULL, 0, "where escaped null after constant and");
    failures += expect_int(mylite_warning_count(database), 1,
                           "where escaped null after constant and warning count");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_division_by_zero,
                           "where escaped null after constant and warning");

    failures += prepare_sql(database, "SELECT n AS x, s FROM t WHERE z = 2", MYLITE_OK, &stmt);
    failures += expect_column_names(stmt, metadata_columns, 2, "where metadata names");
    failures += expect_column_metadata(stmt, metadata, 2, "where metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "where metadata first row");
    failures += expect_string(mylite_column_text(stmt, 0), metadata_values[0],
                              "where metadata first value");
    failures += expect_string(mylite_column_text(stmt, 1), metadata_values[1],
                              "where metadata second value");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "where metadata second row");
    failures += expect_string(mylite_column_text(stmt, 0), metadata_values[2],
                              "where metadata third value");
    failures += expect_string(mylite_column_text(stmt, 1), metadata_values[3],
                              "where metadata fourth value");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "where metadata done");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "where affected rows");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT 1", MYLITE_OK, &stmt);
    failures += expect_int(mylite_warning_count(database), 0, "where warnings cleared on prepare");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "where warning lifecycle row");
    failures += expect_int(mylite_warning_count(database), 0, "where warnings cleared after row");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += expect_int64((int64_t)mylite_last_insert_id(database), (int64_t)last_insert_id,
                             "where last insert id unchanged");
    mylite_close(database);
    return failures;
}

static int test_select_order_limit_offset_execution(void)
{
    enum {
        full_order_row_count = 5,
        hidden_order_warning_count = 5,
    };
    static const char *const id_column[] = {"id"};
    static const char *const ids_1_2[] = {"1", "2"};
    static const char *const ids_2_3[] = {"2", "3"};
    static const char *const ids_2_4[] = {"2", "4"};
    static const char *const ids_3_4_5[] = {"3", "4", "5"};
    static const char *const ids_where_order_limit[] = {"4", "2", "3"};
    static const char *const ids_expression_order[] = {"2", "3", "1"};
    static const char *const ids_nulls_asc[] = {"4", "5"};
    static const char *const ids_nulls_desc[] = {"2", "1"};
    static const char *const ids_string_order[] = {"2", "3", "1"};
    static const char *const ids_base_qualified[] = {"4", "5", "1"};
    static const char *const ids_qualified_alias[] = {"5", "4", "3"};
    static const char *const id_s_columns[] = {"id", "s"};
    static const char *const alias_wins_values[] = {
        "-5", "delta", "-4", "gamma", "-3", "alpha",
    };
    static const char *const s_id_columns[] = {"s", "id"};
    static const char *const ordinal_values[] = {
        "delta", "5", "gamma", "4", "alpha", "3",
    };
    static const char *const sort_key_columns[] = {"Sort_Key", "id"};
    static const char *const sort_key_values[] = {
        "20", "2", "10", "1", "10", "3",
    };
    static const char *const quoted_sort_key_columns[] = {"sort key", "id"};
    static const char *const quoted_sort_key_values[] = {
        "20", "2", "10", "1", "10", "3",
    };
    static const char *const string_literal_order_values[] = {
        "10", "1", "20", "2", "10", "3",
    };
    static const char *const x_id_columns[] = {"x", "id"};
    static const char *const order_expression_alias_values[] = {
        "20",
        "2",
        "10",
        "1",
    };
    static const char *const x_column[] = {"x"};
    static const char *const one_column[] = {"1"};
    static const char *const id_plus_order_values[] = {"2", "3", "4", "5", "6"};
    static const char *const literal_order_values[] = {"1", "1", "1", "1", "1"};
    static const char *const metadata_columns[] = {"x", "s"};
    static const struct expected_column_metadata metadata[] = {
        {"x", "mylite_task18_order", "t", "t", "n"},
        {"s", "mylite_task18_order", "t", "t", "s"},
    };
    static const struct expected_column_metadata id_metadata[] = {
        {"id", "mylite_task18_order", "t", "t", "id"},
    };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    uint64_t last_insert_id = 0U;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open order database");
    failures += execute_sql(database, "CREATE DATABASE mylite_task18_order", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE mylite_task18_order.t ("
                            "id INT PRIMARY KEY, category INT, n INT, s VARCHAR(20), "
                            "nullable INT NULL, CamelCase INT)",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO mylite_task18_order.t VALUES "
                            "(1, 2, 10, 'beta', NULL, 100), "
                            "(2, 1, 20, 'Alpha', 5, 200), "
                            "(3, 2, 10, 'alpha', NULL, 300), "
                            "(4, 1, NULL, 'gamma', 0, 400), "
                            "(5, 3, 1, 'delta', 7, 500)",
                            MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_task18_order", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE ai (id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, v INT) "
                            "AUTO_INCREMENT=10",
                            MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO ai (v) VALUES (7)", MYLITE_DONE);
    last_insert_id = mylite_last_insert_id(database);

    failures += expect_select_rows(database, "SELECT id FROM t ORDER BY id LIMIT 2", id_column, 1,
                                   ids_1_2, 2, "order limit row count");
    failures += expect_select_rows(database, "SELECT id FROM t ORDER BY id LIMIT 1, 2", id_column,
                                   1, ids_2_3, 2, "comma limit offset");
    failures += expect_select_rows(database, "SELECT id FROM t ORDER BY id LIMIT 2 OFFSET 1",
                                   id_column, 1, ids_2_3, 2, "keyword limit offset");
    failures +=
        expect_select_rows(database, "SELECT id FROM t ORDER BY id LIMIT 2, 18446744073709551615",
                           id_column, 1, ids_3_4_5, 3, "max unsigned row count limit");
    failures += expect_select_row_count(database, "SELECT id FROM t LIMIT 2", 2,
                                        "limit without order row count");

    failures += expect_select_rows(
        database, "SELECT id FROM t WHERE category IN (1, 2) ORDER BY category, id DESC LIMIT 3",
        id_column, 1, ids_where_order_limit, 3, "where order limit interaction");
    failures += expect_select_rows(database, "SELECT id FROM t ORDER BY n + id DESC, id LIMIT 3",
                                   id_column, 1, ids_expression_order, 3, "expression order keys");
    failures += expect_select_rows(database, "SELECT id FROM t ORDER BY n ASC, id LIMIT 2",
                                   id_column, 1, ids_nulls_asc, 2, "null ascending order");
    failures += expect_select_rows(database, "SELECT id FROM t ORDER BY n DESC, id LIMIT 2",
                                   id_column, 1, ids_nulls_desc, 2, "null descending order");
    failures += expect_select_rows(database, "SELECT id FROM t ORDER BY s, id LIMIT 3", id_column,
                                   1, ids_string_order, 3, "string order tie breaker");

    failures +=
        expect_select_rows(database, "SELECT -id AS id, s FROM t ORDER BY id LIMIT 3", id_s_columns,
                           2, alias_wins_values, 3, "order alias wins over column");
    failures += expect_select_rows(database, "SELECT id FROM t ORDER BY t.n ASC, id LIMIT 3",
                                   id_column, 1, ids_base_qualified, 3, "qualified order column");
    failures += expect_select_rows(
        database, "SELECT n AS Sort_Key, id FROM t ORDER BY sort_key DESC, id LIMIT 3",
        sort_key_columns, 2, sort_key_values, 3, "case-insensitive order alias");
    failures += expect_select_rows(
        database, "SELECT n AS 'sort key', id FROM t ORDER BY `sort key` DESC, id LIMIT 3",
        quoted_sort_key_columns, 2, quoted_sort_key_values, 3, "quoted order alias");
    failures += expect_select_rows(
        database, "SELECT n AS 'sort key', id FROM t ORDER BY 'sort key' DESC, id LIMIT 3",
        quoted_sort_key_columns, 2, string_literal_order_values, 3, "string literal order key");
    failures += expect_select_rows(
        database, "SELECT n AS x, id FROM t ORDER BY x + 1 DESC, id LIMIT 2", x_id_columns, 2,
        order_expression_alias_values, 2, "order expression alias reference");
    failures += expect_select_rows(database, "SELECT s, id FROM t ORDER BY 2 DESC LIMIT 3",
                                   s_id_columns, 2, ordinal_values, 3, "ordinal order key");
    failures += expect_select_rows(database, "SELECT id FROM t ORDER BY -1, id LIMIT 2", id_column,
                                   1, ids_1_2, 2, "negative constant order key");
    failures +=
        expect_select_rows(database, "SELECT id FROM t AS tt ORDER BY tt.CamelCase DESC LIMIT 3",
                           id_column, 1, ids_qualified_alias, 3, "qualified alias order key");
    failures += expect_select_rows(database, "SELECT id + 1 AS x FROM t ORDER BY id", x_column, 1,
                                   id_plus_order_values, full_order_row_count,
                                   "unreferenced order projection expression");
    failures += expect_select_rows(database, "SELECT 1 FROM t ORDER BY id", one_column, 1,
                                   literal_order_values, full_order_row_count,
                                   "literal projection with order");

    failures += expect_prepare_error(
        database, "SELECT n AS x, category AS x FROM t ORDER BY x LIMIT 1", MYLITE_EXEC_ERROR,
        "Column 'x' in order clause is ambiguous", "duplicate order alias");
    failures += expect_int(mylite_warning_count(database), 1, "duplicate order alias warning");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_ambiguous_column,
                           "duplicate order alias warning code");
    failures += expect_prepare_error(
        database, "SELECT id FROM t ORDER BY missing_col", MYLITE_EXEC_ERROR,
        "Unknown column 'missing_col' in 'order clause'", "unknown order column");
    failures += expect_int((int)mylite_warning_code(database, 0), mysql_warning_unknown_column,
                           "unknown order column warning code");
    failures += expect_prepare_error(
        database, "SELECT id FROM t ORDER BY missing_alias.n", MYLITE_EXEC_ERROR,
        "Unknown column 'missing_alias.n' in 'order clause'", "unknown order qualifier");
    failures += expect_prepare_error(database, "SELECT id FROM t AS tt ORDER BY t.n",
                                     MYLITE_EXEC_ERROR, "Unknown column 't.n' in 'order clause'",
                                     "order alias hides base qualifier");
    failures += expect_prepare_error(database, "SELECT n AS x, id FROM t ORDER BY t.x",
                                     MYLITE_EXEC_ERROR, "Unknown column 't.x' in 'order clause'",
                                     "qualified order alias rejected");
    failures += expect_prepare_error(database, "SELECT id FROM t ORDER BY 0", MYLITE_EXEC_ERROR,
                                     "Unknown column '0' in 'order clause'", "zero order ordinal");
    failures +=
        expect_prepare_error(database, "SELECT id FROM t ORDER BY 2", MYLITE_EXEC_ERROR,
                             "Unknown column '2' in 'order clause'", "out of range order ordinal");

    failures += expect_select_rows(database, "SELECT id FROM t ORDER BY s + 0, id LIMIT 2",
                                   id_column, 1, ids_1_2, 2, "hidden order warnings");
    failures += expect_int(mylite_warning_count(database), hidden_order_warning_count,
                           "hidden order warning count");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_truncated_wrong_value, "hidden order warning code");
    failures += expect_select_rows(database,
                                   "SELECT id FROM t WHERE category = 1 ORDER BY s + 0, id LIMIT 2",
                                   id_column, 1, ids_2_4, 2, "filtered hidden order warnings");
    failures += expect_int(mylite_warning_count(database), 2, "filtered hidden warning count");
    failures +=
        expect_select_rows(database, "SELECT id FROM t WHERE category = 99 ORDER BY s + 0 LIMIT 2",
                           id_column, 1, NULL, 0, "empty filtered hidden order warnings");
    failures += expect_int(mylite_warning_count(database), 0, "empty filtered warning count");

    failures += prepare_sql(
        database,
        "SELECT n AS x, s FROM t WHERE category IN (1, 2) ORDER BY nullable DESC, id LIMIT 2",
        MYLITE_OK, &stmt);
    failures += expect_column_names(stmt, metadata_columns, 2, "hidden order metadata names");
    failures += expect_column_metadata(stmt, metadata, 2, "hidden order metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "hidden order metadata first row");
    failures += expect_string(mylite_column_text(stmt, 0), "20", "hidden order metadata first x");
    failures +=
        expect_string(mylite_column_text(stmt, 1), "Alpha", "hidden order metadata first s");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "hidden order metadata second row");
    failures += expect_null_text(mylite_column_text(stmt, 0), "hidden order metadata second x");
    failures +=
        expect_string(mylite_column_text(stmt, 1), "gamma", "hidden order metadata second s");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "hidden order metadata done");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "hidden order affected rows");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT id FROM t ORDER BY id LIMIT 0", MYLITE_OK, &stmt);
    failures += expect_column_names(stmt, id_column, 1, "limit zero metadata names");
    failures += expect_column_metadata(stmt, id_metadata, 1, "limit zero metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "limit zero done");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "limit zero affected rows");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT id FROM t LIMIT -1", MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "negative limit parse error");
    failures += prepare_sql(database, "SELECT id FROM t LIMIT 1.5", MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "decimal limit parse error");
    failures += prepare_sql(database, "SELECT id FROM t LIMIT '2'", MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "string limit parse error");
    failures += prepare_sql(database, "SELECT id FROM t LIMIT NULL", MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "null limit parse error");
    failures += prepare_sql(database, "SELECT id FROM t LIMIT 1 + 1", MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "expression limit parse error");
    failures += prepare_sql(database, "SELECT id FROM t LIMIT 18446744073709551616",
                            MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "overflow limit parse error");
    failures += prepare_sql(database, "SELECT id FROM t LIMIT ?", MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "parameter limit parse error");

    failures += expect_int64((int64_t)mylite_last_insert_id(database), (int64_t)last_insert_id,
                             "order last insert id unchanged");
    mylite_close(database);
    return failures;
}

static int test_result_metadata_expression_labels_execution(void)
{
    static const char *const wildcard_columns[] = {
        "id", "n", "u", "s", "c", "txt", "b", "d", "r", "dt", "ts", "y",
    };
    static const struct expected_result_metadata base_metadata[] = {
        {"label", "mylite_task23_metadata", "tt", "mylite_task23_metadata", "t", "id", 11U,
         MYLITE_FIELD_TYPE_LONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_PRI_KEY | MYLITE_FIELD_FLAG_AUTO_INCREMENT |
             MYLITE_FIELD_FLAG_PART_KEY | MYLITE_FIELD_FLAG_NUM,
         0U, 0},
        {"label", "mylite_task23_metadata", "tt", "mylite_task23_metadata", "t", "n", 11U,
         MYLITE_FIELD_TYPE_LONG, 0U, 63U,
         MYLITE_FIELD_FLAG_MULTIPLE_KEY | MYLITE_FIELD_FLAG_PART_KEY | MYLITE_FIELD_FLAG_NUM,
         MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"u_alias", "mylite_task23_metadata", "tt", "mylite_task23_metadata", "t", "u", 10U,
         MYLITE_FIELD_TYPE_LONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_NUM |
             MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
         0U, 0},
        {"s_alias", "mylite_task23_metadata", "tt", "mylite_task23_metadata", "t", "s", 48U,
         MYLITE_FIELD_TYPE_VAR_STRING, 0U, 255U, 0U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY, 1},
        {"c", "mylite_task23_metadata", "tt", "mylite_task23_metadata", "t", "c", 12U,
         MYLITE_FIELD_TYPE_STRING, 0U, 255U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE, 0U, 0},
        {"txt", "mylite_task23_metadata", "tt", "mylite_task23_metadata", "t", "txt", 65535U,
         MYLITE_FIELD_TYPE_BLOB, 0U, 255U, MYLITE_FIELD_FLAG_BLOB,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY, 1},
        {"b", "mylite_task23_metadata", "tt", "mylite_task23_metadata", "t", "b", 8U,
         MYLITE_FIELD_TYPE_VAR_STRING, 0U, 63U, MYLITE_FIELD_FLAG_BINARY,
         MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"d", "mylite_task23_metadata", "tt", "mylite_task23_metadata", "t", "d", 8U,
         MYLITE_FIELD_TYPE_NEWDECIMAL, 2U, 63U, MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL,
         1},
        {"r", "mylite_task23_metadata", "tt", "mylite_task23_metadata", "t", "r", 22U,
         MYLITE_FIELD_TYPE_DOUBLE, 31U, 63U, MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"dt", "mylite_task23_metadata", "tt", "mylite_task23_metadata", "t", "dt", 23U,
         MYLITE_FIELD_TYPE_DATETIME, 3U, 63U, MYLITE_FIELD_FLAG_BINARY, MYLITE_FIELD_FLAG_NOT_NULL,
         1},
        {"ts", "mylite_task23_metadata", "tt", "mylite_task23_metadata", "t", "ts", 19U,
         MYLITE_FIELD_TYPE_TIMESTAMP, 0U, 63U, MYLITE_FIELD_FLAG_BINARY, MYLITE_FIELD_FLAG_NOT_NULL,
         1},
        {"y", "mylite_task23_metadata", "tt", "mylite_task23_metadata", "t", "y", 4U,
         MYLITE_FIELD_TYPE_YEAR, 0U, 63U,
         MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_ZEROFILL | MYLITE_FIELD_FLAG_NUM,
         MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"hidden_alias", "mylite_task23_metadata", "tt", "mylite_task23_metadata", "t", "hidden",
         11U, MYLITE_FIELD_TYPE_LONG, 0U, 63U, MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL,
         1},
    };
    static const struct expected_result_metadata scalar_metadata[] = {
        {"1", NULL, NULL, NULL, NULL, NULL, 2U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
        {"nil", NULL, NULL, NULL, NULL, NULL, 0U, MYLITE_FIELD_TYPE_NULL, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"str_lit", NULL, NULL, NULL, NULL, NULL, 12U, MYLITE_FIELD_TYPE_VAR_STRING, 31U, 255U,
         MYLITE_FIELD_FLAG_NOT_NULL, MYLITE_FIELD_FLAG_BINARY, 0},
        {"sum_expr", NULL, NULL, NULL, NULL, NULL, 3U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
        {"slash_expr", NULL, NULL, NULL, NULL, NULL, 7U, MYLITE_FIELD_TYPE_NEWDECIMAL, 4U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
        {"div_expr", NULL, NULL, NULL, NULL, NULL, 2U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
        {"eq_expr", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
        {"is_expr", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
        {"bit_expr", NULL, NULL, NULL, NULL, NULL, 20U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_BINARY |
             MYLITE_FIELD_FLAG_NUM,
         0U, 0},
    };
    static const struct expected_result_metadata table_expression_metadata[] = {
        {"n_plus", NULL, NULL, NULL, NULL, NULL, 12U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"u_plus", NULL, NULL, NULL, NULL, NULL, 11U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_BINARY |
             MYLITE_FIELD_FLAG_NUM,
         0U, 0},
        {"n_is_null", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
        {"nullsafe", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, 0U, 0},
        {"n_in", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"n_between", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
        {"s_like", NULL, NULL, NULL, NULL, NULL, 1U, MYLITE_FIELD_TYPE_LONGLONG, 0U, 63U,
         MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM, MYLITE_FIELD_FLAG_NOT_NULL, 1},
    };
    static const struct expected_result_metadata hidden_order_metadata[] = {
        {"id", "mylite_task23_metadata", "t", "mylite_task23_metadata", "t", "id", 11U,
         MYLITE_FIELD_TYPE_LONG, 0U, 63U,
         MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_PRI_KEY | MYLITE_FIELD_FLAG_AUTO_INCREMENT |
             MYLITE_FIELD_FLAG_PART_KEY | MYLITE_FIELD_FLAG_NUM,
         0U, 0},
    };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open metadata database");
    failures += execute_sql(database,
                            "CREATE DATABASE mylite_task23_metadata DEFAULT CHARACTER SET utf8mb4",
                            MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_task23_metadata", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE t ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
                            "n INT NULL, "
                            "u INT UNSIGNED NOT NULL, "
                            "s VARCHAR(12) NULL, "
                            "c CHAR(3) NOT NULL, "
                            "txt TEXT, "
                            "b VARBINARY(8), "
                            "d DECIMAL(6,2), "
                            "r DOUBLE, "
                            "dt DATETIME(3), "
                            "ts TIMESTAMP NULL, "
                            "y YEAR, "
                            "hidden INT INVISIBLE, "
                            "KEY n_idx(n))",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO t (n,u,s,c,txt,b,d,r,dt,ts,y,hidden) VALUES "
                            "(NULL, 5, 'abc', 'xy', 'body', 'abcd', 12.30, 2.5, "
                            "'2024-01-02 03:04:05.123', NULL, 2024, 9)",
                            MYLITE_DONE);

    failures += prepare_sql(database,
                            "SELECT id AS label, n AS label, u AS u_alias, s AS s_alias, c, "
                            "txt, b, d, r, dt, ts, y, hidden AS hidden_alias "
                            "FROM t AS tt LIMIT 0",
                            MYLITE_OK, &stmt);
    failures += expect_result_metadata(stmt, base_metadata,
                                       (int)(sizeof(base_metadata) / sizeof(base_metadata[0])),
                                       "base result metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "base metadata limit zero");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT * FROM t LIMIT 0", MYLITE_OK, &stmt);
    failures += expect_column_names(stmt, wildcard_columns,
                                    (int)(sizeof(wildcard_columns) / sizeof(wildcard_columns[0])),
                                    "wildcard invisible metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "wildcard metadata limit zero");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "SELECT 1, NULL AS nil, 'abc' AS str_lit, 1 + 2 AS sum_expr, "
                            "5 / 2 AS slash_expr, 5 DIV 2 AS div_expr, 1 = 1 AS eq_expr, "
                            "NULL IS NULL AS is_expr, 1 & 3 AS bit_expr",
                            MYLITE_OK, &stmt);
    failures += expect_result_metadata(stmt, scalar_metadata,
                                       (int)(sizeof(scalar_metadata) / sizeof(scalar_metadata[0])),
                                       "scalar result metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "scalar metadata row");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "scalar metadata done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "SELECT n + 1 AS n_plus, u + 1 AS u_plus, "
                            "n IS NULL AS n_is_null, n <=> NULL AS nullsafe, "
                            "n IN (1,2) AS n_in, n BETWEEN 1 AND 20 AS n_between, "
                            "s LIKE 'a%' AS s_like FROM t LIMIT 0",
                            MYLITE_OK, &stmt);
    failures += expect_result_metadata(
        stmt, table_expression_metadata,
        (int)(sizeof(table_expression_metadata) / sizeof(table_expression_metadata[0])),
        "table expression metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "table expression limit zero");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "SELECT id FROM t ORDER BY n + 1 LIMIT 0", MYLITE_OK, &stmt);
    failures += expect_result_metadata(stmt, hidden_order_metadata, 1, "hidden order metadata");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "hidden order limit zero");
    mylite_finalize(stmt);

    mylite_close(database);
    return failures;
}

static int test_update_single_table_execution(void)
{
    static const char *const ab_columns[] = {"a", "b"};
    static const char *const assignment_order_values[] = {"2", "2"};
    static const char *const repeated_values[] = {"101"};
    static const char *const default_columns[] = {"a", "nn"};
    static const char *const default_values[] = {"3", "7"};
    static const char *const camel_values[] = {"12"};
    static const char *const limited_columns[] = {"id", "s"};
    static const char *const limited_values[] = {
        "10", "alpha", "11", "beta", "12", "limited", "13", "limited",
    };
    static const char *const u_columns[] = {"id", "u"};
    static const char *const u_values[] = {
        "10", "1", "11", "2", "12", "3", "13", "4",
    };
    static const char *const shift_columns[] = {"id", "v"};
    static const char *const shift_values[] = {
        "2", "10", "3", "20", "4", "30",
    };
    static const char *const shift_fail_values[] = {
        "1", "10", "2", "20", "3", "30",
    };
    static const char *const ai_values[] = {"20", "1", "21", "3"};
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    uint64_t last_insert_id = 0U;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open memory database");

    failures += prepare_sql(database, "UPDATE t SET a = 1", MYLITE_OK, &stmt);
    failures += expect_int(mylite_column_count(stmt), 0, "update has no result columns");
    failures += expect_int64(mylite_affected_rows(stmt), 0, "prepared update affected rows");
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "update no database");
    failures +=
        expect_contains(mylite_error_message(database), "No database selected", "update no db");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "CREATE DATABASE mylite_task19_update", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_task19_update", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE t ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
                            "a INT DEFAULT 3, "
                            "b INT DEFAULT 4, "
                            "c INT NULL, "
                            "s VARCHAR(20), "
                            "u INT UNIQUE, "
                            "nn INT NOT NULL DEFAULT 7, "
                            "must INT NOT NULL, "
                            "CamelCase INT) AUTO_INCREMENT=10",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO t (a,b,c,s,u,nn,must,CamelCase) VALUES "
                            "(1,10,NULL,'alpha',1,7,100,11), "
                            "(2,20,5,'beta',2,7,200,22), "
                            "(3,30,NULL,'gamma',3,7,300,33), "
                            "(4,40,0,'delta',4,7,400,44)",
                            MYLITE_DONE);

    failures +=
        prepare_sql(database, "UPDATE t SET a = a + 1, b = a WHERE id = 10", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "update assignment order");
    failures += expect_int64(mylite_affected_rows(stmt), 1, "update assignment order affected");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT a, b FROM t WHERE id = 10", ab_columns, 2,
                                   assignment_order_values, 1, "update assignment order values");

    failures +=
        prepare_sql(database, "UPDATE t SET a = 100, a = a + 1 WHERE id = 11", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "update repeated target");
    failures += expect_int64(mylite_affected_rows(stmt), 1, "update repeated target affected");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT a FROM t WHERE id = 11", (const char *[]){"a"},
                                   1, repeated_values, 1, "update repeated target value");

    failures += prepare_sql(database, "UPDATE t SET a = a WHERE id IN (10, 11)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "update no-op");
    failures += expect_int64(mylite_affected_rows(stmt), 0, "update no-op affected rows");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "UPDATE t SET a = DEFAULT, c = DEFAULT, nn = DEFAULT WHERE id = 13",
                    MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "update defaults");
    failures += expect_int64(mylite_affected_rows(stmt), 1, "update defaults affected rows");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT a, nn FROM t WHERE id = 13", default_columns,
                                   2, default_values, 1, "update defaults values");
    failures += prepare_sql(database, "SELECT c FROM t WHERE id = 13", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_ROW, "update nullable default row");
    failures += expect_null_text(mylite_column_text(stmt, 0), "update nullable default value");
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "update nullable default done");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "UPDATE mylite_task19_update.t AS tt "
                            "SET tt.CamelCase = tt.CamelCase + 1 WHERE tt.id = 10",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "update alias qualified target");
    failures += expect_int64(mylite_affected_rows(stmt), 1, "update alias qualified affected");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT CamelCase FROM t WHERE id = 10",
                                   (const char *[]){"CamelCase"}, 1, camel_values, 1,
                                   "update alias qualified value");

    failures +=
        prepare_sql(database, "UPDATE t SET s = 'limited' WHERE id >= 10 ORDER BY id DESC LIMIT 2",
                    MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "update order limit");
    failures += expect_int64(mylite_affected_rows(stmt), 2, "update order limit affected rows");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT id, s FROM t ORDER BY id", limited_columns, 2,
                                   limited_values, 4, "update order limit values");

    failures += prepare_sql(database, "UPDATE t SET u = 1 WHERE id IN (11, 12)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "update duplicate rollback");
    failures += expect_contains(mylite_error_message(database), "Duplicate entry '1'",
                                "update duplicate error");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "update duplicate affected rows");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT id, u FROM t ORDER BY id", u_columns, 2,
                                   u_values, 4, "update duplicate rollback values");

    failures +=
        execute_sql(database, "CREATE TABLE shift_pk (id INT PRIMARY KEY, v INT)", MYLITE_DONE);
    failures +=
        execute_sql(database, "INSERT INTO shift_pk VALUES (1,10),(2,20),(3,30)", MYLITE_DONE);
    failures +=
        prepare_sql(database, "UPDATE shift_pk SET id = id + 1 ORDER BY id DESC", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "update ordered primary key shift");
    failures += expect_int64(mylite_affected_rows(stmt), 3, "ordered primary key shift affected");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        expect_select_rows(database, "SELECT id, v FROM shift_pk ORDER BY id", shift_columns, 2,
                           shift_values, 3, "ordered primary key shift values");

    failures += execute_sql(database, "CREATE TABLE shift_pk_fail (id INT PRIMARY KEY, v INT)",
                            MYLITE_DONE);
    failures +=
        execute_sql(database, "INSERT INTO shift_pk_fail VALUES (1,10),(2,20),(3,30)", MYLITE_DONE);
    failures += prepare_sql(database, "UPDATE shift_pk_fail SET id = id + 1", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "unordered primary key shift conflict");
    failures += expect_contains(mylite_error_message(database), "Duplicate entry '2'",
                                "unordered primary key shift conflict error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        expect_select_rows(database, "SELECT id, v FROM shift_pk_fail ORDER BY id", shift_columns,
                           2, shift_fail_values, 3, "unordered primary key shift rollback");

    failures += execute_sql(database,
                            "CREATE TABLE ai_update ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, v INT) "
                            "AUTO_INCREMENT=5",
                            MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO ai_update (v) VALUES (1),(2)", MYLITE_DONE);
    last_insert_id = mylite_last_insert_id(database);
    failures += prepare_sql(database, "UPDATE ai_update SET id = 20 WHERE v = 1", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "update auto increment explicit");
    failures += expect_int64(mylite_affected_rows(stmt), 1, "update auto increment affected");
    failures += expect_int64((int64_t)mylite_last_insert_id(database), (int64_t)last_insert_id,
                             "update leaves last insert id");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "INSERT INTO ai_update (v) VALUES (3)", MYLITE_DONE);
    failures += expect_select_rows(database,
                                   "SELECT id, v FROM ai_update WHERE v IN (1,3) "
                                   "ORDER BY v",
                                   (const char *[]){"id", "v"}, 2, ai_values, 2,
                                   "update auto increment next value");

    failures +=
        prepare_sql(database, "UPDATE t SET missing_col = 1 WHERE id = 10", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "update unknown target");
    failures += expect_contains(mylite_error_message(database),
                                "Unknown column 'missing_col' in 'field list'",
                                "update unknown target error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "UPDATE t AS tt SET t.a = 1 WHERE tt.id = 10", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "update alias hides assignment base");
    failures +=
        expect_contains(mylite_error_message(database), "Unknown column 't.a' in 'field list'",
                        "update alias hides assignment error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "UPDATE t AS tt SET tt.a = 1 WHERE t.id = 10", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "update alias hides where");
    failures +=
        expect_contains(mylite_error_message(database), "Unknown column 't.id' in 'where clause'",
                        "update alias hides where error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "UPDATE t AS tt SET tt.a = 1 ORDER BY t.id LIMIT 1",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "update alias hides order");
    failures +=
        expect_contains(mylite_error_message(database), "Unknown column 't.id' in 'order clause'",
                        "update alias hides order error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "UPDATE t SET must = NULL WHERE id = 10", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "update null not null");
    failures += expect_contains(mylite_error_message(database), "cannot be null",
                                "update null not null error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        prepare_sql(database, "UPDATE t SET must = DEFAULT WHERE id = 10", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "update default missing");
    failures += expect_contains(mylite_error_message(database), "doesn't have a default value",
                                "update default missing error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "UPDATE t SET a = 1 LIMIT 1 OFFSET 1", MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "update offset limit parse error");

    mylite_close(database);
    return failures;
}

static int test_delete_single_table_execution(void)
{
    enum {
        ai_delete_first_insert_id = 100,
        ai_delete_after_max_delete_id = 103,
        ai_delete_after_all_delete_id = 104,
    };
    static const char *const id_v_columns[] = {"id", "v"};
    static const char *const after_null_delete_values[] = {
        "11", "30", "13", "40", "14", "50",
    };
    static const char *const after_alias_delete_values[] = {
        "13",
        "40",
        "14",
        "50",
    };
    static const char *const after_order_delete_values[] = {
        "13",
        "40",
    };
    static const char *const strict_warning_values[] = {
        "1", "10", "2", "20", "3", "30", "4", "40",
    };
    static const char *const ai_values[] = {"104", "5"};
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    uint64_t last_insert_id = 0U;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open memory database");

    failures += prepare_sql(database, "DELETE FROM t", MYLITE_OK, &stmt);
    failures += expect_int(mylite_column_count(stmt), 0, "delete has no result columns");
    failures += expect_int64(mylite_affected_rows(stmt), 0, "prepared delete affected rows");
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "delete no database");
    failures +=
        expect_contains(mylite_error_message(database), "No database selected", "delete no db");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "delete no database affected rows");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "CREATE DATABASE mylite_task20_delete", MYLITE_DONE);
    failures += execute_sql(database, "USE mylite_task20_delete", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE t ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
                            "category INT, "
                            "v INT, "
                            "s VARCHAR(20), "
                            "nullable INT, "
                            "CamelCase INT) AUTO_INCREMENT=10",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO t (category,v,s,nullable,CamelCase) VALUES "
                            "(1,10,'alpha',NULL,100), "
                            "(1,30,'beta',5,200), "
                            "(1,20,'gamma',NULL,300), "
                            "(2,40,'delta',0,400), "
                            "(3,50,'epsilon',7,500)",
                            MYLITE_DONE);

    failures += prepare_sql(database, "DELETE FROM t WHERE nullable IS NULL", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "delete where null");
    failures += expect_int64(mylite_affected_rows(stmt), 2, "delete where null affected rows");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT id, v FROM t ORDER BY id", id_v_columns, 2,
                                   after_null_delete_values, 3, "delete where null rows");

    failures += prepare_sql(database,
                            "DELETE FROM mylite_task20_delete.t AS tt "
                            "WHERE tt.CamelCase = 200",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "delete alias qualified");
    failures += expect_int64(mylite_affected_rows(stmt), 1, "delete alias qualified affected rows");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT id, v FROM t ORDER BY id", id_v_columns, 2,
                                   after_alias_delete_values, 2, "delete alias qualified rows");

    failures += prepare_sql(database, "DELETE FROM t WHERE category = 999", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "delete no match");
    failures += expect_int64(mylite_affected_rows(stmt), 0, "delete no match affected rows");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database,
                            "DELETE FROM t WHERE category >= 2 "
                            "ORDER BY v DESC, id ASC LIMIT 1",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "delete order limit");
    failures += expect_int64(mylite_affected_rows(stmt), 1, "delete order limit affected rows");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT id, v FROM t ORDER BY id", id_v_columns, 2,
                                   after_order_delete_values, 1, "delete order limit rows");

    failures += prepare_sql(database, "DELETE FROM t LIMIT 0", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "delete limit zero");
    failures += expect_int64(mylite_affected_rows(stmt), 0, "delete limit zero affected rows");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "DELETE FROM t WHERE NULL", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "delete where null truth");
    failures +=
        expect_int64(mylite_affected_rows(stmt), 0, "delete where null truth affected rows");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "DELETE FROM t AS tt WHERE t.id = 13", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "delete alias hides where");
    failures +=
        expect_contains(mylite_error_message(database), "Unknown column 't.id' in 'where clause'",
                        "delete alias hides where error");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "delete alias hides where affected");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "DELETE FROM t AS tt ORDER BY t.v LIMIT 1", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "delete alias hides order");
    failures +=
        expect_contains(mylite_error_message(database), "Unknown column 't.v' in 'order clause'",
                        "delete alias hides order error");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "delete alias hides order affected");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += prepare_sql(database, "DELETE FROM t WHERE missing_col = 1", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "delete unknown where");
    failures += expect_contains(mylite_error_message(database),
                                "Unknown column 'missing_col' in 'where clause'",
                                "delete unknown where error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "DELETE FROM t ORDER BY missing_col LIMIT 1", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "delete unknown order");
    failures += expect_contains(mylite_error_message(database),
                                "Unknown column 'missing_col' in 'order clause'",
                                "delete unknown order error");
    mylite_finalize(stmt);
    stmt = NULL;

    failures +=
        prepare_sql(database, "DELETE FROM missing_schema.t WHERE id = 1", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "delete missing schema");
    failures += expect_contains(mylite_error_message(database), "Unknown database 'missing_schema'",
                                "delete missing schema error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "DELETE FROM mylite_task20_delete.missing WHERE id = 1",
                            MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "delete missing table");
    failures += expect_contains(mylite_error_message(database),
                                "Table 'mylite_task20_delete.missing' doesn't exist",
                                "delete missing table error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "DELETE FROM information_schema.tables", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "delete system schema");
    failures +=
        expect_contains(mylite_error_message(database), "system schema", "delete system error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "DELETE FROM t LIMIT 1 OFFSET 1", MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "delete offset limit parse error");

    failures += execute_sql(database, "CREATE TABLE w (id INT PRIMARY KEY, v INT, z VARCHAR(20))",
                            MYLITE_DONE);
    failures += execute_sql(database,
                            "INSERT INTO w VALUES "
                            "(1,10,'2'),(2,20,'2a'),(3,30,'a'),(4,40,'10')",
                            MYLITE_DONE);
    failures += prepare_sql(database, "DELETE FROM w WHERE z = 2", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR,
                              "delete strict predicate warning error");
    failures +=
        expect_contains(mylite_error_message(database), "Truncated incorrect DOUBLE value: '2a'",
                        "delete strict predicate warning message");
    failures += expect_int64(mylite_affected_rows(stmt), -1,
                             "delete strict predicate warning affected rows");
    failures +=
        expect_int(mylite_warning_count(database), 1, "delete strict predicate warning count");
    failures +=
        expect_int((int)mylite_warning_code(database, 0), mysql_warning_truncated_wrong_value,
                   "delete strict predicate warning code");
    mylite_finalize(stmt);
    stmt = NULL;
    failures +=
        expect_select_rows(database, "SELECT id, v FROM w ORDER BY id", id_v_columns, 2,
                           strict_warning_values, 4, "delete strict predicate rollback rows");

    failures += prepare_sql(database, "DELETE FROM w ORDER BY z + 0, id LIMIT 1", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "delete strict order warning error");
    failures +=
        expect_contains(mylite_error_message(database), "Truncated incorrect DOUBLE value: '2a'",
                        "delete strict order warning message");
    failures +=
        expect_int64(mylite_affected_rows(stmt), -1, "delete strict order warning affected rows");
    failures += expect_int(mylite_warning_count(database), 1, "delete strict order warning count");
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_truncated_wrong_value, "delete strict order warning code");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_rows(database, "SELECT id, v FROM w ORDER BY id", id_v_columns, 2,
                                   strict_warning_values, 4, "delete strict order rollback rows");

    failures += execute_sql(database,
                            "CREATE TABLE ai_delete ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, v INT) "
                            "AUTO_INCREMENT=100",
                            MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO ai_delete (v) VALUES (1),(2),(3)", MYLITE_DONE);
    last_insert_id = mylite_last_insert_id(database);
    failures += expect_int64((int64_t)last_insert_id, ai_delete_first_insert_id,
                             "delete auto first insert id");
    failures += prepare_sql(database, "DELETE FROM ai_delete WHERE id = 102", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "delete auto max id");
    failures += expect_int64(mylite_affected_rows(stmt), 1, "delete auto max affected rows");
    failures += expect_int64((int64_t)mylite_last_insert_id(database), (int64_t)last_insert_id,
                             "delete leaves last insert id");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "INSERT INTO ai_delete (v) VALUES (4)", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database),
                             ai_delete_after_max_delete_id, "delete auto next generated id");
    failures += prepare_sql(database, "DELETE FROM ai_delete", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, "delete all auto rows");
    failures += expect_int64(mylite_affected_rows(stmt), 3, "delete all auto affected rows");
    failures += expect_int64((int64_t)mylite_last_insert_id(database),
                             ai_delete_after_max_delete_id, "delete all leaves last insert id");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "INSERT INTO ai_delete (v) VALUES (5)", MYLITE_DONE);
    failures += expect_int64((int64_t)mylite_last_insert_id(database),
                             ai_delete_after_all_delete_id, "delete all preserves auto sequence");
    failures += expect_select_rows(database, "SELECT id, v FROM ai_delete", id_v_columns, 2,
                                   ai_values, 1, "delete auto sequence row");

    mylite_close(database);
    return failures;
}

static int test_transaction_statements_execution(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open transaction db");
    failures += execute_sql(database, "CREATE DATABASE tx_db", MYLITE_DONE);
    failures += execute_sql(database, "USE tx_db", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE tx (id INT PRIMARY KEY, v INT)", MYLITE_DONE);

    failures += execute_sql_expect_done_affected(database, "START TRANSACTION", 0,
                                                 "start transaction affected rows");
    failures += execute_sql(database, "INSERT INTO tx VALUES (1, 10)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(database, "ROLLBACK", 0, "rollback affected rows");
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 1", 0,
                                        "rollback removes inserted row");

    failures +=
        execute_sql_expect_done_affected(database, "BEGIN WORK", 0, "begin work affected rows");
    failures += execute_sql(database, "INSERT INTO tx VALUES (2, 20)", MYLITE_DONE);
    failures +=
        execute_sql_expect_done_affected(database, "COMMIT WORK", 0, "commit work affected rows");
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 2", 1,
                                        "commit preserves inserted row");

    failures +=
        execute_sql_expect_done_affected(database, "COMMIT", 0, "inactive commit affected rows");
    failures += expect_int(mylite_warning_count(database), 0, "inactive commit warning count");
    failures += execute_sql_expect_done_affected(database, "ROLLBACK", 0,
                                                 "inactive rollback affected rows");
    failures += expect_int(mylite_warning_count(database), 0, "inactive rollback warning count");

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (3, 30)", MYLITE_DONE);
    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (4, 40)", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 3", 1,
                                        "repeated start commits active transaction");
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 4", 0,
                                        "rollback removes repeated-start new transaction");

    failures += execute_sql(database, "BEGIN", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (15, 150)", MYLITE_DONE);
    failures += execute_sql(database, "BEGIN", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (16, 160)", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 15", 1,
                                        "repeated begin commits active transaction");
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 16", 0,
                                        "rollback removes repeated-begin new transaction");

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (5, 50)", MYLITE_DONE);
    failures += execute_sql(database, "COMMIT AND CHAIN", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (6, 60)", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 5", 1,
                                        "commit chain preserves pre-chain row");
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 6", 0,
                                        "commit chain rolls back post-chain row");

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (7, 70)", MYLITE_DONE);
    failures += execute_sql(database, "COMMIT AND NO CHAIN", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (8, 80)", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 7", 1,
                                        "commit no chain preserves committed row");
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 8", 1,
                                        "commit no chain resumes autocommit");

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (9, 90)", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK AND CHAIN", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (10, 100)", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 9", 0,
                                        "rollback chain removes pre-chain row");
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 10", 0,
                                        "rollback chain rolls back post-chain row");

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (11, 110)", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK AND NO CHAIN", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (12, 120)", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 11", 0,
                                        "rollback no chain removes active row");
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 12", 1,
                                        "rollback no chain resumes autocommit");

    failures += execute_sql(database,
                            "CREATE TABLE tx_ai ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, v INT) "
                            "AUTO_INCREMENT=30",
                            MYLITE_DONE);
    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx_ai (v) VALUES (1),(2)", MYLITE_DONE);
    failures +=
        expect_int64((int64_t)mylite_last_insert_id(database), 30, "rollback ai last insert id");
    failures += execute_sql(database, "ROLLBACK", MYLITE_DONE);
    failures +=
        expect_int64((int64_t)mylite_last_insert_id(database), 30, "rollback keeps last insert id");
    failures += execute_sql(database, "INSERT INTO tx_ai (v) VALUES (3)", MYLITE_DONE);
    {
        static const char *columns[] = {"id", "v"};
        static const char *values[] = {"32", "3"};

        failures += expect_select_rows(database, "SELECT id, v FROM tx_ai ORDER BY id", columns, 2,
                                       values, 1, "rollback preserves consumed auto increment");
    }
    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "UPDATE tx_ai SET id = 100 WHERE v = 3", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx_ai (v) VALUES (4)", MYLITE_DONE);
    {
        static const char *columns[] = {"id", "v"};
        static const char *values[] = {"32", "3", "101", "4"};

        failures +=
            expect_select_rows(database, "SELECT id, v FROM tx_ai ORDER BY id", columns, 2, values,
                               2, "rollback preserves update auto-increment advancement");
    }

    failures += execute_sql(database, "START TRANSACTION READ ONLY", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 2", 1,
                                        "read only transaction allows reads");
    failures += prepare_sql(database, "INSERT INTO tx VALUES (13, 130)", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "read only transaction rejects insert");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "read only insert affected rows");
    failures += expect_contains(mylite_error_message(database),
                                "Cannot execute statement in a READ ONLY transaction",
                                "read only transaction error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "UPDATE tx SET v = 130 WHERE id = 2", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "read only transaction rejects update");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "read only update affected rows");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += prepare_sql(database, "DELETE FROM tx WHERE id = 2", MYLITE_OK, &stmt);
    failures +=
        expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, "read only transaction rejects delete");
    failures += expect_int64(mylite_affected_rows(stmt), -1, "read only delete affected rows");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "COMMIT", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 13", 0,
                                        "read only rejected insert absent");
    {
        static const char *columns[] = {"id", "v"};
        static const char *values[] = {"2", "20"};

        failures += expect_select_rows(database, "SELECT id, v FROM tx WHERE id = 2", columns, 2,
                                       values, 1, "read only rejected update/delete absent");
    }

    failures += execute_sql(database, "START TRANSACTION READ WRITE", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (14, 140)", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 14", 0,
                                        "read write insert rolls back");

    failures += execute_sql(database, "START TRANSACTION WITH CONSISTENT SNAPSHOT", MYLITE_DONE);
    failures += expect_int(mylite_warning_count(database), 0, "consistent snapshot warning count");
    failures += execute_sql(database, "ROLLBACK", MYLITE_DONE);

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (17, 170)", MYLITE_DONE);
    failures +=
        prepare_sql(database, "INSERT INTO tx VALUES (18, 180), (17, 171)", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR,
                              "explicit transaction rolls back failed statement");
    failures +=
        expect_int64(mylite_affected_rows(stmt), -1, "failed statement atomicity affected rows");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "COMMIT", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 17", 1,
                                        "failed statement preserves prior transaction work");
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 18", 0,
                                        "failed statement savepoint rolls back partial work");

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (19, 190)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(database, "COMMIT RELEASE", 0,
                                                 "commit release affected rows");
    failures += expect_prepare_error(database, "SELECT id FROM tx", MYLITE_EXEC_ERROR, "released",
                                     "prepare after release");

    mylite_close(database);
    database = NULL;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open rollback release db");
    failures += execute_sql(database, "CREATE DATABASE tx_release_db", MYLITE_DONE);
    failures += execute_sql(database, "USE tx_release_db", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE tx_release (id INT PRIMARY KEY)", MYLITE_DONE);
    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx_release VALUES (1)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(database, "ROLLBACK RELEASE", 0,
                                                 "rollback release affected rows");
    failures += expect_prepare_error(database, "SELECT id FROM tx_release", MYLITE_EXEC_ERROR,
                                     "released", "prepare after rollback release");

    mylite_close(database);
    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_savepoint_execution(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open savepoint db");
    failures += execute_sql(database, "CREATE DATABASE sp_db", MYLITE_DONE);
    failures += execute_sql(database, "USE sp_db", MYLITE_DONE);
    failures += execute_sql(database, "CREATE TABLE tx (id INT PRIMARY KEY, v INT)", MYLITE_DONE);
    failures += execute_sql(database,
                            "CREATE TABLE ai ("
                            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, v INT) "
                            "AUTO_INCREMENT=10",
                            MYLITE_DONE);

    failures += execute_sql_expect_done_affected(database, "SAVEPOINT outside_sp", 0,
                                                 "savepoint outside transaction");
    failures += execute_sql(database, "INSERT INTO tx VALUES (1, 10)", MYLITE_DONE);
    failures += prepare_sql(database, "ROLLBACK TO SAVEPOINT outside_sp", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "SAVEPOINT outside_sp does not exist",
                                  "rollback to outside savepoint");
    failures += expect_savepoint_warning(database, "SAVEPOINT outside_sp does not exist",
                                         "outside savepoint warning");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 1", 1,
                                        "outside savepoint does not start transaction");

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (2, 20)", MYLITE_DONE);
    failures +=
        execute_sql_expect_done_affected(database, "SAVEPOINT a", 0, "savepoint affected rows");
    failures += execute_sql(database, "INSERT INTO tx VALUES (3, 30)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(database, "ROLLBACK TO a", 0,
                                                 "rollback to savepoint affected rows");
    failures += execute_sql(database, "INSERT INTO tx VALUES (4, 40)", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK WORK TO SAVEPOINT a", MYLITE_DONE);
    failures += execute_sql(database, "COMMIT", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 2", 1,
                                        "rollback to keeps pre-savepoint row");
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id IN (3,4)", 0,
                                        "rollback to removes post-savepoint rows");

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (5, 50)", MYLITE_DONE);
    failures += execute_sql(database, "SAVEPOINT r", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (6, 60)", MYLITE_DONE);
    failures += execute_sql_expect_done_affected(database, "RELEASE SAVEPOINT r", 0,
                                                 "release savepoint affected rows");
    failures += prepare_sql(database, "ROLLBACK TO r", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "SAVEPOINT r does not exist",
                                  "rollback released savepoint");
    failures += expect_savepoint_warning(database, "SAVEPOINT r does not exist",
                                         "released savepoint warning");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "INSERT INTO tx VALUES (7, 70)", MYLITE_DONE);
    failures += execute_sql(database, "COMMIT", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id IN (5,6,7)", 3,
                                        "missing savepoint error keeps transaction active");

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (8, 80)", MYLITE_DONE);
    failures += execute_sql(database, "SAVEPOINT same_name", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (9, 90)", MYLITE_DONE);
    failures += execute_sql(database, "SAVEPOINT middle_name", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (10, 100)", MYLITE_DONE);
    failures += execute_sql(database, "SAVEPOINT same_name", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (11, 110)", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK TO middle_name", MYLITE_DONE);
    failures += execute_sql(database, "COMMIT", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id IN (8,9)", 2,
                                        "replacement keeps intervening savepoint");
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id IN (10,11)", 0,
                                        "rollback to intervening savepoint removes later rows");

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "SAVEPOINT outer_sp", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (12, 120)", MYLITE_DONE);
    failures += execute_sql(database, "SAVEPOINT inner_sp", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (13, 130)", MYLITE_DONE);
    failures += execute_sql(database, "RELEASE SAVEPOINT outer_sp", MYLITE_DONE);
    failures += prepare_sql(database, "ROLLBACK TO inner_sp", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "SAVEPOINT inner_sp does not exist",
                                  "release outer removes inner");
    failures += expect_savepoint_warning(database, "SAVEPOINT inner_sp does not exist",
                                         "released nested savepoint warning");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "COMMIT", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id IN (12,13)", 2,
                                        "release does not roll back data");

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "SAVEPOINT MixedCase", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (14, 140)", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK TO mixedcase", MYLITE_DONE);
    failures += execute_sql(database, "SAVEPOINT `db.sp`", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (15, 150)", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK TO `DB.SP`", MYLITE_DONE);
    failures += execute_sql(database, "RELEASE SAVEPOINT `db.sp`", MYLITE_DONE);
    failures += execute_sql(database, "COMMIT", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id IN (14,15)", 0,
                                        "savepoint lookup is case-insensitive");

    failures += execute_sql(database, "START TRANSACTION READ ONLY", MYLITE_DONE);
    failures += execute_sql(database, "SAVEPOINT ro", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK TO SAVEPOINT ro", MYLITE_DONE);
    failures += execute_sql(database, "RELEASE SAVEPOINT ro", MYLITE_DONE);
    failures += prepare_sql(database, "UPDATE tx SET v = 200 WHERE id = 2", MYLITE_OK, &stmt);
    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR,
                              "read only transaction rejects update after savepoint");
    failures += expect_contains(mylite_error_message(database),
                                "Cannot execute statement in a READ ONLY transaction",
                                "read only update after savepoint error");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "COMMIT", MYLITE_DONE);

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "SAVEPOINT before_ai", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO ai (v) VALUES (100),(200)", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK TO before_ai", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO ai (v) VALUES (300)", MYLITE_DONE);
    failures += execute_sql(database, "COMMIT", MYLITE_DONE);
    {
        static const char *columns[] = {"id", "v"};
        static const char *values[] = {"12", "300"};

        failures +=
            expect_select_rows(database, "SELECT id, v FROM ai ORDER BY id", columns, 2, values, 1,
                               "rollback to preserves consumed auto-increment ids");
    }

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "SAVEPOINT commit_clear", MYLITE_DONE);
    failures += execute_sql(database, "COMMIT", MYLITE_DONE);
    failures += prepare_sql(database, "ROLLBACK TO commit_clear", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "SAVEPOINT commit_clear does not exist",
                                  "commit clears savepoints");
    failures += expect_savepoint_warning(database, "SAVEPOINT commit_clear does not exist",
                                         "commit cleared savepoint warning");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "SAVEPOINT rollback_clear", MYLITE_DONE);
    failures += execute_sql(database, "ROLLBACK", MYLITE_DONE);
    failures += prepare_sql(database, "RELEASE SAVEPOINT rollback_clear", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "SAVEPOINT rollback_clear does not exist",
                                  "rollback clears savepoints");
    failures += expect_savepoint_warning(database, "SAVEPOINT rollback_clear does not exist",
                                         "rollback cleared savepoint warning");
    mylite_finalize(stmt);
    stmt = NULL;

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "SAVEPOINT start_clear", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (16, 160)", MYLITE_DONE);
    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += prepare_sql(database, "ROLLBACK TO start_clear", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "SAVEPOINT start_clear does not exist",
                                  "repeated start clears savepoints");
    failures += expect_savepoint_warning(database, "SAVEPOINT start_clear does not exist",
                                         "repeated start cleared savepoint warning");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "ROLLBACK", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 16", 1,
                                        "repeated start commits preexisting transaction");

    failures += execute_sql(database, "BEGIN", MYLITE_DONE);
    failures += execute_sql(database, "SAVEPOINT begin_clear", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (17, 170)", MYLITE_DONE);
    failures += execute_sql(database, "BEGIN", MYLITE_DONE);
    failures += prepare_sql(database, "RELEASE SAVEPOINT begin_clear", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "SAVEPOINT begin_clear does not exist",
                                  "repeated begin clears savepoints");
    failures += expect_savepoint_warning(database, "SAVEPOINT begin_clear does not exist",
                                         "repeated begin cleared savepoint warning");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "ROLLBACK", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id = 17", 1,
                                        "repeated begin commits preexisting transaction");

    failures += execute_sql(database, "START TRANSACTION", MYLITE_DONE);
    failures += execute_sql(database, "SAVEPOINT mylite_statement_atomicity", MYLITE_DONE);
    failures += execute_sql(database, "INSERT INTO tx VALUES (20, 200)", MYLITE_DONE);
    failures +=
        prepare_sql(database, "INSERT INTO tx VALUES (21, 210),(21, 211)", MYLITE_OK, &stmt);
    failures += expect_exec_error(stmt, database, "Duplicate entry '21'",
                                  "failed statement inside user savepoint");
    mylite_finalize(stmt);
    stmt = NULL;
    failures += execute_sql(database, "ROLLBACK TO mylite_statement_atomicity", MYLITE_DONE);
    failures += execute_sql(database, "COMMIT", MYLITE_DONE);
    failures += expect_select_row_count(database, "SELECT id FROM tx WHERE id IN (20,21)", 0,
                                        "user savepoint does not collide with statement atomicity");

    mylite_close(database);
    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_parse_error(void)
{
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += expect_status(mylite_open_memory(&database), MYLITE_OK, "open memory database");
    failures += prepare_sql(database, "SELECT FROM DUAL", MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "parse error returned a statement handle\n");
        failures = 1;
    }

    mylite_close(database);
    return failures;
}

static int prepare_sql(mylite_db *database, const char *sql, int expected_status,
                       mylite_stmt **out_stmt)
{
    int actual = mylite_prepare(database, sql, strlen(sql), out_stmt);

    if (actual != expected_status) {
        fprintf(stderr, "prepare '%s': expected %s, got %s (%s)\n", sql,
                mylite_status_name(expected_status), mylite_status_name(actual),
                mylite_error_message(database));
        return 1;
    }

    return 0;
}

static int expect_prepare_error(mylite_db *database, const char *sql, int expected_status,
                                const char *error_fragment, const char *context)
{
    mylite_stmt *stmt = NULL;
    int failures = prepare_sql(database, sql, expected_status, &stmt);

    failures += expect_no_stmt_handle(&stmt, context);
    failures += expect_contains(mylite_error_message(database), error_fragment, context);
    return failures;
}

static int expect_exec_error(mylite_stmt *stmt, mylite_db *database, const char *error_fragment,
                             const char *context)
{
    int failures = 0;

    failures += expect_status(mylite_step(stmt), MYLITE_EXEC_ERROR, context);
    failures += expect_int64(mylite_affected_rows(stmt), -1, context);
    failures += expect_contains(mylite_error_message(database), error_fragment, context);
    return failures;
}

static int expect_savepoint_warning(mylite_db *database, const char *error_fragment,
                                    const char *context)
{
    int failures = 0;

    failures += expect_int(mylite_warning_count(database), 1, context);
    failures += expect_int((int)mylite_warning_code(database, 0),
                           mysql_warning_savepoint_does_not_exist, context);
    failures += expect_contains(mylite_warning_message(database, 0), error_fragment, context);
    return failures;
}

static int expect_select_rows(mylite_db *database, const char *sql, const char *const *columns,
                              int column_count, const char *const *values, int row_count,
                              const char *context)
{
    mylite_stmt *stmt = NULL;
    int failures = prepare_sql(database, sql, MYLITE_OK, &stmt);

    failures += expect_column_names(stmt, columns, column_count, context);
    for (int row = 0; row < row_count; ++row) {
        failures += expect_status(mylite_step(stmt), MYLITE_ROW, context);
        for (int column = 0; column < column_count; ++column) {
            const char *expected = values[(row * column_count) + column];

            if (expected == NULL) {
                failures += expect_null_text(mylite_column_text(stmt, column), context);
            } else {
                failures += expect_string(mylite_column_text(stmt, column), expected, context);
            }
        }
    }
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, context);
    failures += expect_int64(mylite_affected_rows(stmt), -1, context);
    mylite_finalize(stmt);
    return failures;
}

static int expect_select_row_count(mylite_db *database, const char *sql, int row_count,
                                   const char *context)
{
    mylite_stmt *stmt = NULL;
    int failures = prepare_sql(database, sql, MYLITE_OK, &stmt);

    for (int row = 0; row < row_count; ++row) {
        failures += expect_status(mylite_step(stmt), MYLITE_ROW, context);
    }
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, context);
    failures += expect_int64(mylite_affected_rows(stmt), -1, context);
    mylite_finalize(stmt);
    return failures;
}

static int expect_no_stmt_handle(mylite_stmt **stmt, const char *context)
{
    if (stmt == NULL || *stmt == NULL) {
        return 0;
    }

    fprintf(stderr, "%s returned a statement handle\n", context);
    mylite_finalize(*stmt);
    *stmt = NULL;
    return 1;
}

static int execute_sql(mylite_db *database, const char *sql, int expected_step_status)
{
    mylite_stmt *stmt = NULL;
    int failures = prepare_sql(database, sql, MYLITE_OK, &stmt);

    if (failures == 0) {
        failures += expect_status(mylite_step(stmt), expected_step_status, sql);
    }
    mylite_finalize(stmt);
    return failures;
}

static int execute_sql_expect_done_affected(mylite_db *database, const char *sql,
                                            int64_t expected_affected_rows, const char *context)
{
    mylite_stmt *stmt = NULL;
    int failures = prepare_sql(database, sql, MYLITE_OK, &stmt);

    if (failures == 0) {
        failures += expect_int(mylite_column_count(stmt), 0, context);
        failures += expect_status(mylite_step(stmt), MYLITE_DONE, context);
        failures += expect_int64(mylite_affected_rows(stmt), expected_affected_rows, context);
    }
    mylite_finalize(stmt);
    return failures;
}

static int expect_information_schema_schemata_row(mylite_db *database,
                                                  const struct expected_schemata_row *expected)
{
    mylite_stmt *stmt = NULL;
    int failures =
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.SCHEMATA", MYLITE_OK, &stmt);
    int saw_schema = 0;

    while (failures == 0) {
        int status = mylite_step(stmt);

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "schemata row");
        if (failures != 0) {
            break;
        }

        if (strcmp(mylite_column_text(stmt, schemata_name_column), expected->schema_name) == 0) {
            saw_schema = 1;
            failures += expect_string(mylite_column_text(stmt, schemata_catalog_column), "def",
                                      "schemata catalog");
            failures += expect_string(mylite_column_text(stmt, schemata_character_set_column),
                                      expected->character_set, "schemata charset");
            failures += expect_string(mylite_column_text(stmt, schemata_collation_column),
                                      expected->collation, "schemata collation");
            failures += expect_null_text(mylite_column_text(stmt, schemata_sql_path_column),
                                         "schemata sql path");
            failures += expect_string(mylite_column_text(stmt, schemata_encryption_column),
                                      expected->encryption, "schemata encryption");
            break;
        }
    }

    if (!saw_schema) {
        fprintf(stderr, "schemata did not return required schema '%s'\n", expected->schema_name);
        failures = 1;
    }

    mylite_finalize(stmt);
    return failures;
}

static int expect_no_information_schema_schemata_row(mylite_db *database, const char *schema_name)
{
    mylite_stmt *stmt = NULL;
    int failures =
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.SCHEMATA", MYLITE_OK, &stmt);

    while (failures == 0) {
        int status = mylite_step(stmt);

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "schemata row");
        if (failures != 0) {
            break;
        }
        if (strcmp(mylite_column_text(stmt, schemata_name_column), schema_name) == 0) {
            fprintf(stderr, "schemata unexpectedly returned schema '%s'\n", schema_name);
            failures = 1;
            break;
        }
    }

    mylite_finalize(stmt);
    return failures;
}

static int expect_information_schema_tables_views(mylite_db *database)
{
    static const char *const expected_tables[] = {
        "CHARACTER_SETS", "COLLATIONS", "COLUMNS", "ENGINES", "SCHEMATA", "STATISTICS", "TABLES",
    };
    mylite_stmt *stmt = NULL;
    int failures =
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.TABLES", MYLITE_OK, &stmt);
    int seen[information_schema_view_count] = {0};

    while (failures == 0) {
        int status = mylite_step(stmt);
        const char *schema_name = NULL;
        const char *table_name = NULL;

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "tables row");
        if (failures != 0) {
            break;
        }

        schema_name = mylite_column_text(stmt, tables_schema_column);
        table_name = mylite_column_text(stmt, tables_name_column);
        if (schema_name == NULL || strcmp(schema_name, "information_schema") != 0) {
            continue;
        }

        for (int index = 0; index < information_schema_view_count; ++index) {
            if (strcmp(table_name, expected_tables[index]) == 0) {
                seen[index] = 1;
                failures += expect_string(mylite_column_text(stmt, tables_catalog_column), "def",
                                          "tables catalog");
                failures += expect_string(mylite_column_text(stmt, tables_type_column),
                                          "SYSTEM VIEW", "tables type");
                failures += expect_null_text(mylite_column_text(stmt, tables_engine_column),
                                             "tables engine");
                failures += expect_int64(mylite_column_int64(stmt, tables_version_column),
                                         information_schema_table_version, "tables version");
                failures +=
                    expect_int64(mylite_column_int64(stmt, tables_rows_column), 0, "tables rows");
                failures += expect_null_text(mylite_column_text(stmt, tables_collation_column),
                                             "tables table collation");
                failures += expect_string(mylite_column_text(stmt, tables_comment_column), "",
                                          "tables comment");
            }
        }
    }

    for (int index = 0; index < information_schema_view_count; ++index) {
        if (!seen[index]) {
            fprintf(stderr, "tables did not return information_schema.%s\n",
                    expected_tables[index]);
            failures = 1;
        }
    }

    mylite_finalize(stmt);
    return failures;
}

static int expect_no_information_schema_table_schema_row(mylite_db *database,
                                                         const char *schema_name)
{
    mylite_stmt *stmt = NULL;
    int failures =
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.TABLES", MYLITE_OK, &stmt);

    while (failures == 0) {
        int status = mylite_step(stmt);

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "tables row");
        if (failures != 0) {
            break;
        }
        if (strcmp(mylite_column_text(stmt, tables_schema_column), schema_name) == 0) {
            fprintf(stderr, "tables unexpectedly returned row for schema '%s'\n", schema_name);
            failures = 1;
            break;
        }
    }

    mylite_finalize(stmt);
    return failures;
}

static int expect_no_information_schema_table_name_row(mylite_db *database, const char *table_name)
{
    mylite_stmt *stmt = NULL;
    int failures =
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.TABLES", MYLITE_OK, &stmt);

    while (failures == 0) {
        int status = mylite_step(stmt);

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "tables row");
        if (failures != 0) {
            break;
        }
        if (strcmp(mylite_column_text(stmt, tables_name_column), table_name) == 0) {
            fprintf(stderr, "tables unexpectedly returned row for table '%s'\n", table_name);
            failures = 1;
            break;
        }
    }

    mylite_finalize(stmt);
    return failures;
}

static int expect_information_schema_column_row(mylite_db *database,
                                                const struct expected_columns_row *expected)
{
    mylite_stmt *stmt = NULL;
    int failures =
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.COLUMNS", MYLITE_OK, &stmt);
    int saw_row = 0;

    while (failures == 0) {
        int status = mylite_step(stmt);
        const char *table_name = NULL;
        const char *column_name = NULL;

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "columns row");
        if (failures != 0) {
            break;
        }
        table_name = mylite_column_text(stmt, columns_table_name_column);
        column_name = mylite_column_text(stmt, columns_name_column);
        if (strcmp(table_name, expected->table_name) != 0 ||
            strcmp(column_name, expected->column_name) != 0) {
            continue;
        }

        saw_row = 1;
        failures += expect_int64(mylite_column_int64(stmt, columns_ordinal_column),
                                 expected->ordinal_position, "columns ordinal");
        if (expected->column_default == NULL) {
            failures += expect_null_text(mylite_column_text(stmt, columns_default_column),
                                         "columns default");
        } else {
            failures += expect_string(mylite_column_text(stmt, columns_default_column),
                                      expected->column_default, "columns default");
        }
        failures += expect_string(mylite_column_text(stmt, columns_nullable_column),
                                  expected->is_nullable, "columns nullable");
        failures += expect_string(mylite_column_text(stmt, columns_data_type_column),
                                  expected->data_type, "columns data type");
        failures += expect_string(mylite_column_text(stmt, columns_type_column),
                                  expected->column_type, "columns type");
        failures += expect_string(mylite_column_text(stmt, columns_key_column),
                                  expected->column_key, "columns key");
        failures += expect_string(mylite_column_text(stmt, columns_extra_column), expected->extra,
                                  "columns extra");
        break;
    }

    if (!saw_row) {
        fprintf(stderr, "columns did not return row for table '%s' column '%s'\n",
                expected->table_name, expected->column_name);
        failures = 1;
    }

    mylite_finalize(stmt);
    return failures;
}

static int expect_no_information_schema_column_table_name_row(mylite_db *database,
                                                              const char *table_name)
{
    mylite_stmt *stmt = NULL;
    int failures =
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.COLUMNS", MYLITE_OK, &stmt);

    while (failures == 0) {
        int status = mylite_step(stmt);

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "columns row");
        if (failures != 0) {
            break;
        }
        if (strcmp(mylite_column_text(stmt, columns_table_name_column), table_name) == 0) {
            fprintf(stderr, "columns unexpectedly returned row for table '%s'\n", table_name);
            failures = 1;
            break;
        }
    }

    mylite_finalize(stmt);
    return failures;
}

static int expect_no_information_schema_column_row(mylite_db *database, const char *table_name,
                                                   const char *column_name)
{
    mylite_stmt *stmt = NULL;
    int failures =
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.COLUMNS", MYLITE_OK, &stmt);

    while (failures == 0) {
        int status = mylite_step(stmt);

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "columns row");
        if (failures != 0) {
            break;
        }
        if (strcmp(mylite_column_text(stmt, columns_table_name_column), table_name) == 0 &&
            strcmp(mylite_column_text(stmt, columns_name_column), column_name) == 0) {
            fprintf(stderr, "columns unexpectedly returned row for table '%s' column '%s'\n",
                    table_name, column_name);
            failures = 1;
            break;
        }
    }

    mylite_finalize(stmt);
    return failures;
}

static int expect_no_information_schema_statistics_table_name_row(mylite_db *database,
                                                                  const char *table_name)
{
    mylite_stmt *stmt = NULL;
    int failures =
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.STATISTICS", MYLITE_OK, &stmt);

    while (failures == 0) {
        int status = mylite_step(stmt);

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "statistics row");
        if (failures != 0) {
            break;
        }
        if (strcmp(mylite_column_text(stmt, statistics_table_name_column), table_name) == 0) {
            fprintf(stderr, "statistics unexpectedly returned row for table '%s'\n", table_name);
            failures = 1;
            break;
        }
    }

    mylite_finalize(stmt);
    return failures;
}

static int expect_no_information_schema_statistics_column_row(mylite_db *database,
                                                              const char *table_name,
                                                              const char *column_name)
{
    mylite_stmt *stmt = NULL;
    int failures =
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.STATISTICS", MYLITE_OK, &stmt);

    while (failures == 0) {
        int status = mylite_step(stmt);

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "statistics row");
        if (failures != 0) {
            break;
        }
        if (strcmp(mylite_column_text(stmt, statistics_table_name_column), table_name) == 0 &&
            strcmp(mylite_column_text(stmt, statistics_column_name_column), column_name) == 0) {
            fprintf(stderr, "statistics unexpectedly returned row for table '%s' column '%s'\n",
                    table_name, column_name);
            failures = 1;
            break;
        }
    }

    mylite_finalize(stmt);
    return failures;
}

static int expect_information_schema_statistics_row(mylite_db *database,
                                                    const struct expected_statistics_row *expected)
{
    mylite_stmt *stmt = NULL;
    int failures =
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.STATISTICS", MYLITE_OK, &stmt);
    int saw_row = 0;

    while (failures == 0) {
        int status = mylite_step(stmt);
        const char *table_name = NULL;
        const char *index_name = NULL;
        int64_t seq_in_index = 0;

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "statistics row");
        if (failures != 0) {
            break;
        }

        table_name = mylite_column_text(stmt, statistics_table_name_column);
        index_name = mylite_column_text(stmt, statistics_index_name_column);
        seq_in_index = mylite_column_int64(stmt, statistics_seq_column);
        if (strcmp(table_name, expected->table_name) != 0 ||
            strcmp(index_name, expected->index_name) != 0 ||
            seq_in_index != expected->seq_in_index) {
            continue;
        }

        saw_row = 1;
        failures += expect_int64(mylite_column_int64(stmt, statistics_non_unique_column),
                                 expected->non_unique, "statistics non unique");
        failures += expect_string(mylite_column_text(stmt, statistics_column_name_column),
                                  expected->column_name, "statistics column name");
        failures += expect_string(mylite_column_text(stmt, statistics_collation_column),
                                  expected->collation, "statistics collation");
        if (expected->sub_part == NULL) {
            failures += expect_null_text(mylite_column_text(stmt, statistics_sub_part_column),
                                         "statistics sub part");
        } else {
            failures += expect_string(mylite_column_text(stmt, statistics_sub_part_column),
                                      expected->sub_part, "statistics sub part");
        }
        failures += expect_string(mylite_column_text(stmt, statistics_index_type_column), "BTREE",
                                  "statistics index type");
        failures += expect_string(mylite_column_text(stmt, statistics_index_comment_column),
                                  expected->index_comment, "statistics index comment");
        failures += expect_string(mylite_column_text(stmt, statistics_visible_column),
                                  expected->visible, "statistics visibility");
        break;
    }

    if (!saw_row) {
        fprintf(stderr, "statistics did not return row for %s.%s seq %" PRId64 "\n",
                expected->table_name, expected->index_name, expected->seq_in_index);
        failures = 1;
    }

    mylite_finalize(stmt);
    return failures;
}

static int expect_no_information_schema_statistics_index_row(mylite_db *database,
                                                             const char *table_name,
                                                             const char *index_name)
{
    mylite_stmt *stmt = NULL;
    int failures =
        prepare_sql(database, "SELECT * FROM INFORMATION_SCHEMA.STATISTICS", MYLITE_OK, &stmt);

    while (failures == 0) {
        int status = mylite_step(stmt);
        const char *current_table_name = NULL;
        const char *current_index_name = NULL;

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "statistics row");
        if (failures != 0) {
            break;
        }
        current_table_name = mylite_column_text(stmt, statistics_table_name_column);
        current_index_name = mylite_column_text(stmt, statistics_index_name_column);
        if (strcmp(current_table_name, table_name) == 0 &&
            strcmp(current_index_name, index_name) == 0) {
            fprintf(stderr, "statistics unexpectedly returned row for table '%s' index '%s'\n",
                    table_name, index_name);
            failures = 1;
            break;
        }
    }

    mylite_finalize(stmt);
    return failures;
}

static int expect_empty_information_schema_table(mylite_db *database, const char *sql,
                                                 const char *const *columns, int column_count)
{
    mylite_stmt *stmt = NULL;
    int failures = prepare_sql(database, sql, MYLITE_OK, &stmt);

    failures += expect_column_names(stmt, columns, column_count, sql);
    failures += expect_status(mylite_step(stmt), MYLITE_DONE, sql);

    mylite_finalize(stmt);
    return failures;
}

static int expect_show_database_rows(mylite_db *database, const char *required,
                                     const char *forbidden)
{
    mylite_stmt *stmt = NULL;
    int failures = prepare_sql(database, "SHOW SCHEMAS", MYLITE_OK, &stmt);
    int saw_required = required == NULL ? 1 : 0;

    while (failures == 0) {
        int status = mylite_step(stmt);
        const char *schema_name = NULL;

        if (status == MYLITE_DONE) {
            break;
        }
        failures += expect_status(status, MYLITE_ROW, "show schemas row");
        if (failures != 0) {
            break;
        }

        schema_name = mylite_column_text(stmt, 0);
        if (required != NULL && schema_name != NULL && strcmp(schema_name, required) == 0) {
            saw_required = 1;
        }
        if (forbidden != NULL && schema_name != NULL && strcmp(schema_name, forbidden) == 0) {
            fprintf(stderr, "show schemas unexpectedly returned '%s'\n", forbidden);
            failures = 1;
            break;
        }
    }

    if (!saw_required) {
        fprintf(stderr, "show schemas did not return required schema '%s'\n", required);
        failures = 1;
    }

    mylite_finalize(stmt);
    return failures;
}

static int expect_connection_state(mylite_db *database, const char *client, const char *connection,
                                   const char *results, const char *collation, const char *context)
{
    int failures = 0;

    failures += expect_string(mylite_connection_character_set_client(database), client, context);
    failures +=
        expect_string(mylite_connection_character_set_connection(database), connection, context);
    failures += expect_string(mylite_connection_character_set_results(database), results, context);
    failures += expect_string(mylite_connection_collation_connection(database), collation, context);
    return failures;
}

static int expect_column_names(const mylite_stmt *stmt, const char *const *expected, int count,
                               const char *context)
{
    int failures = expect_int(mylite_column_count(stmt), count, context);

    for (int index = 0; index < count; ++index) {
        failures += expect_string(mylite_column_name(stmt, index), expected[index], context);
    }
    return failures;
}

static int expect_column_metadata(const mylite_stmt *stmt,
                                  const struct expected_column_metadata *expected, int count,
                                  const char *context)
{
    int failures = expect_int(mylite_column_count(stmt), count, context);

    for (int index = 0; index < count; ++index) {
        failures += expect_string(mylite_column_name(stmt, index), expected[index].name, context);
        failures += expect_string(mylite_column_schema_name(stmt, index),
                                  expected[index].schema_name, context);
        failures += expect_string(mylite_column_table_name(stmt, index), expected[index].table_name,
                                  context);
        failures += expect_string(mylite_column_origin_table_name(stmt, index),
                                  expected[index].origin_table_name, context);
        failures += expect_string(mylite_column_origin_name(stmt, index),
                                  expected[index].origin_column_name, context);
    }
    return failures;
}

static int expect_result_metadata(const mylite_stmt *stmt,
                                  const struct expected_result_metadata *expected, int count,
                                  const char *context)
{
    int failures = expect_int(mylite_column_count(stmt), count, context);

    for (int index = 0; index < count; ++index) {
        unsigned int flags = mylite_column_flags(stmt, index);

        failures += expect_string(mylite_column_name(stmt, index), expected[index].name, context);
        if (expected[index].schema_name == NULL) {
            failures += expect_null_text(mylite_column_schema_name(stmt, index), context);
        } else {
            failures += expect_string(mylite_column_schema_name(stmt, index),
                                      expected[index].schema_name, context);
        }
        if (expected[index].table_name == NULL) {
            failures += expect_null_text(mylite_column_table_name(stmt, index), context);
        } else {
            failures += expect_string(mylite_column_table_name(stmt, index),
                                      expected[index].table_name, context);
        }
        if (expected[index].origin_schema_name == NULL) {
            failures += expect_null_text(mylite_column_origin_schema_name(stmt, index), context);
        } else {
            failures += expect_string(mylite_column_origin_schema_name(stmt, index),
                                      expected[index].origin_schema_name, context);
        }
        if (expected[index].origin_table_name == NULL) {
            failures += expect_null_text(mylite_column_origin_table_name(stmt, index), context);
        } else {
            failures += expect_string(mylite_column_origin_table_name(stmt, index),
                                      expected[index].origin_table_name, context);
        }
        if (expected[index].origin_column_name == NULL) {
            failures += expect_null_text(mylite_column_origin_name(stmt, index), context);
        } else {
            failures += expect_string(mylite_column_origin_name(stmt, index),
                                      expected[index].origin_column_name, context);
        }
        failures +=
            expect_int(mylite_column_field_type(stmt, index), expected[index].field_type, context);
        failures += expect_int64((int64_t)mylite_column_declared_length(stmt, index),
                                 (int64_t)expected[index].declared_length, context);
        failures += expect_int64((int64_t)mylite_column_max_length(stmt, index), 0, context);
        failures +=
            expect_u16(mylite_column_decimals(stmt, index), expected[index].decimals, context);
        failures +=
            expect_u16(mylite_column_charset_id(stmt, index), expected[index].charset_id, context);
        failures +=
            expect_int(mylite_column_is_nullable(stmt, index), expected[index].nullable, context);
        if ((flags & expected[index].flags_set) != expected[index].flags_set) {
            fprintf(stderr, "%s: column %d expected flags 0x%x in 0x%x\n", context, index,
                    expected[index].flags_set, flags);
            failures = 1;
        }
        if ((flags & expected[index].flags_clear) != 0U) {
            fprintf(stderr, "%s: column %d expected flags 0x%x clear in 0x%x\n", context, index,
                    expected[index].flags_clear, flags);
            failures = 1;
        }
    }
    return failures;
}

static char *expected_physical_table_name(const char *schema_name, const char *table_name)
{
    static const char prefix[] = "__mylite_user_";
    static const char separator[] = "__";
    size_t schema_length = strlen(schema_name);
    size_t table_length = strlen(table_name);
    size_t output_length =
        strlen(prefix) + (schema_length * 2U) + strlen(separator) + (table_length * 2U);
    char *output = malloc(output_length + 1U);
    size_t offset = 0U;

    if (output == NULL) {
        return NULL;
    }

    memcpy(output + offset, prefix, strlen(prefix));
    offset += strlen(prefix);
    for (size_t index = 0U; index < schema_length; ++index) {
        (void)snprintf(output + offset, 3U, "%02X", (unsigned char)schema_name[index]);
        offset += 2U;
    }
    memcpy(output + offset, separator, strlen(separator));
    offset += strlen(separator);
    for (size_t index = 0U; index < table_length; ++index) {
        (void)snprintf(output + offset, 3U, "%02X", (unsigned char)table_name[index]);
        offset += 2U;
    }
    output[offset] = '\0';
    return output;
}

static int expect_sqlite_table_exists(const struct sqlite_table_lookup *lookup)
{
    sqlite3 *sqlite = NULL;
    sqlite3_stmt *stmt = NULL;
    int failures = expect_sqlite_status(
        sqlite3_open_v2(lookup->path, &sqlite, SQLITE_OPEN_READONLY, mylite_vfs_name()), SQLITE_OK,
        "open sqlite for physical table check");
    int rc = SQLITE_OK;

    if (sqlite == NULL) {
        return failures + 1;
    }

    rc = sqlite3_prepare_v2(sqlite, "SELECT 1 FROM sqlite_schema WHERE type='table' AND name=?", -1,
                            &stmt, NULL);
    failures += expect_sqlite_status(rc, SQLITE_OK, "prepare physical table check");
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, lookup->table_name, -1, SQLITE_STATIC);
        failures += expect_sqlite_status(sqlite3_step(stmt), SQLITE_ROW, "physical table exists");
    }
    sqlite3_finalize(stmt);
    sqlite3_close(sqlite);
    return failures;
}

static int expect_sqlite_table_missing(const struct sqlite_table_lookup *lookup)
{
    sqlite3 *sqlite = NULL;
    sqlite3_stmt *stmt = NULL;
    int failures = expect_sqlite_status(
        sqlite3_open_v2(lookup->path, &sqlite, SQLITE_OPEN_READONLY, mylite_vfs_name()), SQLITE_OK,
        "open sqlite for physical table missing check");
    int rc = SQLITE_OK;

    if (sqlite == NULL) {
        return failures + 1;
    }

    rc = sqlite3_prepare_v2(sqlite, "SELECT 1 FROM sqlite_schema WHERE type='table' AND name=?", -1,
                            &stmt, NULL);
    failures += expect_sqlite_status(rc, SQLITE_OK, "prepare physical table missing check");
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, lookup->table_name, -1, SQLITE_STATIC);
        failures += expect_sqlite_status(sqlite3_step(stmt), SQLITE_DONE, "physical table missing");
    }
    sqlite3_finalize(stmt);
    sqlite3_close(sqlite);
    return failures;
}

static int expect_sqlite_physical_int64(const char *path, const char *physical_name,
                                        const char *expression, const char *tail, int64_t expected,
                                        const char *context)
{
    return expect_sqlite_physical_value(&(const struct sqlite_physical_value_expectation){
        .path = path,
        .physical_name = physical_name,
        .expression = expression,
        .tail = tail,
        .expected_type = SQLITE_INTEGER,
        .expected_int = expected,
        .context = context,
    });
}

static int expect_sqlite_physical_text(const char *path, const char *physical_name,
                                       const char *expression, const char *tail,
                                       const char *expected, const char *context)
{
    return expect_sqlite_physical_value(&(const struct sqlite_physical_value_expectation){
        .path = path,
        .physical_name = physical_name,
        .expression = expression,
        .tail = tail,
        .expected_type = SQLITE_TEXT,
        .expected_text = expected,
        .context = context,
    });
}

static int expect_sqlite_physical_null(const char *path, const char *physical_name,
                                       const char *expression, const char *tail,
                                       const char *context)
{
    return expect_sqlite_physical_value(&(const struct sqlite_physical_value_expectation){
        .path = path,
        .physical_name = physical_name,
        .expression = expression,
        .tail = tail,
        .expected_type = SQLITE_NULL,
        .context = context,
    });
}

static int expect_sqlite_physical_not_null(const char *path, const char *physical_name,
                                           const char *expression, const char *tail,
                                           const char *context)
{
    return expect_sqlite_physical_value(&(const struct sqlite_physical_value_expectation){
        .path = path,
        .physical_name = physical_name,
        .expression = expression,
        .tail = tail,
        .expected_type = -1,
        .context = context,
    });
}

static int expect_sqlite_physical_value(const struct sqlite_physical_value_expectation *expected)
{
    sqlite3 *sqlite = NULL;
    sqlite3_stmt *stmt = NULL;
    char *sql =
        sqlite3_mprintf("SELECT %s FROM \"%w\" %s", expected->expression, expected->physical_name,
                        expected->tail == NULL ? "" : expected->tail);
    int failures = 0;
    int rc = SQLITE_OK;

    if (sql == NULL) {
        fprintf(stderr, "%s: out of memory while building sqlite query\n", expected->context);
        return 1;
    }

    failures += expect_sqlite_status(
        sqlite3_open_v2(expected->path, &sqlite, SQLITE_OPEN_READONLY, mylite_vfs_name()),
        SQLITE_OK, "open sqlite for physical value check");
    if (sqlite == NULL) {
        sqlite3_free(sql);
        return failures + 1;
    }

    rc = sqlite3_prepare_v2(sqlite, sql, -1, &stmt, NULL);
    failures += expect_sqlite_status(rc, SQLITE_OK, expected->context);
    if (rc == SQLITE_OK) {
        rc = sqlite3_step(stmt);
        failures += expect_sqlite_status(rc, SQLITE_ROW, expected->context);
    }
    if (rc == SQLITE_ROW && expected->expected_type == SQLITE_INTEGER) {
        failures += expect_int64((int64_t)sqlite3_column_int64(stmt, 0), expected->expected_int,
                                 expected->context);
    } else if (rc == SQLITE_ROW && expected->expected_type == SQLITE_TEXT) {
        failures += expect_string((const char *)sqlite3_column_text(stmt, 0),
                                  expected->expected_text, expected->context);
    } else if (rc == SQLITE_ROW && expected->expected_type == SQLITE_NULL) {
        if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
            fprintf(stderr, "%s: expected sqlite null\n", expected->context);
            failures = 1;
        }
    } else if (rc == SQLITE_ROW && expected->expected_type == -1) {
        if (sqlite3_column_type(stmt, 0) == SQLITE_NULL) {
            fprintf(stderr, "%s: expected sqlite non-null value\n", expected->context);
            failures = 1;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(sqlite);
    sqlite3_free(sql);
    return failures;
}

static void remove_runtime_test_files(void)
{
    (void)remove(MYLITE_RUNTIME_TEST_FILE_PATH);
    (void)remove(MYLITE_RUNTIME_TEST_FILE_PATH "-journal");
    (void)remove(MYLITE_RUNTIME_TEST_FILE_PATH "-wal");
    (void)remove(MYLITE_RUNTIME_TEST_FILE_PATH "-shm");
}

static int read_file_at(const char *path, long offset, unsigned char *buffer, size_t size)
{
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        fprintf(stderr, "failed to open '%s'\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek '%s' to %ld\n", path, offset);
        fclose(file);
        return 1;
    }

    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    if (bytes_read != size) {
        fprintf(stderr, "expected to read %zu bytes from '%s', got %zu\n", size, path, bytes_read);
        return 1;
    }

    return 0;
}

static int exec_sqlite(sqlite3 *database, const char *sql)
{
    char *error_message = NULL;
    int rc = sqlite3_exec(database, sql, NULL, NULL, &error_message);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "sqlite exec '%s' failed: %s\n", sql,
                error_message == NULL ? sqlite3_errstr(rc) : error_message);
        sqlite3_free(error_message);
        return 1;
    }

    return 0;
}

static int expect_default_sqlite_rejects_mylite(const char *path)
{
    sqlite3 *sqlite = NULL;
    sqlite3_stmt *stmt = NULL;
    int failures = 0;
    int rc = sqlite3_open_v2(path, &sqlite, SQLITE_OPEN_READWRITE, NULL);

    failures += expect_sqlite_status(rc, SQLITE_OK, "open default sqlite view");
    if (sqlite != NULL) {
        rc = sqlite3_prepare_v2(sqlite, "SELECT name FROM sqlite_schema", -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            fprintf(stderr, "default sqlite unexpectedly accepted a .mylite file\n");
            failures = 1;
        }
        sqlite3_finalize(stmt);
        sqlite3_close(sqlite);
    }

    return failures;
}

static int expect_sqlite_status(int actual, int expected, const char *context)
{
    if (actual != expected) {
        fprintf(stderr, "%s: expected sqlite %s, got %s\n", context, sqlite3_errstr(expected),
                sqlite3_errstr(actual));
        return 1;
    }

    return 0;
}

static int expect_status(int actual, int expected, const char *context)
{
    if (actual != expected) {
        fprintf(stderr, "%s: expected %s, got %s\n", context, mylite_status_name(expected),
                mylite_status_name(actual));
        return 1;
    }

    return 0;
}

static int expect_int(int actual, int expected, const char *context)
{
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context)
{
    if (actual != expected) {
        fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_u16(unsigned int actual, unsigned int expected, const char *context)
{
    if (actual != expected) {
        fprintf(stderr, "%s: expected %u, got %u\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_string(const char *actual, const char *expected, const char *context)
{
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", context, expected,
                actual == NULL ? "(null)" : actual);
        return 1;
    }

    return 0;
}

static int expect_unsigned_decimal_text(const char *actual, const char *context)
{
    if (actual == NULL || actual[0] == '\0') {
        fprintf(stderr, "%s: expected unsigned decimal text, got '%s'\n", context,
                actual == NULL ? "(null)" : actual);
        return 1;
    }
    for (const char *cursor = actual; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9') {
            fprintf(stderr, "%s: expected unsigned decimal text, got '%s'\n", context, actual);
            return 1;
        }
    }
    return 0;
}

static int expect_null_text(const char *actual, const char *context)
{
    if (actual != NULL) {
        fprintf(stderr, "%s: expected null, got '%s'\n", context, actual);
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *expected_fragment, const char *context)
{
    if (actual == NULL || strstr(actual, expected_fragment) == NULL) {
        fprintf(stderr, "%s: expected '%s' to contain '%s'\n", context,
                actual == NULL ? "(null)" : actual, expected_fragment);
        return 1;
    }

    return 0;
}

static int expect_bytes(const unsigned char *actual, const void *expected, size_t size,
                        const char *context)
{
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte sequence mismatch\n", context);
        return 1;
    }

    return 0;
}
