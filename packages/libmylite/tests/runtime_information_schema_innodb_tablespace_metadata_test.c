#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    size_t warning_count;
    const char *context;
};

struct expected_status {
    const char *warning_count;
    const char *row_count;
    const char *context;
};

struct expected_statement {
    const char *sql;
    const char *context;
};

static int test_information_schema_innodb_tablespace_metadata_queries(void);
static int test_information_schema_innodb_tablespace_metadata_file_backed_safety(void);
static int expect_statement_ok(mylite_db *database, struct expected_statement expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_status(mylite_db *database, struct expected_status expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_information_schema_innodb_tablespace_metadata_queries();
    failures += test_information_schema_innodb_tablespace_metadata_file_backed_safety();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_innodb_tablespace_metadata_queries(void) {
    static const char *const datafiles_columns[] = {
        "SPACE",
        "PATH",
    };
    static const char *const datafiles_rows[] = {
        "1",
        "./sys/sys_config.ibd",
        "4294967279",
        "./undo_001",
        "4294967278",
        "./undo_002",
        "0",
        "ibdata1",
    };
    static const char *const brief_columns[] = {
        "SPACE",
        "NAME",
        "PATH",
        "FLAG",
        "SPACE_TYPE",
    };
    static const char *const brief_rows[] = {
        "0",
        "innodb_system",
        "ibdata1",
        "18432",
        "System",
        "4294967279",
        "innodb_undo_001",
        "./undo_001",
        "0",
        "Single",
        "4294967278",
        "innodb_undo_002",
        "./undo_002",
        "0",
        "Single",
        "1",
        "sys/sys_config",
        "./sys/sys_config.ibd",
        "16417",
        "Single",
    };
    static const char *const tablespaces_columns[] = {
        "SPACE",
        "NAME",
        "FLAG",
        "ROW_FORMAT",
        "PAGE_SIZE",
        "ZIP_PAGE_SIZE",
        "SPACE_TYPE",
        "FS_BLOCK_SIZE",
        "FILE_SIZE",
        "ALLOCATED_SIZE",
        "AUTOEXTEND_SIZE",
        "SERVER_VERSION",
        "SPACE_VERSION",
        "ENCRYPTION",
        "STATE",
    };
    static const char *const tablespaces_rows[] = {
        "4294967293",
        "innodb_temporary",
        "4096",
        "Compact or Redundant",
        "16384",
        "0",
        "System",
        "4096",
        "12582912",
        "12582912",
        "0",
        "8.4.9",
        "1",
        "N",
        "normal",
        "4294967279",
        "innodb_undo_001",
        "0",
        "Undo",
        "16384",
        "0",
        "Undo",
        "4096",
        "16777216",
        "16777216",
        "0",
        "8.4.9",
        "1",
        "N",
        "active",
        "4294967278",
        "innodb_undo_002",
        "0",
        "Undo",
        "16384",
        "0",
        "Undo",
        "4096",
        "16777216",
        "16777216",
        "0",
        "8.4.9",
        "1",
        "N",
        "active",
        "4294967294",
        "mysql",
        "18432",
        "Any",
        "16384",
        "0",
        "General",
        "4096",
        "32505856",
        "32509952",
        "0",
        "8.4.9",
        "1",
        "N",
        "normal",
        "1",
        "sys/sys_config",
        "16417",
        "Dynamic",
        "16384",
        "0",
        "Single",
        "4096",
        "114688",
        "114688",
        "0",
        "8.4.9",
        "1",
        "N",
        "normal",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_four[] = {"4"};
    static const char *const count_five[] = {"5"};
    static const char *const count_three[] = {"3"};
    static const char *const count_two[] = {"2"};
    static const char *const space_column[] = {"SPACE"};
    static const char *const system_space[] = {"0"};
    static const char *const brief_name_column[] = {"NAME"};
    static const char *const sys_config_name[] = {"sys/sys_config"};
    static const char *const tablespace_alias_columns[] = {"NAME", "FILE_SIZE", "STATE"};
    static const char *const tablespace_alias_values[] = {"sys/sys_config", "114688", "normal"};
    static const char *const system_table_columns[] = {
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "DATA_LENGTH",
        "AUTO_INCREMENT",
    };
    static const char *const system_table_values[] = {
        "INNODB_DATAFILES",         "SYSTEM VIEW", NULL, "10", NULL, "0", "0", NULL,
        "INNODB_TABLESPACES",       "SYSTEM VIEW", NULL, "10", NULL, "0", "0", NULL,
        "INNODB_TABLESPACES_BRIEF", "SYSTEM VIEW", NULL, "10", NULL, "0", "0", NULL,
    };
    static const char *const columns_metadata_columns[] = {
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
        "PRIVILEGES",
    };
    static const char *const datafiles_columns_metadata_values[] = {
        "INNODB_DATAFILES",
        "SPACE",
        "1",
        NULL,
        "YES",
        "varbinary",
        "256",
        "256",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "varbinary(256)",
        "select",
        "INNODB_DATAFILES",
        "PATH",
        "2",
        NULL,
        "NO",
        "varchar",
        "512",
        "1536",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(512)",
        "select",
    };
    static const char *const tablespaces_columns_metadata_values[] = {
        "INNODB_TABLESPACES",
        "SPACE",
        "1",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int unsigned",
        "select",
        "INNODB_TABLESPACES",
        "NAME",
        "2",
        "",
        "NO",
        "varchar",
        "218",
        "655",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(655)",
        "select",
        "INNODB_TABLESPACES",
        "FLAG",
        "3",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int unsigned",
        "select",
        "INNODB_TABLESPACES",
        "ROW_FORMAT",
        "4",
        "",
        "YES",
        "varchar",
        "7",
        "22",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(22)",
        "select",
        "INNODB_TABLESPACES",
        "PAGE_SIZE",
        "5",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int unsigned",
        "select",
        "INNODB_TABLESPACES",
        "ZIP_PAGE_SIZE",
        "6",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int unsigned",
        "select",
        "INNODB_TABLESPACES",
        "SPACE_TYPE",
        "7",
        "",
        "YES",
        "varchar",
        "3",
        "10",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(10)",
        "select",
        "INNODB_TABLESPACES",
        "FS_BLOCK_SIZE",
        "8",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int unsigned",
        "select",
        "INNODB_TABLESPACES",
        "FILE_SIZE",
        "9",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_TABLESPACES",
        "ALLOCATED_SIZE",
        "10",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_TABLESPACES",
        "AUTOEXTEND_SIZE",
        "11",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_TABLESPACES",
        "SERVER_VERSION",
        "12",
        "",
        "YES",
        "varchar",
        "3",
        "10",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(10)",
        "select",
        "INNODB_TABLESPACES",
        "SPACE_VERSION",
        "13",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int unsigned",
        "select",
        "INNODB_TABLESPACES",
        "ENCRYPTION",
        "14",
        "",
        "YES",
        "varchar",
        "0",
        "1",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(1)",
        "select",
        "INNODB_TABLESPACES",
        "STATE",
        "15",
        "",
        "YES",
        "varchar",
        "3",
        "10",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(10)",
        "select",
    };
    static const char *const brief_columns_metadata_values[] = {
        "INNODB_TABLESPACES_BRIEF",
        "SPACE",
        "1",
        NULL,
        "YES",
        "varbinary",
        "256",
        "256",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "varbinary(256)",
        "select",
        "INNODB_TABLESPACES_BRIEF",
        "NAME",
        "2",
        NULL,
        "NO",
        "varchar",
        "268",
        "804",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(268)",
        "select",
        "INNODB_TABLESPACES_BRIEF",
        "PATH",
        "3",
        NULL,
        "NO",
        "varchar",
        "512",
        "1536",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(512)",
        "select",
        "INNODB_TABLESPACES_BRIEF",
        "FLAG",
        "4",
        NULL,
        "YES",
        "varbinary",
        "256",
        "256",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "varbinary(256)",
        "select",
        "INNODB_TABLESPACES_BRIEF",
        "SPACE_TYPE",
        "5",
        "",
        "NO",
        "varchar",
        "7",
        "21",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(7)",
        "select",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open innodb metadata db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.INNODB_DATAFILES LIMIT 0",
            .column_names = datafiles_columns,
            .column_count = sizeof(datafiles_columns) / sizeof(datafiles_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .context = "innodb datafiles wildcard columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SPACE, PATH FROM INFORMATION_SCHEMA.INNODB_DATAFILES ORDER BY PATH",
            .column_names = datafiles_columns,
            .column_count = sizeof(datafiles_columns) / sizeof(datafiles_columns[0]),
            .values = datafiles_rows,
            .row_count = sizeof(datafiles_rows) / sizeof(datafiles_rows[0]) /
                         (sizeof(datafiles_columns) / sizeof(datafiles_columns[0])),
            .warning_count = 0U,
            .context = "innodb datafiles rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF LIMIT 0",
            .column_names = brief_columns,
            .column_count = sizeof(brief_columns) / sizeof(brief_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .context = "innodb tablespaces brief wildcard columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SPACE, NAME, PATH, FLAG, SPACE_TYPE "
                   "FROM INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF ORDER BY NAME",
            .column_names = brief_columns,
            .column_count = sizeof(brief_columns) / sizeof(brief_columns[0]),
            .values = brief_rows,
            .row_count = sizeof(brief_rows) / sizeof(brief_rows[0]) /
                         (sizeof(brief_columns) / sizeof(brief_columns[0])),
            .warning_count = 0U,
            .context = "innodb tablespaces brief rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.INNODB_TABLESPACES LIMIT 0",
            .column_names = tablespaces_columns,
            .column_count = sizeof(tablespaces_columns) / sizeof(tablespaces_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .context = "innodb tablespaces wildcard columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SPACE, NAME, FLAG, ROW_FORMAT, PAGE_SIZE, ZIP_PAGE_SIZE, "
                   "SPACE_TYPE, FS_BLOCK_SIZE, FILE_SIZE, ALLOCATED_SIZE, "
                   "AUTOEXTEND_SIZE, SERVER_VERSION, SPACE_VERSION, ENCRYPTION, STATE "
                   "FROM INFORMATION_SCHEMA.INNODB_TABLESPACES ORDER BY NAME",
            .column_names = tablespaces_columns,
            .column_count = sizeof(tablespaces_columns) / sizeof(tablespaces_columns[0]),
            .values = tablespaces_rows,
            .row_count = sizeof(tablespaces_rows) / sizeof(tablespaces_rows[0]) /
                         (sizeof(tablespaces_columns) / sizeof(tablespaces_columns[0])),
            .warning_count = 0U,
            .context = "innodb tablespaces rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.innodb_datafiles",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_four,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "innodb datafiles lower-case count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.innodb_tablespaces",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_five,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "innodb tablespaces lower-case count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SPACE FROM INFORMATION_SCHEMA.INNODB_DATAFILES WHERE PATH = 'ibdata1'",
            .column_names = space_column,
            .column_count = 1U,
            .values = system_space,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "innodb datafiles system path predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT b.NAME FROM INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF AS b "
                   "WHERE b.SPACE = '1'",
            .column_names = brief_name_column,
            .column_count = 1U,
            .values = sys_config_name,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "innodb tablespaces brief alias predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT t.NAME, t.FILE_SIZE, t.STATE "
                   "FROM INFORMATION_SCHEMA.INNODB_TABLESPACES AS t "
                   "WHERE t.NAME = 'sys/sys_config'",
            .column_names = tablespace_alias_columns,
            .column_count = sizeof(tablespace_alias_columns) / sizeof(tablespace_alias_columns[0]),
            .values = tablespace_alias_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "innodb tablespaces alias predicate",
        }
    );
    failures += expect_status(
        database,
        (struct expected_status){
            .warning_count = "0",
            .row_count = "-1",
            .context = "innodb tablespaces brief alias status",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE information_schema",
            .context = "use information_schema",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INNODB_TABLESPACES_BRIEF WHERE SPACE_TYPE = 'Single'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_three,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "selected information_schema innodb brief count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INNODB_TABLESPACES WHERE SPACE_TYPE = 'Undo'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_two,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "selected information_schema innodb tablespaces count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME IN "
                   "('INNODB_DATAFILES', 'INNODB_TABLESPACES', 'INNODB_TABLESPACES_BRIEF') "
                   "ORDER BY TABLE_NAME",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = sizeof(system_table_values) / sizeof(system_table_values[0]) /
                         (sizeof(system_table_columns) / sizeof(system_table_columns[0])),
            .warning_count = 0U,
            .context = "innodb tablespace metadata system table rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "
                   "DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'INNODB_DATAFILES' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = datafiles_columns_metadata_values,
            .row_count = sizeof(datafiles_columns_metadata_values) /
                         sizeof(datafiles_columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .warning_count = 0U,
            .context = "innodb datafiles columns metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "
                   "DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'INNODB_TABLESPACES' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = tablespaces_columns_metadata_values,
            .row_count = sizeof(tablespaces_columns_metadata_values) /
                         sizeof(tablespaces_columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .warning_count = 0U,
            .context = "innodb tablespaces columns metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "
                   "DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'INNODB_TABLESPACES_BRIEF' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = brief_columns_metadata_values,
            .row_count = sizeof(brief_columns_metadata_values) /
                         sizeof(brief_columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .warning_count = 0U,
            .context = "innodb tablespaces brief columns metadata",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_information_schema_innodb_tablespace_metadata_file_backed_safety(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_four[] = {"4"};
    static const char *const count_five[] = {"5"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "file") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file innodb metadata db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_DATAFILES",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_four,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "file innodb datafiles count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TABLESPACES",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_five,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "file innodb tablespaces count",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after innodb tablespace metadata query"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int expect_statement_ok(mylite_db *database, struct expected_statement expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            expected.context,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            expected.context,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", expected.context);
        return 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);

    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            size_t value_index = (row_index * expected.column_count) + column_index;

            failures += expect_text_or_null(
                mylite_result_value_text(result, row_index, column_index),
                expected.values[value_index],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_status(mylite_db *database, struct expected_status expected) {
    static const char *const status_columns[] = {"@@warning_count", "ROW_COUNT()"};
    const char *const status_values[] = {expected.warning_count, expected.row_count};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .column_names = status_columns,
            .column_count = 2U,
            .values = status_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = expected.context,
        }
    );
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_information_schema_innodb_tablespace_metadata_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path for %s\n", name);
        return 1;
    }
    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(related)) {
        remove(related);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "%s: expected readable file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read expected bytes\n", path);
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
        return 1;
    }
    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %s, got %s\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
