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
    const char *is_nullable;
    const char *data_type;
    const char *character_maximum_length;
    const char *character_octet_length;
    const char *character_set_name;
    const char *collation_name;
    const char *column_type;
};

static const struct expected_information_schema_column buffer_page_columns[] = {
    {"POOL_ID", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"BLOCK_ID", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"SPACE", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"PAGE_NUMBER", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"PAGE_TYPE", "YES", "varchar", "21", "64", "utf8mb3", "utf8mb3_general_ci", "varchar(64)"},
    {"FLUSH_TYPE", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"FIX_COUNT", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"IS_HASHED", "YES", "varchar", "1", "3", "utf8mb3", "utf8mb3_general_ci", "varchar(3)"},
    {"NEWEST_MODIFICATION", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"OLDEST_MODIFICATION", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"ACCESS_TIME", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"TABLE_NAME",
     "YES",
     "varchar",
     "341",
     "1024",
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(1024)"},
    {"INDEX_NAME",
     "YES",
     "varchar",
     "341",
     "1024",
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(1024)"},
    {"NUMBER_RECORDS", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"DATA_SIZE", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"COMPRESSED_SIZE", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"PAGE_STATE", "YES", "varchar", "21", "64", "utf8mb3", "utf8mb3_general_ci", "varchar(64)"},
    {"IO_FIX", "YES", "varchar", "21", "64", "utf8mb3", "utf8mb3_general_ci", "varchar(64)"},
    {"IS_OLD", "YES", "varchar", "1", "3", "utf8mb3", "utf8mb3_general_ci", "varchar(3)"},
    {"FREE_PAGE_CLOCK", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"IS_STALE", "YES", "varchar", "1", "3", "utf8mb3", "utf8mb3_general_ci", "varchar(3)"},
};

static const struct expected_information_schema_column buffer_page_lru_columns[] = {
    {"POOL_ID", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"LRU_POSITION", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"SPACE", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"PAGE_NUMBER", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"PAGE_TYPE", "YES", "varchar", "21", "64", "utf8mb3", "utf8mb3_general_ci", "varchar(64)"},
    {"FLUSH_TYPE", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"FIX_COUNT", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"IS_HASHED", "YES", "varchar", "1", "3", "utf8mb3", "utf8mb3_general_ci", "varchar(3)"},
    {"NEWEST_MODIFICATION", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"OLDEST_MODIFICATION", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"ACCESS_TIME", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"TABLE_NAME",
     "YES",
     "varchar",
     "341",
     "1024",
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(1024)"},
    {"INDEX_NAME",
     "YES",
     "varchar",
     "341",
     "1024",
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(1024)"},
    {"NUMBER_RECORDS", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"DATA_SIZE", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"COMPRESSED_SIZE", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
    {"COMPRESSED", "YES", "varchar", "1", "3", "utf8mb3", "utf8mb3_general_ci", "varchar(3)"},
    {"IO_FIX", "YES", "varchar", "21", "64", "utf8mb3", "utf8mb3_general_ci", "varchar(64)"},
    {"IS_OLD", "YES", "varchar", "1", "3", "utf8mb3", "utf8mb3_general_ci", "varchar(3)"},
    {"FREE_PAGE_CLOCK", "NO", "bigint", NULL, NULL, NULL, NULL, "bigint unsigned"},
};

static int test_information_schema_innodb_buffer_page_tables_queries(void);
static int setup_indexed_table(mylite_db *database);
static int expect_statement_ok(mylite_db *database, struct expected_statement expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_columns_metadata(
    mylite_db *database,
    const char *table_name,
    const struct expected_information_schema_column *expected_columns,
    size_t expected_column_count
);
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

    failures += test_information_schema_innodb_buffer_page_tables_queries();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_innodb_buffer_page_tables_queries(void) {
    static const char *const buffer_page_column_names[] = {
        "POOL_ID",
        "BLOCK_ID",
        "SPACE",
        "PAGE_NUMBER",
        "PAGE_TYPE",
        "FLUSH_TYPE",
        "FIX_COUNT",
        "IS_HASHED",
        "NEWEST_MODIFICATION",
        "OLDEST_MODIFICATION",
        "ACCESS_TIME",
        "TABLE_NAME",
        "INDEX_NAME",
        "NUMBER_RECORDS",
        "DATA_SIZE",
        "COMPRESSED_SIZE",
        "PAGE_STATE",
        "IO_FIX",
        "IS_OLD",
        "FREE_PAGE_CLOCK",
        "IS_STALE",
    };
    static const char *const buffer_page_lru_column_names[] = {
        "POOL_ID",
        "LRU_POSITION",
        "SPACE",
        "PAGE_NUMBER",
        "PAGE_TYPE",
        "FLUSH_TYPE",
        "FIX_COUNT",
        "IS_HASHED",
        "NEWEST_MODIFICATION",
        "OLDEST_MODIFICATION",
        "ACCESS_TIME",
        "TABLE_NAME",
        "INDEX_NAME",
        "NUMBER_RECORDS",
        "DATA_SIZE",
        "COMPRESSED_SIZE",
        "COMPRESSED",
        "IO_FIX",
        "IS_OLD",
        "FREE_PAGE_CLOCK",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const block_column[] = {"BLOCK_ID"};
    static const char *const lru_position_column[] = {"LRU_POSITION"};
    static const char *const system_table_columns[] = {
        "TABLE_SCHEMA",
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
        "information_schema",
        "INNODB_BUFFER_PAGE",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        NULL,
        "information_schema",
        "INNODB_BUFFER_PAGE_LRU",
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open buffer page tables db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE",
            .column_names = buffer_page_column_names,
            .column_count = ARRAY_COUNT(buffer_page_column_names),
            .values = NULL,
            .row_count = 0U,
            .context = "buffer page wildcard",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU",
            .column_names = buffer_page_lru_column_names,
            .column_count = ARRAY_COUNT(buffer_page_lru_column_names),
            .values = NULL,
            .row_count = 0U,
            .context = "buffer page lru wildcard",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "buffer page count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_buffer_page_lru",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "case-insensitive buffer page lru table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT b.BLOCK_ID FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE AS b "
                   "WHERE b.PAGE_TYPE = 'INDEX' AND b.TABLE_NAME LIKE '%probe%' "
                   "ORDER BY b.BLOCK_ID LIMIT 1",
            .column_names = block_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "buffer page alias predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT l.LRU_POSITION FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU AS l "
                   "WHERE l.COMPRESSED = 'NO' AND l.IO_FIX IS NULL "
                   "ORDER BY l.LRU_POSITION LIMIT 1",
            .column_names = lru_position_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "buffer page lru alias predicate",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE information_schema",
            .context = "use information_schema for buffer page tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INNODB_BUFFER_PAGE",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "unqualified buffer page count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INNODB_BUFFER_PAGE_LRU",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "unqualified buffer page lru count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME IN ('INNODB_BUFFER_PAGE','INNODB_BUFFER_PAGE_LRU') "
                   "ORDER BY TABLE_NAME",
            .column_names = system_table_columns,
            .column_count = ARRAY_COUNT(system_table_columns),
            .values = system_table_values,
            .row_count = ARRAY_COUNT(system_table_values) / ARRAY_COUNT(system_table_columns),
            .context = "buffer page system table rows",
        }
    );
    failures += expect_columns_metadata(
        database,
        "INNODB_BUFFER_PAGE",
        buffer_page_columns,
        ARRAY_COUNT(buffer_page_columns)
    );
    failures += expect_columns_metadata(
        database,
        "INNODB_BUFFER_PAGE_LRU",
        buffer_page_lru_columns,
        ARRAY_COUNT(buffer_page_lru_columns)
    );
    failures += expect_row_count_status(database, "buffer page row count status");
    failures += setup_indexed_table(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "buffer page remains empty after table activity",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "buffer page lru remains empty after table activity",
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
            .context = "create buffer page schema",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE app",
            .context = "use buffer page schema",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE TABLE page_probe (id INT PRIMARY KEY, v INT, KEY v_key (v))",
            .context = "create indexed table for buffer page views",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "INSERT INTO page_probe VALUES (1, 10), (2, 20)",
            .context = "insert indexed table rows for buffer page views",
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

static int expect_columns_metadata(
    mylite_db *database,
    const char *table_name,
    const struct expected_information_schema_column *expected_columns,
    size_t expected_column_count
) {
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
        "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = '%s' "
        "ORDER BY ORDINAL_POSITION",
        table_name
    );
    int rc = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "%s columns metadata: SQL buffer too small\n", table_name);
        return 1;
    }

    rc = mylite_execute(database, sql, strlen(sql), &result);
    failures += expect_int(rc, MYLITE_OK, "buffer page columns metadata");
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s columns metadata: %s\n", table_name, mylite_errmsg(database));
        mylite_result_free(result);
        return failures + 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s columns metadata: expected result object\n", table_name);
        return failures + 1;
    }

    failures += expect_size(
        mylite_result_column_count(result),
        ARRAY_COUNT(metadata_column_names),
        "buffer page columns metadata"
    );
    failures += expect_size(
        mylite_result_row_count(result),
        expected_column_count,
        "buffer page columns metadata"
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 0, "buffer page columns metadata");
    failures +=
        expect_size(mylite_result_warning_count(result), 0U, "buffer page columns metadata");

    for (size_t column = 0U; column < ARRAY_COUNT(metadata_column_names); ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            metadata_column_names[column],
            "buffer page columns metadata"
        );
    }
    for (size_t row = 0U; row < expected_column_count; ++row) {
        const struct expected_information_schema_column *expected = &expected_columns[row];
        char ordinal_text[metadata_ordinal_text_capacity];

        written = snprintf(ordinal_text, sizeof(ordinal_text), "%zu", row + 1U);
        if (written < 0 || (size_t)written >= sizeof(ordinal_text)) {
            fprintf(stderr, "%s columns metadata: ordinal overflow\n", table_name);
            failures += 1;
            continue;
        }

        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 0U),
            table_name,
            "buffer page columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 1U),
            expected->name,
            "buffer page columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 2U),
            ordinal_text,
            "buffer page columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 3U),
            "",
            "buffer page columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 4U),
            expected->is_nullable,
            "buffer page columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_data_type_column),
            expected->data_type,
            "buffer page columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_character_maximum_length_column),
            expected->character_maximum_length,
            "buffer page columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_character_octet_length_column),
            expected->character_octet_length,
            "buffer page columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_numeric_precision_column),
            NULL,
            "buffer page columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_numeric_scale_column),
            NULL,
            "buffer page columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_datetime_precision_column),
            NULL,
            "buffer page columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_character_set_name_column),
            expected->character_set_name,
            "buffer page columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_collation_name_column),
            expected->collation_name,
            "buffer page columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_column_type_column),
            expected->column_type,
            "buffer page columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_privileges_column),
            "select",
            "buffer page columns metadata"
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
        "%s/mylite_information_schema_innodb_buffer_page_tables_%d_%s.mylite",
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
