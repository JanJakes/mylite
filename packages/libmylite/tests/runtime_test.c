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
    information_schema_view_count = 4,
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
    statistics_non_unique_column = 3,
    statistics_index_name_column = 5,
    statistics_seq_column = 6,
    statistics_column_name_column = 7,
    statistics_collation_column = 8,
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
    mysql_warning_ambiguous_column = 1052,
    mysql_warning_unknown_column = 1054,
    mysql_warning_incorrect_escape_arguments = 1210,
    mysql_warning_truncated_wrong_value = 1292,
    mysql_warning_savepoint_does_not_exist = 1305,
    mysql_warning_division_by_zero = 1365,
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

static int test_select_integer_literal(void);
static int test_select_integer_literal_with_semicolon(void);
static int test_expression_operator_foundation(void);
static int test_schema_lifecycle(void);
static int test_character_set_collation_foundation(void);
static int test_core_metadata_catalog(void);
static int test_mylite_file_preamble_and_vfs_payload(void);
static int test_mylite_open_rejects_plain_sqlite(void);
static int test_unsupported_statement(void);
static int test_create_table_base_execution(void);
static int test_create_table_prepare_has_no_side_effects(void);
static int test_drop_table_base_execution(void);
static int test_insert_values_execution(void);
static int test_insert_set_execution(void);
static int test_select_table_core_execution(void);
static int test_select_where_execution(void);
static int test_select_order_limit_offset_execution(void);
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
static int expect_select_row_count(mylite_db *database, const char *sql, int row_count,
                                   const char *context);
static int expect_information_schema_schemata_row(mylite_db *database,
                                                  const struct expected_schemata_row *expected);
static int expect_no_information_schema_schemata_row(mylite_db *database, const char *schema_name);
static int expect_information_schema_tables_views(mylite_db *database);
static int expect_no_information_schema_table_schema_row(mylite_db *database,
                                                         const char *schema_name);
static int expect_no_information_schema_table_name_row(mylite_db *database, const char *table_name);
static int expect_no_information_schema_column_table_name_row(mylite_db *database,
                                                              const char *table_name);
static int expect_no_information_schema_statistics_table_name_row(mylite_db *database,
                                                                  const char *table_name);
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
    failures += test_schema_lifecycle();
    failures += test_character_set_collation_foundation();
    failures += test_core_metadata_catalog();
    failures += test_mylite_file_preamble_and_vfs_payload();
    failures += test_mylite_open_rejects_plain_sqlite();
    failures += test_unsupported_statement();
    failures += test_create_table_base_execution();
    failures += test_create_table_prepare_has_no_side_effects();
    failures += test_drop_table_base_execution();
    failures += test_insert_values_execution();
    failures += test_insert_set_execution();
    failures += test_select_table_core_execution();
    failures += test_select_where_execution();
    failures += test_select_order_limit_offset_execution();
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
    failures += prepare_sql(database, "SELECT ROW(1,2) IN ((1,2))", MYLITE_PARSE_ERROR, &stmt);
    failures += expect_no_stmt_handle(&stmt, "row in deferred");

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
    failures += expect_prepare_error(database, "SELECT a + 1 FROM t", MYLITE_UNSUPPORTED,
                                     "Unsupported SELECT projection",
                                     "select unsupported expression projection");
    failures += expect_prepare_error(database, "SELECT 1 FROM t", MYLITE_UNSUPPORTED,
                                     "Unsupported SELECT projection",
                                     "select unsupported literal projection");

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
    enum { hidden_order_warning_count = 5 };
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
    failures += expect_prepare_error(database, "SELECT id + 1 AS x FROM t ORDER BY id",
                                     MYLITE_UNSUPPORTED, "Unsupported SELECT projection",
                                     "unreferenced order projection expression");
    failures += expect_prepare_error(database, "SELECT 1 FROM t ORDER BY id", MYLITE_UNSUPPORTED,
                                     "Unsupported SELECT projection",
                                     "literal projection remains unsupported with order");

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

            failures += expect_string(mylite_column_text(stmt, column), expected, context);
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
        "COLUMNS",
        "SCHEMATA",
        "STATISTICS",
        "TABLES",
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
