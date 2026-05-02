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

static int test_select_integer_literal(void);
static int test_select_integer_literal_with_semicolon(void);
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
static int test_parse_error(void);
static int prepare_sql(mylite_db *database, const char *sql, int expected_status,
                       mylite_stmt **out_stmt);
static int expect_no_stmt_handle(mylite_stmt **stmt, const char *context);
static int execute_sql(mylite_db *database, const char *sql, int expected_step_status);
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
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "unsupported information_schema filter returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
    }
    failures += prepare_sql(database, "SELECT * FROM SCHEMATA", MYLITE_UNSUPPORTED, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "unqualified information_schema table returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
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
    failures += prepare_sql(database, "SELECT 1 + 2", MYLITE_UNSUPPORTED, &stmt);
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
