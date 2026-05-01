#include <mylite/mylite.h>

#include "mylite_file_format.h"
#include "mylite_internal.h"
#include "mylite_vfs.h"
#include "sqlite3.h"

#include <inttypes.h>
#include <stdio.h>
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
    tables_collation_column = 17,
    tables_comment_column = 20,
    information_schema_table_version = 10,
};

struct expected_schemata_row {
    const char *schema_name;
    const char *character_set;
    const char *collation;
    const char *encryption;
};

static int test_select_integer_literal(void);
static int test_select_integer_literal_with_semicolon(void);
static int test_schema_lifecycle(void);
static int test_character_set_collation_foundation(void);
static int test_core_metadata_catalog(void);
static int test_mylite_file_preamble_and_vfs_payload(void);
static int test_mylite_open_rejects_plain_sqlite(void);
static int test_unsupported_statement(void);
static int test_create_table_column_type_prepare_is_unsupported(void);
static int test_parse_error(void);
static int prepare_sql(mylite_db *database, const char *sql, int expected_status,
                       mylite_stmt **out_stmt);
static int execute_sql(mylite_db *database, const char *sql, int expected_step_status);
static int expect_information_schema_schemata_row(mylite_db *database,
                                                  const struct expected_schemata_row *expected);
static int expect_no_information_schema_schemata_row(mylite_db *database, const char *schema_name);
static int expect_information_schema_tables_views(mylite_db *database);
static int expect_no_information_schema_table_schema_row(mylite_db *database,
                                                         const char *schema_name);
static int expect_no_information_schema_table_name_row(mylite_db *database, const char *table_name);
static int expect_empty_information_schema_table(mylite_db *database, const char *sql,
                                                 const char *const *columns, int column_count);
static int expect_show_database_rows(mylite_db *database, const char *required,
                                     const char *forbidden);
static int expect_connection_state(mylite_db *database, const char *client, const char *connection,
                                   const char *results, const char *collation, const char *context);
static int expect_column_names(const mylite_stmt *stmt, const char *const *expected, int count,
                               const char *context);
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
    failures += test_create_table_column_type_prepare_is_unsupported();
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

static int test_create_table_column_type_prepare_is_unsupported(void)
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
                            MYLITE_UNSUPPORTED, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "parse-only CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
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
                            MYLITE_UNSUPPORTED, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "parse-only string/binary CREATE TABLE returned a statement handle\n");
        failures = 1;
        mylite_finalize(stmt);
        stmt = NULL;
    }
    failures += expect_no_information_schema_table_name_row(database, "string_binary_types");
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
    failures += prepare_sql(database, "CREATE TABLE unsupported_attribute (a INT NOT NULL);",
                            MYLITE_PARSE_ERROR, &stmt);
    if (stmt != NULL) {
        fprintf(stderr, "unsupported-attribute CREATE TABLE returned a statement handle\n");
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
