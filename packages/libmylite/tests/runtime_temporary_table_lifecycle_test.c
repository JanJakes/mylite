#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

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
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_table = 1051,
    mysql_error_table_does_not_exist = 1146,
    show_columns_column_count = 6,
    show_index_column_count = 15,
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_statement {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_temporary_shadowing_metadata_drop_and_cleanup(void);
static int test_temporary_if_exists_and_diagnostics(void);
static int test_temporary_keyword_remains_nonreserved_identifier(void);
static int test_schema_qualified_temporary_table_survives_schema_drop(void);
static int test_temporary_table_dml_transactions(void);
static int test_independent_handles_have_independent_temporary_tables(void);
static int seed_app_schema(mylite_db *database);
static int expect_statement(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_query_contains(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *needle,
    const char *context
);
static int expect_error(mylite_db *database, const char *sql, int expected_code);
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
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_temporary_shadowing_metadata_drop_and_cleanup();
    failures += test_temporary_if_exists_and_diagnostics();
    failures += test_temporary_keyword_remains_nonreserved_identifier();
    failures += test_schema_qualified_temporary_table_survives_schema_drop();
    failures += test_temporary_table_dml_transactions();
    failures += test_independent_handles_have_independent_temporary_tables();

    return failures == 0 ? 0 : 1;
}

static int test_temporary_shadowing_metadata_drop_and_cleanup(void) {
    static const char *const temp_rows[] = {"2", "250", "40", "3", "300", NULL};
    static const char *const persistent_rows[] = {"1", "10"};
    static const char *const persistent_reopened_rows[] = {"1", "10", "2", "20"};
    static const char *const temp_columns[] = {
        "id",   "int",         "NO",  "PRI", NULL, "", "v",     "int", "YES", "", NULL, "",
        "note", "varchar(10)", "YES", "MUL", NULL, "", "added", "int", "YES", "", NULL, "",
    };
    static const char *const temp_indexes[] = {
        "shadowed", "0", "PRIMARY", "1",   "id",  "A",        "0", NULL,       NULL,  "",
        "BTREE",    "",  "",        "YES", NULL,  "shadowed", "1", "note_idx", "1",   "note",
        "A",        "0", "3",       NULL,  "YES", "BTREE",    "",  "",         "YES", NULL,
    };
    static const char *const persistent_show_tables[] = {"shadowed"};
    static const char *const zero_count_rows[] = {"0"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "shadowing") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open shadowing file");
    failures += seed_app_schema(database);
    failures += expect_statement(
        database,
        "CREATE TABLE shadowed (id INT, persistent_value INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO shadowed VALUES (1, 10), (2, 20)",
        (struct expected_statement){2, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE shadowed ("
        "id INT NOT NULL PRIMARY KEY, v INT, note VARCHAR(10), KEY note_idx (note(3)))",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO shadowed VALUES (1, 100, 'abc'), (2, 200, 'bcd'), (3, 300, 'cde')",
        (struct expected_statement){3, 0U}
    );
    failures += expect_statement(
        database,
        "UPDATE shadowed SET v = 250 WHERE id = 2",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        database,
        "DELETE FROM shadowed WHERE id = 1",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        database,
        "ALTER TABLE shadowed ADD COLUMN added INT",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "UPDATE shadowed SET added = 40 WHERE id = 2",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, added FROM shadowed ORDER BY id",
            .values = temp_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "temporary table shadows persistent table for DML and ALTER",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM shadowed",
            .values = temp_columns,
            .column_count = show_columns_column_count,
            .row_count = 4U,
            .context = "SHOW COLUMNS sees altered temporary descriptor",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM shadowed",
            .values = temp_indexes,
            .column_count = show_index_column_count,
            .row_count = 2U,
            .context = "SHOW INDEX sees temporary descriptor",
        }
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE shadowed",
        0U,
        1U,
        "CREATE TEMPORARY TABLE `shadowed`",
        "SHOW CREATE TABLE renders temporary table"
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE shadowed",
        0U,
        1U,
        "KEY `note_idx` (`note`(3))",
        "SHOW CREATE TABLE renders temporary prefix index"
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE shadowed",
        0U,
        1U,
        "`added` int",
        "SHOW CREATE TABLE renders added temporary column"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW TABLES LIKE 'shadowed'",
            .values = persistent_show_tables,
            .column_count = 1U,
            .row_count = 1U,
            .context = "SHOW TABLES still reports persistent table only",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE temp_only (id INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW TABLES LIKE 'temp_only'",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "SHOW TABLES omits temporary-only table",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'temp_only'",
            .values = zero_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "INFORMATION_SCHEMA.TABLES omits temporary table",
        }
    );
    failures +=
        expect_statement(database, "DROP TABLE shadowed", (struct expected_statement){0, 0U});
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, persistent_value FROM shadowed ORDER BY id LIMIT 1",
            .values = persistent_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "plain DROP TABLE drops temporary shadow first",
        }
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen shadowing file");
    failures += expect_statement(database, "USE app", (struct expected_statement){0, 0U});
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, persistent_value FROM shadowed ORDER BY id",
            .values = persistent_reopened_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "temporary tables are cleaned up at close",
        }
    );
    failures +=
        expect_error(database, "SELECT COUNT(*) FROM temp_only", mysql_error_table_does_not_exist);
    mylite_close(database);

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read shadowing preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "temporary lifecycle preserves MyLite preamble"
    );

    remove_related_files(path);
    return failures;
}

