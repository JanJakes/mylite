#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

#include <inttypes.h>
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
    show_create_sql_capacity = 128,
    show_table_status_row_format_column = 3,
    show_table_status_create_options_column = 16,
    mysql_error_parse = 1064,
    mysql_error_table_storage_engine_option = 1031,
    catalog_schema_version_after_storage_statistics_options = MYLITE_CATALOG_SCHEMA_VERSION,
    catalog_minimum_reader_version_after_storage_statistics_options =
        MYLITE_CATALOG_MINIMUM_READER_SCHEMA_VERSION,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_persistent_metadata_like_reopen_and_preamble(void);
static int test_storage_statistics_diagnostics(void);
static int test_catalog_v30_migration_defaults(void);
static int test_independent_file_option_state(void);
static int make_catalog_look_like_v30(sqlite3 *sqlite);
static int expect_catalog_state_versions(
    sqlite3 *sqlite,
    int64_t expected_schema_version,
    int64_t expected_minimum_reader_schema_version,
    const char *context
);
static int create_app_schema(mylite_db *database);
static int expect_show_create_contains(
    mylite_db *database,
    const char *table_name, // NOLINT(bugprone-easily-swappable-parameters)
    const char *needle,
    const char *context
);
static int expect_show_create_not_contains(
    mylite_db *database,
    const char *table_name, // NOLINT(bugprone-easily-swappable-parameters)
    const char *needle,
    const char *context
);
static int expect_single_cell(
    mylite_db *database,
    const char *sql,
    size_t column_index,
    const char *expected,
    const char *context
);
static int expect_single_cell_contains(
    mylite_db *database,
    const char *sql,
    size_t column_index,
    const char *needle,
    const char *context
);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int execute_sql(sqlite3 *connection, const char *sql);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int expect_text_not_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_persistent_metadata_like_reopen_and_preamble();
    failures += test_storage_statistics_diagnostics();
    failures += test_catalog_v30_migration_defaults();
    failures += test_independent_file_option_state();

    return failures == 0 ? 0 : 1;
}

