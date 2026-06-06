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

#ifndef P_tmpdir
#  define P_tmpdir "."
#endif

#define ARRAY_COUNT(items) (sizeof(items) / sizeof((items)[0]))

enum {
    test_path_capacity = 1024,
    test_path_suffix_capacity = 16,
    metadata_ordinal_text_capacity = 16,
    metadata_data_type_column = 5,
    metadata_first_nullable_column = 6,
    metadata_last_nullable_column = 12,
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
    const char *data_type;
    const char *column_type;
};

static const struct expected_information_schema_column buffer_pool_stats_columns[] = {
    {"POOL_ID", "bigint", "bigint unsigned"},
    {"POOL_SIZE", "bigint", "bigint unsigned"},
    {"FREE_BUFFERS", "bigint", "bigint unsigned"},
    {"DATABASE_PAGES", "bigint", "bigint unsigned"},
    {"OLD_DATABASE_PAGES", "bigint", "bigint unsigned"},
    {"MODIFIED_DATABASE_PAGES", "bigint", "bigint unsigned"},
    {"PENDING_DECOMPRESS", "bigint", "bigint unsigned"},
    {"PENDING_READS", "bigint", "bigint unsigned"},
    {"PENDING_FLUSH_LRU", "bigint", "bigint unsigned"},
    {"PENDING_FLUSH_LIST", "bigint", "bigint unsigned"},
    {"PAGES_MADE_YOUNG", "bigint", "bigint unsigned"},
    {"PAGES_NOT_MADE_YOUNG", "bigint", "bigint unsigned"},
    {"PAGES_MADE_YOUNG_RATE", "float", "float(12,0)"},
    {"PAGES_MADE_NOT_YOUNG_RATE", "float", "float(12,0)"},
    {"NUMBER_PAGES_READ", "bigint", "bigint unsigned"},
    {"NUMBER_PAGES_CREATED", "bigint", "bigint unsigned"},
    {"NUMBER_PAGES_WRITTEN", "bigint", "bigint unsigned"},
    {"PAGES_READ_RATE", "float", "float(12,0)"},
    {"PAGES_CREATE_RATE", "float", "float(12,0)"},
    {"PAGES_WRITTEN_RATE", "float", "float(12,0)"},
    {"NUMBER_PAGES_GET", "bigint", "bigint unsigned"},
    {"HIT_RATE", "bigint", "bigint unsigned"},
    {"YOUNG_MAKE_PER_THOUSAND_GETS", "bigint", "bigint unsigned"},
    {"NOT_YOUNG_MAKE_PER_THOUSAND_GETS", "bigint", "bigint unsigned"},
    {"NUMBER_PAGES_READ_AHEAD", "bigint", "bigint unsigned"},
    {"NUMBER_READ_AHEAD_EVICTED", "bigint", "bigint unsigned"},
    {"READ_AHEAD_RATE", "float", "float(12,0)"},
    {"READ_AHEAD_EVICTED_RATE", "float", "float(12,0)"},
    {"LRU_IO_TOTAL", "bigint", "bigint unsigned"},
    {"LRU_IO_CURRENT", "bigint", "bigint unsigned"},
    {"UNCOMPRESS_TOTAL", "bigint", "bigint unsigned"},
    {"UNCOMPRESS_CURRENT", "bigint", "bigint unsigned"},
};