static int test_temporary_if_exists_and_diagnostics(void) {
    static const char *const empty_temporary_rows[] = {"0"};
    static const char *const persistent_count_rows[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "if_exists") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open if-exists file");
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE no_default (id INT)",
        mysql_error_no_database_selected
    );
    failures += seed_app_schema(database);
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE missing_schema.bad_tmp (id INT)",
        mysql_error_unknown_database
    );
    failures += expect_statement(
        database,
        "CREATE TABLE persistent_only (id INT)",
        (struct expected_statement){0, 0U}
    );
    failures +=
        expect_error(database, "DROP TEMPORARY TABLE persistent_only", mysql_error_unknown_table);
    failures += expect_statement(
        database,
        "DROP TEMPORARY TABLE IF EXISTS persistent_only",
        (struct expected_statement){0, 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'persistent_only'",
            .values = persistent_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "DROP TEMPORARY IF EXISTS leaves persistent table",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE IF NOT EXISTS persistent_only (id INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM persistent_only",
            .values = empty_temporary_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "CREATE TEMPORARY IF NOT EXISTS ignores persistent table",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE IF NOT EXISTS persistent_only (id INT)",
        (struct expected_statement){0, 1U}
    );
    failures += expect_statement(
        database,
        "DROP TEMPORARY TABLE persistent_only",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM persistent_only",
            .values = empty_temporary_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "DROP TEMPORARY reveals persistent table",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_temporary_keyword_remains_nonreserved_identifier(void) {
    static const char *const values[] = {"7"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "nonreserved_identifier") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open nonreserved file");
    failures += seed_app_schema(database);
    failures += expect_statement(
        database,
        "CREATE TABLE temporary (temporary INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO temporary VALUES (7)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT temporary FROM temporary",
            .values = values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "TEMPORARY remains a nonreserved identifier",
        }
    );
    mylite_close(database);

    remove_related_files(path);
    return failures;
}

static int test_schema_qualified_temporary_table_survives_schema_drop(void) {
    static const char *const temp_rows[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "schema_drop") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open schema-drop file");
    failures +=
        expect_statement(database, "CREATE DATABASE other", (struct expected_statement){1, 0U});
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE other.keep_tmp (id INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO other.keep_tmp VALUES (1)",
        (struct expected_statement){1, 0U}
    );
    failures +=
        expect_statement(database, "DROP DATABASE other", (struct expected_statement){0, 0U});
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM other.keep_tmp",
            .values = temp_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "qualified temporary table survives dropped schema",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_temporary_table_dml_transactions(void) {
    static const char *const zero_rows[] = {"0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "transactions") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open transactions file");
    failures += seed_app_schema(database);
    failures += expect_statement(
        database,
        "CREATE TABLE p_tx (id INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE tx_rows (id INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(database, "START TRANSACTION", (struct expected_statement){0, 0U});
    failures += expect_statement(
        database,
        "INSERT INTO tx_rows VALUES (1)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(database, "ROLLBACK", (struct expected_statement){0, 0U});
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM tx_rows",
            .values = zero_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary DML rolls back",
        }
    );
    failures += expect_statement(database, "START TRANSACTION", (struct expected_statement){0, 0U});
    failures += expect_statement(
        database,
        "INSERT INTO p_tx VALUES (1)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE tx_created (id INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(database, "ROLLBACK", (struct expected_statement){0, 0U});
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM p_tx",
            .values = zero_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary create does not commit active transaction",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM tx_created",
            .values = zero_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary create survives rollback without rows",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE tx_drop (id INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(database, "START TRANSACTION", (struct expected_statement){0, 0U});
    failures += expect_statement(
        database,
        "INSERT INTO p_tx VALUES (2)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        database,
        "DROP TEMPORARY TABLE tx_drop",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(database, "ROLLBACK", (struct expected_statement){0, 0U});
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM p_tx",
            .values = zero_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary drop does not commit active transaction",
        }
    );
    failures +=
        expect_error(database, "SELECT COUNT(*) FROM tx_drop", mysql_error_table_does_not_exist);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_handles_have_independent_temporary_tables(void) {
    static const char *const first_temp_rows[] = {"1", "100"};
    static const char *const second_persistent_rows[] = {"1", "10"};
    char path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "handles") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &first), MYLITE_OK, "open first handle");
    failures += seed_app_schema(first);
    failures += expect_statement(
        first,
        "CREATE TABLE shared (id INT, value INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        first,
        "INSERT INTO shared VALUES (1, 10)",
        (struct expected_statement){1, 0U}
    );

    failures += expect_int(mylite_open(path, &second), MYLITE_OK, "open second handle");
    failures += expect_statement(second, "USE app", (struct expected_statement){0, 0U});
    failures += expect_statement(
        first,
        "CREATE TEMPORARY TABLE shared (id INT, value INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        first,
        "INSERT INTO shared VALUES (1, 100)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, value FROM shared",
            .values = first_temp_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first handle reads its temporary shadow",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, value FROM shared",
            .values = second_persistent_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second handle reads persistent table",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(path);
    return failures;
}

static int seed_app_schema(mylite_db *database) {
    int failures = 0;

    failures +=
        expect_statement(database, "CREATE DATABASE app", (struct expected_statement){1, 0U});
    failures += expect_statement(database, "USE app", (struct expected_statement){0, 0U});
    return failures;
}

static int expect_statement(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_column_count(result), 0U, "statement columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "statement rows");
        failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    } else {
        fprintf(stderr, "%s failed: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);

    failures += expect_int(rc, MYLITE_OK, query.context);
    if (rc == MYLITE_OK) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
        failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                const size_t value_index = (row * query.column_count) + column;

                failures += expect_result_value(
                    result,
                    row,
                    column,
                    query.values == NULL ? NULL : query.values[value_index],
                    query.context
                );
            }
        }
    } else {
        fprintf(stderr, "%s failed: %s\n", query.sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_contains(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *needle,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_OK, context);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_row_count(result) > row ? 1U : 0U, 1U, context);
        failures += expect_size(mylite_result_column_count(result) > column ? 1U : 0U, 1U, context);
        if (mylite_result_row_count(result) > row && mylite_result_column_count(result) > column) {
            failures +=
                expect_contains(mylite_result_value_text(result, row, column), needle, context);
        }
    } else {
        fprintf(stderr, "%s failed: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, const char *sql, int expected_code) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected_code, sql);
    failures += expect_size((size_t)(result != NULL), 0U, "error result");
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
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        return expect_size((size_t)(actual != NULL), 0U, context);
    }
    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_temporary_table_lifecycle_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path truncated\n");
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
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-wal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    (void)remove(related);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    return bytes_read == size ? 0 : 1;
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", context, expected, actual);
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }

    fprintf(stderr, "%s: expected \"%s\" to contain \"%s\"\n", context, actual, needle);
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: bytes differ\n", context);
    return 1;
}
