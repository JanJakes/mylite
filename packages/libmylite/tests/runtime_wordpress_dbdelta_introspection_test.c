#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

#include <stdbool.h>
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
    show_columns_column_count = 6,
    show_full_columns_column_count = 9,
    show_index_column_count = 15,
    show_create_column_count = 2,
    information_schema_columns_column_count = 8,
    statistics_column_count = 8,
    default_prefix_length = 191,
    alternate_prefix_length = 20,
    create_postmeta_sql_capacity = 768,
    suffix_extra_capacity = 16,
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct catalog_metadata_trace {
    size_t schema_scan_count;
    size_t table_scan_count;
    size_t column_query_count;
    size_t index_query_count;
    size_t index_column_query_count;
};

static int test_dbdelta_introspection_persistence_and_preamble(void);
static int test_dbdelta_introspection_independent_handles(void);
static int test_dbdelta_introspection_reuses_cached_metadata(void);
static int create_fixture_schema(mylite_db *database);
static int create_wordpress_dbdelta_fixture_tables(mylite_db *database);
static int create_wordpress_postmeta_fixture_table(mylite_db *database, int prefix_length);
static int verify_dbdelta_introspection_metadata(mylite_db *database, const char *context);
static int verify_postmeta_prefix_metadata(
    mylite_db *database,
    const char *const *values,
    const char *context
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
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
static int count_catalog_metadata_query(
    unsigned int trace_kind,
    void *context,
    void *statement_handle,
    void *expanded_sql
);

int main(void) {
    int failures = 0;

    failures += test_dbdelta_introspection_persistence_and_preamble();
    failures += test_dbdelta_introspection_independent_handles();
    failures += test_dbdelta_introspection_reuses_cached_metadata();

    return failures == 0 ? 0 : 1;
}

static int test_dbdelta_introspection_persistence_and_preamble(void) {
    static const char *const row_count_rows[] = {"-1"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "metadata") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open metadata fixture");
    failures += create_fixture_schema(database);
    failures += create_wordpress_dbdelta_fixture_tables(database);
    failures += verify_dbdelta_introspection_metadata(database, "initial metadata");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = row_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row count after metadata introspection",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "metadata fixture preamble"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen metadata fixture");
    failures += expect_statement_ok(database, "USE wp");
    failures += verify_dbdelta_introspection_metadata(database, "reopened metadata");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_dbdelta_introspection_independent_handles(void) {
    static const char *const prefix_191_values[] = {
        "wp_postmeta",
        "1",
        "meta_key",
        "1",
        "meta_key",
        "A",
        "0",
        "191",
        NULL,
        "YES",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const prefix_20_values[] = {
        "wp_postmeta",
        "1",
        "meta_key",
        "1",
        "meta_key",
        "A",
        "0",
        "20",
        NULL,
        "YES",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first fixture");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second fixture");

    failures += create_fixture_schema(first);
    failures += create_wordpress_postmeta_fixture_table(first, default_prefix_length);
    failures += create_fixture_schema(second);
    failures += create_wordpress_postmeta_fixture_table(second, alternate_prefix_length);

    failures += verify_postmeta_prefix_metadata(first, prefix_191_values, "first prefix metadata");
    failures += verify_postmeta_prefix_metadata(second, prefix_20_values, "second prefix metadata");

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int test_dbdelta_introspection_reuses_cached_metadata(void) {
    static const char *const zero_count_values[] = {"0"};
    char path[test_path_capacity];
    struct catalog_metadata_trace trace = {0};
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "cached_metadata") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open cached metadata fixture");
    failures += create_fixture_schema(database);
    failures += create_wordpress_dbdelta_fixture_tables(database);
    failures += verify_dbdelta_introspection_metadata(database, "warm cached metadata");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA='WP' AND TABLE_NAME='WP_OPTIONS'",
            .values = zero_count_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "case-sensitive exact metadata lookup",
        }
    );

    sqlite = mylite_connection_sqlite_for_test(database);
    failures += expect_int(
        sqlite3_trace_v2(sqlite, SQLITE_TRACE_STMT, count_catalog_metadata_query, &trace),
        SQLITE_OK,
        "install cached metadata trace"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA='wp' AND TABLE_NAME='missing_table'",
            .values = zero_count_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "missing exact metadata lookup",
        }
    );
    failures += verify_dbdelta_introspection_metadata(database, "reuse cached metadata");
    failures += expect_size(trace.schema_scan_count, 0U, "exact metadata schema scans");
    failures += expect_size(trace.table_scan_count, 0U, "exact metadata table scans");
    failures += expect_size(trace.column_query_count, 0U, "cached metadata column queries");
    failures += expect_size(trace.index_query_count, 0U, "cached metadata index queries");
    failures +=
        expect_size(trace.index_column_query_count, 0U, "cached metadata index column queries");
    failures += expect_int(
        sqlite3_trace_v2(sqlite, 0U, NULL, NULL),
        SQLITE_OK,
        "remove cached metadata trace"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int count_catalog_metadata_query(
    unsigned int trace_kind,
    void *context, // NOLINT(bugprone-easily-swappable-parameters): SQLite trace callback ABI.
    void *statement_handle,
    void *expanded_sql
) {
    struct catalog_metadata_trace *trace = context;
    sqlite3_stmt *statement = statement_handle;
    const char *sql = statement == NULL ? NULL : sqlite3_sql(statement);

    (void)expanded_sql;
    if (trace_kind != SQLITE_TRACE_STMT || trace == NULL || sql == NULL) {
        return 0;
    }
    if (strstr(sql, "FROM _mylite_catalog_schemas ORDER BY name") != NULL) {
        ++trace->schema_scan_count;
    }
    if (strstr(sql, "FROM _mylite_catalog_tables WHERE schema_id = ?1 ORDER BY name") != NULL) {
        ++trace->table_scan_count;
    }
    if (strstr(sql, "FROM _mylite_catalog_columns") != NULL) {
        ++trace->column_query_count;
    }
    if (strstr(sql, "FROM _mylite_catalog_indexes") != NULL) {
        ++trace->index_query_count;
    }
    if (strstr(sql, "FROM _mylite_catalog_index_columns") != NULL) {
        ++trace->index_column_query_count;
    }
    return 0;
}

static int create_fixture_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(
        database,
        "CREATE DATABASE wp DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci"
    );
    failures += expect_statement_ok(database, "USE wp");
    failures += expect_statement_ok(database, "SET sql_mode = ''");
    return failures;
}

