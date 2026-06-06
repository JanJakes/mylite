#include <mylite/mylite.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

#define ARRAY_COUNT(items) (sizeof(items) / sizeof((items)[0]))

enum {
    test_path_capacity = 1024,
    test_path_suffix_capacity = 16,
    metadata_query_capacity = 512,
    metadata_ordinal_text_capacity = 16,
    metadata_data_type_column = 5,
    metadata_character_maximum_length_column = 6,
    metadata_character_octet_length_column = 7,
    metadata_numeric_precision_column = 8,
    metadata_numeric_scale_column = 9,
    metadata_datetime_precision_column = 10,
    metadata_character_set_name_column = 11,
    metadata_collation_name_column = 12,
    metadata_column_type_column = 13,
    metadata_privileges_column = 14,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_statement {
    const char *sql;
    const char *context;
};

struct expected_information_schema_column {
    const char *name;
    const char *column_default;
    const char *is_nullable;
    const char *data_type;
    const char *character_maximum_length;
    const char *character_octet_length;
    const char *numeric_precision;
    const char *numeric_scale;
    const char *character_set_name;
    const char *collation_name;
    const char *column_type;
};

static const char *const files_column_names[] = {
    "FILE_ID",
    "FILE_NAME",
    "FILE_TYPE",
    "TABLESPACE_NAME",
    "TABLE_CATALOG",
    "TABLE_SCHEMA",
    "TABLE_NAME",
    "LOGFILE_GROUP_NAME",
    "LOGFILE_GROUP_NUMBER",
    "ENGINE",
    "FULLTEXT_KEYS",
    "DELETED_ROWS",
    "UPDATE_COUNT",
    "FREE_EXTENTS",
    "TOTAL_EXTENTS",
    "EXTENT_SIZE",
    "INITIAL_SIZE",
    "MAXIMUM_SIZE",
    "AUTOEXTEND_SIZE",
    "CREATION_TIME",
    "LAST_UPDATE_TIME",
    "LAST_ACCESS_TIME",
    "RECOVER_TIME",
    "TRANSACTION_COUNTER",
    "VERSION",
    "ROW_FORMAT",
    "TABLE_ROWS",
    "AVG_ROW_LENGTH",
    "DATA_LENGTH",
    "MAX_DATA_LENGTH",
    "INDEX_LENGTH",
    "DATA_FREE",
    "CREATE_TIME",
    "UPDATE_TIME",
    "CHECK_TIME",
    "CHECKSUM",
    "STATUS",
    "EXTRA",
};

static const char *const ibdata1_values[] = {
    "0",        "./ibdata1", "TABLESPACE", "innodb_system",
    "",         NULL,        NULL,         NULL,
    NULL,       "InnoDB",    NULL,         NULL,
    NULL,       "2",         "12",         "1048576",
    "12582912", NULL,        "67108864",   NULL,
    NULL,       NULL,        NULL,         NULL,
    NULL,       NULL,        NULL,         NULL,
    NULL,       NULL,        NULL,         "6291456",
    NULL,       NULL,        NULL,         NULL,
    "NORMAL",   NULL,
};

static const struct expected_information_schema_column files_columns_metadata[] = {
    {"FILE_ID", NULL, "YES", "bigint", NULL, NULL, "19", "0", NULL, NULL, "bigint"},
    {"FILE_NAME",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"FILE_TYPE",
     NULL,
     "YES",
     "varchar",
     "256",
     "768",
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(256)"},
    {"TABLESPACE_NAME",
     NULL,
     "NO",
     "varchar",
     "268",
     "804",
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "varchar(268)"},
    {"TABLE_CATALOG",
     "",
     "NO",
     "varchar",
     "0",
     "0",
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(0)"},
    {"TABLE_SCHEMA", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"},
    {"TABLE_NAME", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"},
    {"LOGFILE_GROUP_NAME",
     NULL,
     "YES",
     "varchar",
     "256",
     "768",
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(256)"},
    {"LOGFILE_GROUP_NUMBER", NULL, "YES", "bigint", NULL, NULL, "19", "0", NULL, NULL, "bigint"},
    {"ENGINE",
     NULL,
     "NO",
     "varchar",
     "64",
     "192",
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(64)"},
    {"FULLTEXT_KEYS", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"},
    {"DELETED_ROWS", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"},
    {"UPDATE_COUNT", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"},
    {"FREE_EXTENTS", NULL, "YES", "bigint", NULL, NULL, "19", "0", NULL, NULL, "bigint"},
    {"TOTAL_EXTENTS", NULL, "YES", "bigint", NULL, NULL, "19", "0", NULL, NULL, "bigint"},
    {"EXTENT_SIZE", NULL, "YES", "bigint", NULL, NULL, "19", "0", NULL, NULL, "bigint"},
    {"INITIAL_SIZE", NULL, "YES", "bigint", NULL, NULL, "19", "0", NULL, NULL, "bigint"},
    {"MAXIMUM_SIZE", NULL, "YES", "bigint", NULL, NULL, "19", "0", NULL, NULL, "bigint"},
    {"AUTOEXTEND_SIZE", NULL, "YES", "bigint", NULL, NULL, "19", "0", NULL, NULL, "bigint"},
    {"CREATION_TIME", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"},
    {"LAST_UPDATE_TIME", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"
    },
    {"LAST_ACCESS_TIME", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"
    },
    {"RECOVER_TIME", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"},
    {"TRANSACTION_COUNTER",
     NULL,
     "YES",
     "varbinary",
     "0",
     "0",
     NULL,
     NULL,
     NULL,
     NULL,
     "varbinary(0)"},
    {"VERSION", NULL, "YES", "bigint", NULL, NULL, "19", "0", NULL, NULL, "bigint"},
    {"ROW_FORMAT",
     NULL,
     "YES",
     "varchar",
     "256",
     "768",
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(256)"},
    {"TABLE_ROWS", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"},
    {"AVG_ROW_LENGTH", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"},
    {"DATA_LENGTH", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"},
    {"MAX_DATA_LENGTH", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"},
    {"INDEX_LENGTH", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"},
    {"DATA_FREE", NULL, "YES", "bigint", NULL, NULL, "19", "0", NULL, NULL, "bigint"},
    {"CREATE_TIME", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"},
    {"UPDATE_TIME", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"},
    {"CHECK_TIME", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"},
    {"CHECKSUM", NULL, "YES", "varbinary", "0", "0", NULL, NULL, NULL, NULL, "varbinary(0)"},
    {"STATUS",
     NULL,
     "YES",
     "varchar",
     "256",
     "768",
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(256)"},
    {"EXTRA",
     NULL,
     "YES",
     "varchar",
     "256",
     "768",
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(256)"},
};

static int test_information_schema_files_queries(void);
static int setup_indexed_table(mylite_db *database);
static int expect_statement_ok(mylite_db *database, struct expected_statement expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_columns_metadata(mylite_db *database);
static int expect_row_count_status(mylite_db *database, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);

int main(void) {
    int failures = 0;

    failures += test_information_schema_files_queries();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_files_queries(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_six[] = {"6"};
    static const char *const count_two[] = {"2"};
    static const char *const files_core_columns[] = {
        "FILE_ID",
        "FILE_NAME",
        "FILE_TYPE",
        "TABLESPACE_NAME",
        "ENGINE",
        "FREE_EXTENTS",
        "TOTAL_EXTENTS",
        "EXTENT_SIZE",
        "INITIAL_SIZE",
        "AUTOEXTEND_SIZE",
        "DATA_FREE",
        "STATUS",
    };
    static const char *const files_core_values[] = {
        "0",          "./ibdata1",
        "TABLESPACE", "innodb_system",
        "InnoDB",     "2",
        "12",         "1048576",
        "12582912",   "67108864",
        "6291456",    "NORMAL",
        "4294967293", "./ibtmp1",
        "TEMPORARY",  "innodb_temporary",
        "InnoDB",     "2",
        "12",         "1048576",
        "12582912",   "67108864",
        "6291456",    "NORMAL",
        "4294967294", "./mysql.ibd",
        "TABLESPACE", "mysql",
        "InnoDB",     "1",
        "31",         "1048576",
        "0",          "1048576",
        "4194304",    "NORMAL",
        "1",          "./sys/sys_config.ibd",
        "TABLESPACE", "sys/sys_config",
        "InnoDB",     "0",
        "0",          "1048576",
        "0",          "1048576",
        "0",          "NORMAL",
        "4294967279", "./undo_001",
        "UNDO LOG",   "innodb_undo_001",
        "InnoDB",     "2",
        "16",         "1048576",
        "16777216",   "16777216",
        "6291456",    "NORMAL",
        "4294967278", "./undo_002",
        "UNDO LOG",   "innodb_undo_002",
        "InnoDB",     "2",
        "16",         "1048576",
        "16777216",   "16777216",
        "6291456",    "NORMAL",
    };
    static const char *const alias_columns[] = {"FILE_NAME", "DATA_FREE"};
    static const char *const mysql_file_values[] = {"./mysql.ibd", "4194304"};
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
        "FILES",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "queries") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open files db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.FILES WHERE FILE_NAME = './ibdata1'",
            .column_names = files_column_names,
            .column_count = ARRAY_COUNT(files_column_names),
            .values = ibdata1_values,
            .row_count = 1U,
            .context = "files ibdata1 full row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.FILES",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_six,
            .row_count = 1U,
            .context = "files count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.files WHERE FILE_TYPE = 'UNDO LOG'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_two,
            .row_count = 1U,
            .context = "case-insensitive files undo count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FILE_ID, FILE_NAME, FILE_TYPE, TABLESPACE_NAME, ENGINE, FREE_EXTENTS, "
                   "TOTAL_EXTENTS, EXTENT_SIZE, INITIAL_SIZE, AUTOEXTEND_SIZE, DATA_FREE, STATUS "
                   "FROM INFORMATION_SCHEMA.FILES ORDER BY FILE_NAME",
            .column_names = files_core_columns,
            .column_count = ARRAY_COUNT(files_core_columns),
            .values = files_core_values,
            .row_count = ARRAY_COUNT(files_core_values) / ARRAY_COUNT(files_core_columns),
            .context = "files ordered rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT f.FILE_NAME, f.DATA_FREE FROM INFORMATION_SCHEMA.FILES AS f "
                   "WHERE f.TABLESPACE_NAME = 'mysql'",
            .column_names = alias_columns,
            .column_count = ARRAY_COUNT(alias_columns),
            .values = mysql_file_values,
            .row_count = 1U,
            .context = "files alias row",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE information_schema",
            .context = "use information_schema for files",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM FILES WHERE ENGINE = 'InnoDB'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_six,
            .row_count = 1U,
            .context = "unqualified files count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'FILES'",
            .column_names = system_table_columns,
            .column_count = ARRAY_COUNT(system_table_columns),
            .values = system_table_values,
            .row_count = 1U,
            .context = "files system table row",
        }
    );
    failures += expect_columns_metadata(database);
    failures += expect_row_count_status(database, "files row count status");
    failures += setup_indexed_table(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.FILES",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_six,
            .row_count = 1U,
            .context = "files remains static after table activity",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int setup_indexed_table(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE SCHEMA app",
            .context = "create files schema",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE app",
            .context = "use files schema",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE TABLE file_probe (id INT PRIMARY KEY, v INT, KEY v_key (v))",
            .context = "create indexed table for files",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "INSERT INTO file_probe VALUES (1, 10), (2, 20)",
            .context = "insert indexed table rows for files",
        }
    );

    return failures;
}

static int expect_statement_ok(mylite_db *database, struct expected_statement expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    } else {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
        mylite_result_free(result);
        return failures + 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", expected.context);
        return failures + 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            expected.column_names[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_text_or_null(
                mylite_result_value_text(result, row, column),
                expected.values[value_index],
                expected.context
            );
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_columns_metadata(mylite_db *database) {
    static const char *const metadata_column_names[] = {
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
    mylite_result *result = NULL;
    char sql[metadata_query_capacity];
    int failures = 0;
    int written = snprintf(
        sql,
        sizeof(sql),
        "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "
        "DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, "
        "NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "
        "COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'FILES' "
        "ORDER BY ORDINAL_POSITION"
    );
    int rc = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "files columns metadata: SQL buffer too small\n");
        return 1;
    }

    rc = mylite_execute(database, sql, strlen(sql), &result);
    failures += expect_int(rc, MYLITE_OK, "files columns metadata");
    if (rc != MYLITE_OK) {
        fprintf(stderr, "files columns metadata: %s\n", mylite_errmsg(database));
        mylite_result_free(result);
        return failures + 1;
    }
    if (result == NULL) {
        fprintf(stderr, "files columns metadata: expected result object\n");
        return failures + 1;
    }

    failures += expect_size(
        mylite_result_column_count(result),
        ARRAY_COUNT(metadata_column_names),
        "files columns metadata"
    );
    failures += expect_size(
        mylite_result_row_count(result),
        ARRAY_COUNT(files_columns_metadata),
        "files columns metadata"
    );
    failures += expect_int64(mylite_result_affected_rows(result), 0, "files columns metadata");
    failures += expect_size(mylite_result_warning_count(result), 0U, "files columns metadata");

    for (size_t column = 0U; column < ARRAY_COUNT(metadata_column_names); ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            metadata_column_names[column],
            "files columns metadata"
        );
    }
    for (size_t row = 0U; row < ARRAY_COUNT(files_columns_metadata); ++row) {
        const struct expected_information_schema_column *expected = &files_columns_metadata[row];
        char ordinal_text[metadata_ordinal_text_capacity];

        written = snprintf(ordinal_text, sizeof(ordinal_text), "%zu", row + 1U);
        if (written < 0 || (size_t)written >= sizeof(ordinal_text)) {
            fprintf(stderr, "files columns metadata: ordinal overflow\n");
            failures += 1;
            continue;
        }

        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 0U),
            "FILES",
            "files columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 1U),
            expected->name,
            "files columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 2U),
            ordinal_text,
            "files columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 3U),
            expected->column_default,
            "files columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 4U),
            expected->is_nullable,
            "files columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_data_type_column),
            expected->data_type,
            "files columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_character_maximum_length_column),
            expected->character_maximum_length,
            "files columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_character_octet_length_column),
            expected->character_octet_length,
            "files columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_numeric_precision_column),
            expected->numeric_precision,
            "files columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_numeric_scale_column),
            expected->numeric_scale,
            "files columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_datetime_precision_column),
            NULL,
            "files columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_character_set_name_column),
            expected->character_set_name,
            "files columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_collation_name_column),
            expected->collation_name,
            "files columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_column_type_column),
            expected->column_type,
            "files columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_privileges_column),
            "select",
            "files columns metadata"
        );
    }

    mylite_result_free(result);
    return failures;
}

static int expect_row_count_status(mylite_db *database, const char *context) {
    static const char *const status_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const status_values[] = {"0", "-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .column_names = status_columns,
            .column_count = 2U,
            .values = status_values,
            .row_count = 1U,
            .context = context,
        }
    );
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "%s/mylite_information_schema_files_%d_%s.mylite",
        P_tmpdir,
        current_process_id(),
        name
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
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
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}