static int test_persistent_metadata_like_reopen_and_preamble(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "persistent") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open persistent file");
    failures += create_app_schema(database);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE all_options (id INT) ENGINE=InnoDB, ROW_FORMAT=COMPACT "
        "PACK_KEYS=1 STATS_PERSISTENT=1 STATS_AUTO_RECALC=0 STATS_SAMPLE_PAGES=7 "
        "CHECKSUM=2"
    );
    failures += execute_dml_ok(database, "INSERT INTO all_options VALUES (1)", 1);
    failures +=
        expect_single_cell(database, "SELECT id FROM all_options", 0U, "1", "stored row value");
    failures += expect_show_create_contains(
        database,
        "all_options",
        "PACK_KEYS=1 STATS_PERSISTENT=1 STATS_AUTO_RECALC=0 STATS_SAMPLE_PAGES=7 CHECKSUM=1 "
        "ROW_FORMAT=COMPACT",
        "all options SHOW CREATE suffix"
    );
    failures += expect_show_create_not_contains(
        database,
        "all_options",
        "KEY_BLOCK_SIZE=",
        "all options omit default key block size"
    );
    failures += expect_single_cell(
        database,
        "SELECT ROW_FORMAT FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'all_options'",
        0U,
        "Compact",
        "information schema row format"
    );
    failures += expect_single_cell(
        database,
        "SELECT CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'all_options'",
        0U,
        "row_format=COMPACT stats_sample_pages=7 stats_auto_recalc=0 stats_persistent=1 "
        "pack_keys=1 checksum=1",
        "information schema create options"
    );
    failures += expect_single_cell(
        database,
        "SHOW TABLE STATUS WHERE Name = 'all_options'",
        show_table_status_row_format_column,
        "Compact",
        "SHOW TABLE STATUS row format"
    );
    failures += expect_single_cell(
        database,
        "SHOW TABLE STATUS WHERE Name = 'all_options'",
        show_table_status_create_options_column,
        "row_format=COMPACT stats_sample_pages=7 stats_auto_recalc=0 stats_persistent=1 "
        "pack_keys=1 checksum=1",
        "SHOW TABLE STATUS create options"
    );

    failures += execute_statement_ok(
        database,
        "CREATE TABLE defaults (id INT) ROW_FORMAT=DEFAULT KEY_BLOCK_SIZE=0 PACK_KEYS=DEFAULT "
        "CHECKSUM=0 STATS_PERSISTENT=DEFAULT STATS_AUTO_RECALC=DEFAULT "
        "STATS_SAMPLE_PAGES=DEFAULT"
    );
    failures += expect_show_create_not_contains(
        database,
        "defaults",
        "ROW_FORMAT=",
        "default row format omitted"
    );
    failures += expect_show_create_not_contains(
        database,
        "defaults",
        "PACK_KEYS=",
        "default pack keys omitted"
    );
    failures += expect_single_cell(
        database,
        "SELECT CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'defaults'",
        0U,
        "",
        "default create options omitted"
    );

    failures += execute_statement_ok(
        database,
        "CREATE TABLE duplicates (id INT) ROW_FORMAT=COMPACT ROW_FORMAT=DYNAMIC "
        "PACK_KEYS=1 PACK_KEYS=DEFAULT STATS_PERSISTENT=0 STATS_PERSISTENT=1"
    );
    failures += expect_show_create_contains(
        database,
        "duplicates",
        "ROW_FORMAT=DYNAMIC",
        "duplicate row format last wins"
    );
    failures += expect_show_create_contains(
        database,
        "duplicates",
        "STATS_PERSISTENT=1",
        "duplicate stats persistent last wins"
    );
    failures += expect_show_create_not_contains(
        database,
        "duplicates",
        "PACK_KEYS=",
        "duplicate pack keys default clears"
    );

    failures += execute_statement_ok(database, "CREATE TABLE kbs (id INT) KEY_BLOCK_SIZE=8");
    failures += expect_show_create_contains(
        database,
        "kbs",
        "KEY_BLOCK_SIZE=8",
        "key block size SHOW CREATE"
    );
    failures += expect_single_cell(
        database,
        "SHOW TABLE STATUS WHERE Name = 'kbs'",
        show_table_status_row_format_column,
        "Compressed",
        "key block size status row format"
    );
    failures += expect_single_cell(
        database,
        "SELECT CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'kbs'",
        0U,
        "KEY_BLOCK_SIZE=8",
        "key block size create options"
    );

    failures += execute_statement_ok(
        database,
        "CREATE TABLE comma_options (id INT) ENGINE=InnoDB, ROW_FORMAT=REDUNDANT, PACK_KEYS=1"
    );
    failures += expect_show_create_contains(
        database,
        "comma_options",
        "ROW_FORMAT=REDUNDANT",
        "comma-separated row format"
    );
    failures += execute_statement_ok(database, "CREATE TABLE clone_like LIKE all_options");
    failures += expect_show_create_contains(
        database,
        "clone_like",
        "PACK_KEYS=1 STATS_PERSISTENT=1 STATS_AUTO_RECALC=0 STATS_SAMPLE_PAGES=7 CHECKSUM=1 "
        "ROW_FORMAT=COMPACT",
        "CREATE TABLE LIKE clones table options"
    );

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen persistent file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_show_create_contains(
        database,
        "clone_like",
        "ROW_FORMAT=COMPACT",
        "reopen preserves cloned row format"
    );
    failures += expect_single_cell(
        database,
        "SELECT CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'all_options'",
        0U,
        "row_format=COMPACT stats_sample_pages=7 stats_auto_recalc=0 stats_persistent=1 "
        "pack_keys=1 checksum=1",
        "reopen preserves create options"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_storage_statistics_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open diagnostics");

    failures += create_app_schema(database);
    failures += execute_error(
        database,
        "CREATE TABLE bad_fixed (id INT) ROW_FORMAT=FIXED",
        (struct expected_sql_error){
            .code = mysql_error_table_storage_engine_option,
            .sqlstate = "HY000",
            .message_part = "doesn't have this option",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_key_block (id INT) KEY_BLOCK_SIZE=7",
        (struct expected_sql_error){
            .code = mysql_error_table_storage_engine_option,
            .sqlstate = "HY000",
            .message_part = "doesn't have this option",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_conflict (id INT) KEY_BLOCK_SIZE=8 ROW_FORMAT=DYNAMIC",
        (struct expected_sql_error){
            .code = mysql_error_table_storage_engine_option,
            .sqlstate = "HY000",
            .message_part = "doesn't have this option",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_pack (id INT) PACK_KEYS=2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "PACK_KEYS supports only DEFAULT, 0, or 1",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_stats_sample_zero (id INT) STATS_SAMPLE_PAGES=0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "valid range for stats_sample_pages",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_stats_sample_high (id INT) STATS_SAMPLE_PAGES=65536",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "valid range for stats_sample_pages",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_row_string (id INT) ROW_FORMAT='COMPACT'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures +=
        execute_statement_ok(database, "CREATE TABLE max_sample (id INT) STATS_SAMPLE_PAGES=65535");
    failures += expect_show_create_contains(
        database,
        "max_sample",
        "STATS_SAMPLE_PAGES=65535",
        "maximum stats sample pages accepted"
    );

    mylite_close(database);
    return failures;
}

static int test_catalog_v30_migration_defaults(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "migration_v30") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open migration source");
    failures += create_app_schema(database);
    failures += execute_statement_ok(database, "CREATE TABLE migrated (id INT)");
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += make_catalog_look_like_v30(sqlite);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open migrated v30 catalog");
    failures += execute_statement_ok(database, "USE app");
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += expect_catalog_state_versions(
        sqlite,
        catalog_schema_version_after_storage_statistics_options,
        catalog_minimum_reader_version_after_storage_statistics_options,
        "v30 migration preserves minimum reader version"
    );
    failures += expect_show_create_not_contains(
        database,
        "migrated",
        "ROW_FORMAT=",
        "v30 migration defaults omit row format"
    );
    failures += expect_single_cell(
        database,
        "SELECT ROW_FORMAT FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'migrated'",
        0U,
        "Dynamic",
        "v30 migration default row format"
    );
    failures += expect_single_cell(
        database,
        "SELECT CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'migrated'",
        0U,
        "",
        "v30 migration default create options"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_file_option_state(void) {
    char left_path[test_path_capacity];
    char right_path[test_path_capacity];
    mylite_db *left = NULL;
    mylite_db *right = NULL;
    int failures = 0;

    if (make_test_path(left_path, sizeof(left_path), "left") != 0 ||
        make_test_path(right_path, sizeof(right_path), "right") != 0) {
        return 1;
    }
    remove_related_files(left_path);
    remove_related_files(right_path);

    failures += expect_int(mylite_open(left_path, &left), MYLITE_OK, "open left file");
    failures += expect_int(mylite_open(right_path, &right), MYLITE_OK, "open right file");
    failures += create_app_schema(left);
    failures += create_app_schema(right);
    failures += execute_statement_ok(left, "CREATE TABLE t (id INT) ROW_FORMAT=DYNAMIC");
    failures += execute_statement_ok(right, "CREATE TABLE t (id INT) ROW_FORMAT=COMPRESSED");
    failures += expect_single_cell(
        left,
        "SELECT CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 't'",
        0U,
        "row_format=DYNAMIC",
        "left file create options"
    );
    failures += expect_single_cell(
        right,
        "SELECT CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 't'",
        0U,
        "row_format=COMPRESSED",
        "right file create options"
    );

    mylite_close(left);
    mylite_close(right);
    remove_related_files(left_path);
    remove_related_files(right_path);
    return failures;
}

static int expect_catalog_state_versions(
    sqlite3 *sqlite,
    int64_t expected_schema_version,
    int64_t expected_minimum_reader_schema_version,
    const char *context
) {
    sqlite3_stmt *statement = NULL;
    int failures = 0;
    int sqlite_rc = sqlite3_prepare_v2(
        sqlite,
        "SELECT schema_version, minimum_reader_schema_version FROM _mylite_catalog_state",
        -1,
        &statement,
        NULL
    );

    if (sqlite_rc != SQLITE_OK) {
        fprintf(stderr, "%s: failed to prepare catalog state query\n", context);
        return 1;
    }
    sqlite_rc = sqlite3_step(statement);
    if (sqlite_rc != SQLITE_ROW) {
        fprintf(stderr, "%s: expected one catalog state row\n", context);
        sqlite3_finalize(statement);
        return 1;
    }

    failures += expect_int64(
        sqlite3_column_int64(statement, 0),
        expected_schema_version,
        "catalog schema version"
    );
    failures += expect_int64(
        sqlite3_column_int64(statement, 1),
        expected_minimum_reader_schema_version,
        "catalog minimum reader schema version"
    );

    sqlite_rc = sqlite3_step(statement);
    if (sqlite_rc != SQLITE_DONE) {
        fprintf(stderr, "%s: expected one catalog state row only\n", context);
        ++failures;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        fprintf(stderr, "%s: failed to finalize catalog state query\n", context);
        ++failures;
    }
    return failures;
}

static int make_catalog_look_like_v30(sqlite3 *sqlite) {
    int failures = 0;

    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN row_format_option");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN key_block_size");
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN pack_keys");
    failures += execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN checksum");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN stats_persistent");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN stats_auto_recalc");
    failures +=
        execute_sql(sqlite, "ALTER TABLE _mylite_catalog_tables DROP COLUMN stats_sample_pages");
    failures += execute_sql(
        sqlite,
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 30, minimum_reader_schema_version = 30"
    );

    return failures;
}

static int create_app_schema(mylite_db *database) {
    int failures = execute_dml_ok(database, "CREATE DATABASE app", 1U);

    failures += execute_statement_ok(database, "USE app");
    return failures;
}

static int expect_show_create_contains(
    mylite_db *database,
    const char *table_name, // NOLINT(bugprone-easily-swappable-parameters)
    const char *needle,
    const char *context
) {
    char sql[show_create_sql_capacity];
    int written = snprintf(sql, sizeof(sql), "SHOW CREATE TABLE %s", table_name);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "%s: SHOW CREATE SQL is too long\n", context);
        return 1;
    }
    return expect_single_cell_contains(database, sql, 1U, needle, context);
}

static int expect_show_create_not_contains(
    mylite_db *database,
    const char *table_name, // NOLINT(bugprone-easily-swappable-parameters)
    const char *needle,
    const char *context
) {
    char sql[show_create_sql_capacity];
    mylite_result *result = NULL;
    const char *actual = NULL;
    int written = snprintf(sql, sizeof(sql), "SHOW CREATE TABLE %s", table_name);
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "%s: SHOW CREATE SQL is too long\n", context);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    if (mylite_result_column_count(result) <= 1U) {
        fprintf(stderr, "%s: expected SHOW CREATE SQL column\n", context);
        failures += 1;
    } else {
        actual = mylite_result_value_text(result, 0U, 1U);
        failures += expect_text_not_contains(actual, needle, context);
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int expect_single_cell(
    mylite_db *database,
    const char *sql,
    size_t column_index,
    const char *expected,
    const char *context
) {
    mylite_result *result = NULL;
    const char *actual = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    if (mylite_result_column_count(result) <= column_index) {
        fprintf(stderr, "%s: expected column %zu in result\n", context, column_index);
        failures += 1;
    } else {
        actual = mylite_result_value_text(result, 0U, column_index);
        failures += expect_text(actual, expected, context);
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int expect_single_cell_contains(
    mylite_db *database,
    const char *sql,
    size_t column_index,
    const char *needle,
    const char *context
) {
    mylite_result *result = NULL;
    const char *actual = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    if (mylite_result_column_count(result) <= column_index) {
        fprintf(stderr, "%s: expected column %zu in result\n", context, column_index);
        failures += 1;
    } else {
        actual = mylite_result_value_text(result, 0U, column_index);
        failures += expect_text_contains(actual, needle, context);
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), 0, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result == NULL || *out_result == NULL) {
        fprintf(stderr, "%s: expected result object\n", sql);
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected error %d/%s, got success\n",
            sql,
            expected.code,
            expected.sqlstate
        );
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int execute_sql(sqlite3 *connection, const char *sql) {
    int rc = sqlite3_exec(connection, sql, NULL, NULL, NULL);

    if (rc == SQLITE_OK) {
        return 0;
    }

    fprintf(stderr, "sqlite exec failed for '%s': %d\n", sql, rc);
    return 1;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_table_storage_statistics_options_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
        return 1;
    }

    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return (int)getpid();
#endif
}

static void remove_related_files(const char *path) {
    (void)remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    if (bytes_read != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        return 1;
    }

    return 0;
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

    fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if ((actual == NULL && expected == NULL) ||
        (actual != NULL && expected != NULL && strcmp(actual, expected) == 0)) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected text \"%s\", got \"%s\"\n",
        context,
        expected == NULL ? "(null)" : expected,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected \"%s\" to contain \"%s\"\n",
        context,
        actual == NULL ? "(null)" : actual,
        needle == NULL ? "(null)" : needle
    );
    return 1;
}

static int expect_text_not_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) == NULL) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected \"%s\" not to contain \"%s\"\n",
        context,
        actual == NULL ? "(null)" : actual,
        needle == NULL ? "(null)" : needle
    );
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (actual != NULL && expected != NULL && memcmp(actual, expected, size) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: bytes differ\n", context);
    return 1;
}