static int create_wordpress_dbdelta_fixture_tables(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(
        database,
        "CREATE TABLE wp_options ("
        "option_id bigint(20) unsigned NOT NULL auto_increment, "
        "option_name varchar(191) NOT NULL default '', "
        "option_value longtext NOT NULL, "
        "autoload varchar(20) NOT NULL default 'yes', "
        "PRIMARY KEY (option_id), "
        "UNIQUE KEY option_name (option_name), "
        "KEY autoload (autoload)"
        ") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci"
    );
    failures += create_wordpress_postmeta_fixture_table(database, default_prefix_length);
    return failures;
}

static int create_wordpress_postmeta_fixture_table(mylite_db *database, int prefix_length) {
    char sql[create_postmeta_sql_capacity];
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE wp_postmeta ("
        "meta_id bigint(20) unsigned NOT NULL auto_increment, "
        "post_id bigint(20) unsigned NOT NULL default '0', "
        "meta_key varchar(255) default NULL, "
        "meta_value longtext, "
        "PRIMARY KEY (meta_id), "
        "KEY post_id (post_id), "
        "KEY meta_key (meta_key(%d))"
        ") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci",
        prefix_length
    );

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    return expect_statement_ok(database, sql);
}

static int verify_dbdelta_introspection_metadata(mylite_db *database, const char *context) {
    static const char *const describe_values[] = {
        "option_id",    "bigint unsigned", "NO", "PRI", NULL,  "auto_increment",
        "option_name",  "varchar(191)",    "NO", "UNI", "",    "",
        "option_value", "longtext",        "NO", "",    NULL,  "",
        "autoload",     "varchar(20)",     "NO", "MUL", "yes", "",
    };
    static const char *const full_columns_values[] = {
        "option_id",
        "bigint unsigned",
        NULL,
        "NO",
        "PRI",
        NULL,
        "auto_increment",
        "select,insert,update,references",
        "",
        "option_name",
        "varchar(191)",
        "utf8mb4_unicode_520_ci",
        "NO",
        "UNI",
        "",
        "",
        "select,insert,update,references",
        "",
        "option_value",
        "longtext",
        "utf8mb4_unicode_520_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "autoload",
        "varchar(20)",
        "utf8mb4_unicode_520_ci",
        "NO",
        "MUL",
        "yes",
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const full_columns_where_values[] = {
        "option_id",
        "bigint unsigned",
        NULL,
        "NO",
        "PRI",
        NULL,
        "auto_increment",
        "select,insert,update,references",
        "",
        "option_name",
        "varchar(191)",
        "utf8mb4_unicode_520_ci",
        "NO",
        "UNI",
        "",
        "",
        "select,insert,update,references",
        "",
        "autoload",
        "varchar(20)",
        "utf8mb4_unicode_520_ci",
        "NO",
        "MUL",
        "yes",
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const postmeta_index_values[] = {
        "wp_postmeta", "1",        "post_id", "1",        "post_id", "A",   "0",   NULL,
        NULL,          "",         "BTREE",   "",         "",        "YES", NULL,  "wp_postmeta",
        "1",           "meta_key", "1",       "meta_key", "A",       "0",   "191", NULL,
        "YES",         "BTREE",    "",        "",         "YES",     NULL,
    };
    static const char *const unique_index_values[] = {
        "wp_options",
        "0",
        "option_name",
        "1",
        "option_name",
        "A",
        "0",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const information_schema_columns_values[] = {
        "wp_options",
        "option_id",
        "bigint unsigned",
        NULL,
        "NO",
        "PRI",
        NULL,
        "auto_increment",
        "wp_options",
        "option_name",
        "varchar(191)",
        "",
        "NO",
        "UNI",
        "utf8mb4_unicode_520_ci",
        "",
        "wp_options",
        "option_value",
        "longtext",
        NULL,
        "NO",
        "",
        "utf8mb4_unicode_520_ci",
        "",
        "wp_options",
        "autoload",
        "varchar(20)",
        "yes",
        "NO",
        "MUL",
        "utf8mb4_unicode_520_ci",
        "",
    };
    static const char *const statistics_values[] = {
        "wp_postmeta",
        "meta_key",
        "1",
        "meta_key",
        "1",
        "191",
        "BTREE",
        "YES",
    };
    static const char *const show_create_values[] = {
        "wp_options",
        "CREATE TABLE `wp_options` (\n"
        "  `option_id` bigint unsigned NOT NULL AUTO_INCREMENT,\n"
        "  `option_name` varchar(191) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  `option_value` longtext COLLATE utf8mb4_unicode_520_ci NOT NULL,\n"
        "  `autoload` varchar(20) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT 'yes',\n"
        "  PRIMARY KEY (`option_id`),\n"
        "  UNIQUE KEY `option_name` (`option_name`),\n"
        "  KEY `autoload` (`autoload`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci",
    };
    int failures = 0;

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESCRIBE wp_options",
            .values = describe_values,
            .column_count = show_columns_column_count,
            .row_count = 4U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESC wp_options",
            .values = describe_values,
            .column_count = show_columns_column_count,
            .row_count = 4U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "EXPLAIN wp_options",
            .values = describe_values,
            .column_count = show_columns_column_count,
            .row_count = 4U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM wp_options",
            .values = full_columns_values,
            .column_count = show_full_columns_column_count,
            .row_count = 4U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM wp_options "
                   "WHERE Field IN ('option_id','option_name','autoload')",
            .values = full_columns_where_values,
            .column_count = show_full_columns_column_count,
            .row_count = 3U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM wp_postmeta WHERE Key_name IN ('post_id','meta_key')",
            .values = postmeta_index_values,
            .column_count = show_index_column_count,
            .row_count = 2U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM wp_options "
                   "WHERE Non_unique = '0' AND Column_name = 'option_name'",
            .values = unique_index_values,
            .column_count = show_index_column_count,
            .row_count = 1U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME,COLUMN_NAME,COLUMN_TYPE,COLUMN_DEFAULT,IS_NULLABLE,"
                   "COLUMN_KEY,COLLATION_NAME,EXTRA FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA='wp' AND TABLE_NAME='wp_options' "
                   "ORDER BY ORDINAL_POSITION",
            .values = information_schema_columns_values,
            .column_count = information_schema_columns_column_count,
            .row_count = 4U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME,INDEX_NAME,SEQ_IN_INDEX,COLUMN_NAME,NON_UNIQUE,SUB_PART,"
                   "INDEX_TYPE,IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA='wp' AND TABLE_NAME='wp_postmeta' "
                   "AND INDEX_NAME='meta_key'",
            .values = statistics_values,
            .column_count = statistics_column_count,
            .row_count = 1U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE wp_options",
            .values = show_create_values,
            .column_count = show_create_column_count,
            .row_count = 1U,
            .context = context,
        }
    );

    return failures;
}

static int verify_postmeta_prefix_metadata(
    mylite_db *database,
    const char *const *values,
    const char *context
) {
    return expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM wp_postmeta WHERE Key_name = 'meta_key'",
            .values = values,
            .column_count = show_index_column_count,
            .row_count = 1U,
            .context = context,
        }
    );
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d (%s %d: %s)\n",
            sql,
            rc,
            mylite_sqlstate(database),
            mylite_errcode(database),
            mylite_errmsg(database)
        );
        if (result != NULL) {
            mylite_result_free(result);
        }
        return 1;
    }
    if (out_result == NULL) {
        mylite_result_free(result);
    } else {
        *out_result = result;
    }
    return 0;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        mylite_result_free(result);
    }
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    size_t expected_value_count = query.column_count * query.row_count;
    int failures = execute_ok(database, query.sql, &result);

    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);

    for (size_t index = 0U; index < expected_value_count; ++index) {
        failures += expect_result_value(
            result,
            index / query.column_count,
            index % query.column_count,
            query.values[index],
            query.context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    return expect_text_or_null(mylite_result_value_text(result, row, column), expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_wordpress_dbdelta_introspection_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path\n");
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
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + suffix_extra_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek\n", path);
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read bytes\n", path);
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
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "NULL" : expected,
                actual == NULL ? "NULL" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected [%s], got [%s]\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }
    return 0;
}