static int test_information_schema_innodb_buffer_pool_stats_queries(void);
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

    failures += test_information_schema_innodb_buffer_pool_stats_queries();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_innodb_buffer_pool_stats_queries(void) {
    static const char *const stats_column_names[] = {
        "POOL_ID",
        "POOL_SIZE",
        "FREE_BUFFERS",
        "DATABASE_PAGES",
        "OLD_DATABASE_PAGES",
        "MODIFIED_DATABASE_PAGES",
        "PENDING_DECOMPRESS",
        "PENDING_READS",
        "PENDING_FLUSH_LRU",
        "PENDING_FLUSH_LIST",
        "PAGES_MADE_YOUNG",
        "PAGES_NOT_MADE_YOUNG",
        "PAGES_MADE_YOUNG_RATE",
        "PAGES_MADE_NOT_YOUNG_RATE",
        "NUMBER_PAGES_READ",
        "NUMBER_PAGES_CREATED",
        "NUMBER_PAGES_WRITTEN",
        "PAGES_READ_RATE",
        "PAGES_CREATE_RATE",
        "PAGES_WRITTEN_RATE",
        "NUMBER_PAGES_GET",
        "HIT_RATE",
        "YOUNG_MAKE_PER_THOUSAND_GETS",
        "NOT_YOUNG_MAKE_PER_THOUSAND_GETS",
        "NUMBER_PAGES_READ_AHEAD",
        "NUMBER_READ_AHEAD_EVICTED",
        "READ_AHEAD_RATE",
        "READ_AHEAD_EVICTED_RATE",
        "LRU_IO_TOTAL",
        "LRU_IO_CURRENT",
        "UNCOMPRESS_TOTAL",
        "UNCOMPRESS_CURRENT",
    };
    static const char *const zero_values[] = {
        "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
        "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_one[] = {"1"};
    static const char *const alias_columns[] = {
        "POOL_ID",
        "POOL_SIZE",
        "PAGES_MADE_YOUNG_RATE",
        "HIT_RATE",
    };
    static const char *const alias_values[] = {"0", "0", "0", "0"};
    static const char *const post_table_columns[] = {
        "POOL_ID",
        "DATABASE_PAGES",
        "NUMBER_PAGES_GET",
        "READ_AHEAD_RATE",
        "UNCOMPRESS_CURRENT",
    };
    static const char *const post_table_values[] = {"0", "0", "0", "0", "0"};
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
        "INNODB_BUFFER_POOL_STATS",
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open buffer pool stats db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS",
            .column_names = stats_column_names,
            .column_count = ARRAY_COUNT(stats_column_names),
            .values = zero_values,
            .row_count = 1U,
            .context = "buffer pool stats wildcard row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_one,
            .row_count = 1U,
            .context = "buffer pool stats count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_buffer_pool_stats",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_one,
            .row_count = 1U,
            .context = "case-insensitive buffer pool stats table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT s.POOL_ID, s.POOL_SIZE, s.PAGES_MADE_YOUNG_RATE, s.HIT_RATE "
                   "FROM INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS AS s "
                   "WHERE s.POOL_ID = 0 AND s.PAGES_READ_RATE = 0 ORDER BY s.POOL_ID LIMIT 1",
            .column_names = alias_columns,
            .column_count = ARRAY_COUNT(alias_columns),
            .values = alias_values,
            .row_count = 1U,
            .context = "buffer pool stats alias predicate",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE information_schema",
            .context = "use information_schema for buffer pool stats",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INNODB_BUFFER_POOL_STATS",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_one,
            .row_count = 1U,
            .context = "unqualified buffer pool stats count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'INNODB_BUFFER_POOL_STATS'",
            .column_names = system_table_columns,
            .column_count = ARRAY_COUNT(system_table_columns),
            .values = system_table_values,
            .row_count = 1U,
            .context = "buffer pool stats system table row",
        }
    );
    failures += expect_columns_metadata(database);
    failures += expect_row_count_status(database, "buffer pool stats row count status");
    failures += setup_indexed_table(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT POOL_ID, DATABASE_PAGES, NUMBER_PAGES_GET, READ_AHEAD_RATE, "
                   "UNCOMPRESS_CURRENT FROM INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS",
            .column_names = post_table_columns,
            .column_count = ARRAY_COUNT(post_table_columns),
            .values = post_table_values,
            .row_count = 1U,
            .context = "buffer pool stats remains zero after table activity",
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
            .context = "create buffer pool stats schema",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE app",
            .context = "use buffer pool stats schema",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "CREATE TABLE buffer_probe (id INT PRIMARY KEY, v INT, KEY v_key (v))",
            .context = "create indexed table for buffer pool stats",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "INSERT INTO buffer_probe VALUES (1, 10), (2, 20)",
            .context = "insert indexed table rows for buffer pool stats",
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
    int failures = 0;
    int rc = mylite_execute(
        database,
        "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "
        "DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, "
        "NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "
        "COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = 'information_schema' "
        "AND TABLE_NAME = 'INNODB_BUFFER_POOL_STATS' ORDER BY ORDINAL_POSITION",
        strlen("SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "
               "DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, "
               "NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "
               "COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
               "WHERE TABLE_SCHEMA = 'information_schema' "
               "AND TABLE_NAME = 'INNODB_BUFFER_POOL_STATS' ORDER BY ORDINAL_POSITION"),
        &result
    );

    failures += expect_int(rc, MYLITE_OK, "buffer pool stats columns metadata");
    if (rc != MYLITE_OK) {
        fprintf(stderr, "buffer pool stats columns metadata: %s\n", mylite_errmsg(database));
        mylite_result_free(result);
        return failures + 1;
    }
    if (result == NULL) {
        fprintf(stderr, "buffer pool stats columns metadata: expected result object\n");
        return failures + 1;
    }

    failures += expect_size(
        mylite_result_column_count(result),
        ARRAY_COUNT(metadata_column_names),
        "buffer pool stats columns metadata"
    );
    failures += expect_size(
        mylite_result_row_count(result),
        ARRAY_COUNT(buffer_pool_stats_columns),
        "buffer pool stats columns metadata"
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 0, "buffer pool stats columns metadata");
    failures +=
        expect_size(mylite_result_warning_count(result), 0U, "buffer pool stats columns metadata");

    for (size_t column = 0U; column < ARRAY_COUNT(metadata_column_names); ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            metadata_column_names[column],
            "buffer pool stats columns metadata"
        );
    }
    for (size_t row = 0U; row < ARRAY_COUNT(buffer_pool_stats_columns); ++row) {
        const struct expected_information_schema_column *expected = &buffer_pool_stats_columns[row];
        char ordinal_text[metadata_ordinal_text_capacity];
        int written = snprintf(ordinal_text, sizeof(ordinal_text), "%zu", row + 1U);

        if (written < 0 || (size_t)written >= sizeof(ordinal_text)) {
            fprintf(stderr, "buffer pool stats columns metadata: ordinal overflow\n");
            failures += 1;
            continue;
        }

        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 0U),
            "INNODB_BUFFER_POOL_STATS",
            "buffer pool stats columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 1U),
            expected->name,
            "buffer pool stats columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 2U),
            ordinal_text,
            "buffer pool stats columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 3U),
            "",
            "buffer pool stats columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 4U),
            "NO",
            "buffer pool stats columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_data_type_column),
            expected->data_type,
            "buffer pool stats columns metadata"
        );
        for (size_t nullable_column = metadata_first_nullable_column;
             nullable_column <= metadata_last_nullable_column;
             ++nullable_column) {
            failures += expect_text_or_null(
                mylite_result_value_text(result, row, nullable_column),
                NULL,
                "buffer pool stats columns metadata"
            );
        }
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_column_type_column),
            expected->column_type,
            "buffer pool stats columns metadata"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, metadata_privileges_column),
            "select",
            "buffer pool stats columns metadata"
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
        "%s/mylite_information_schema_innodb_buffer_pool_stats_%d_%s.mylite",
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
