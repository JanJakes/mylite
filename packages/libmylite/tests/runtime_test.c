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
