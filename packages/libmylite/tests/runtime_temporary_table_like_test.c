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
    show_table_status_column_count = 18,
    mysql_error_parse = 1064,
    mysql_error_table_exists = 1050,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_fulltext_temporary = 1796,
    mysql_error_table_does_not_exist = 1146,
};

struct expected_statement {
    int64_t affected_rows;
    size_t warning_count;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_temporary_like_success_metadata_persistence_and_cleanup(void);
static int test_temporary_like_source_shadowing_and_persistent_clone(void);
static int test_temporary_like_diagnostics(void);
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
static int expect_query_not_contains(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *needle,
    const char *context
);
static int expect_error(
    mylite_db *database,
    const char *sql,
    int expected_code,
    const char *message_part
);
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
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_temporary_like_success_metadata_persistence_and_cleanup();
    failures += test_temporary_like_source_shadowing_and_persistent_clone();
    failures += test_temporary_like_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_temporary_like_success_metadata_persistence_and_cleanup(void) {
    static const char *const status_rows[] = {"0", "0", "0"};
    static const char *const zero_count_rows[] = {"0"};
    static const char *const clone_rows[] = {"8", "abc"};
    static const char *const source_rows[] = {"1", "aa", "2", "bb"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures +=
        expect_statement(database, "CREATE DATABASE app", (struct expected_statement){1, 0U});
    failures += expect_statement(database, "USE app", (struct expected_statement){0, 0U});
    failures += expect_statement(
        database,
        "CREATE TABLE src(id INT NOT NULL DEFAULT 7, name VARCHAR(10), KEY name_key(name(3)))",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO src VALUES (1, 'aa'), (2, 'bb')",
        (struct expected_statement){2, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE temp_clone LIKE src",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, @@error_count",
            .values = status_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "temporary like status",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM temp_clone",
            .values = zero_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary like target starts empty",
        }
    );
    failures += expect_statement(
        database,
        "INSERT INTO temp_clone(id, name) VALUES (8, 'abc')",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, name FROM temp_clone",
            .values = clone_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "temporary like target accepts cloned columns",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, name FROM src ORDER BY id",
            .values = source_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "temporary like leaves source rows unchanged",
        }
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE temp_clone",
        0U,
        1U,
        "CREATE TEMPORARY TABLE `temp_clone`",
        "temporary like show create table"
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE temp_clone",
        0U,
        1U,
        "KEY `name_key` (`name`(3))",
        "temporary like cloned secondary index"
    );
    failures += expect_query_contains(
        database,
        "SHOW INDEX FROM temp_clone",
        0U,
        2U,
        "name_key",
        "temporary like show index"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS LIKE 'temp_clone'",
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 0U,
            .context = "temporary like omitted from show table status",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'temp_clone'",
            .values = zero_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary like omitted from information schema",
        }
    );

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "temporary like preserves preamble"
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_statement(database, "USE app", (struct expected_statement){0, 0U});
    failures += expect_error(
        database,
        "SELECT COUNT(*) FROM temp_clone",
        mysql_error_table_does_not_exist,
        "Table 'app.temp_clone' doesn't exist"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, name FROM src ORDER BY id",
            .values = source_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "persistent source survives reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_temporary_like_source_shadowing_and_persistent_clone(void) {
    static const char *const temp_source_rows[] = {"9"};
    static const char *const persistent_count_rows[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "shadow") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open shadow file");
    failures +=
        expect_statement(database, "CREATE DATABASE app", (struct expected_statement){1, 0U});
    failures += expect_statement(database, "USE app", (struct expected_statement){0, 0U});
    failures += expect_statement(
        database,
        "CREATE TABLE shadow_src(persistent_col INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE shadow_src(temp_col VARCHAR(8) NOT NULL DEFAULT 'x')",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE shadow_temp_clone LIKE shadow_src",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE shadow_temp_clone",
        0U,
        1U,
        "`temp_col` varchar(8) NOT NULL DEFAULT 'x'",
        "temporary source shadows persistent source for temporary clone"
    );
    failures += expect_statement(
        database,
        "INSERT INTO shadow_temp_clone VALUES ('9')",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT temp_col FROM shadow_temp_clone",
            .values = temp_source_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary source clone stores rows",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TABLE persistent_from_temp LIKE shadow_src",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE persistent_from_temp",
        0U,
        1U,
        "CREATE TABLE `persistent_from_temp`",
        "persistent target from temporary source is persistent"
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE persistent_from_temp",
        0U,
        1U,
        "`temp_col` varchar(8) NOT NULL DEFAULT 'x'",
        "persistent target clones temporary source descriptor"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'persistent_from_temp'",
            .values = persistent_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "persistent clone from temporary source is durable metadata",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_temporary_like_diagnostics(void) {
    static const char *const existing_persistent_status_rows[] = {"0", "0"};
    static const char *const one_count_rows[] = {"1"};
    static const char *const zero_count_rows[] = {"0"};
    static const char *const existing_temp_status_rows[] = {"0", "1", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures +=
        expect_statement(database, "CREATE DATABASE app", (struct expected_statement){1, 0U});
    failures += expect_statement(
        database,
        "CREATE TABLE app.src(id INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE no_default LIKE app.src",
        mysql_error_no_database_selected,
        "No database selected"
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE app.target_source_unqualified LIKE src",
        mysql_error_no_database_selected,
        "No database selected"
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE nosuch_target.dst LIKE nosuch_source.src",
        mysql_error_unknown_database,
        "Unknown database 'nosuch_source'"
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE nosuch_target.dst LIKE app.missing_source_precedence",
        mysql_error_table_does_not_exist,
        "Table 'app.missing_source_precedence' doesn't exist"
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE nosuch_target.dst LIKE app.src",
        mysql_error_unknown_database,
        "Unknown database 'nosuch_target'"
    );

    failures += expect_statement(database, "USE app", (struct expected_statement){0, 0U});
    failures += expect_statement(
        database,
        "CREATE TABLE existing_target(id INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO existing_target VALUES (1)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE existing_target LIKE src",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = existing_persistent_status_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "temporary target shadows persistent target status",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'existing_target'",
            .values = one_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "persistent target metadata remains visible",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM existing_target",
            .values = zero_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary target shadows persistent target rows",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE existing_temp(id INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE IF NOT EXISTS existing_temp LIKE src",
        (struct expected_statement){0, 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, @@error_count",
            .values = existing_temp_status_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "temporary like if not exists existing temporary target",
        }
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE existing_temp LIKE src",
        mysql_error_table_exists,
        "Table 'existing_temp' already exists"
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE IF NOT EXISTS existing_temp LIKE missing_source",
        mysql_error_table_does_not_exist,
        "Table 'app.missing_source' doesn't exist"
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE _mylite_temp LIKE src",
        mysql_error_incorrect_table_name,
        "Incorrect table name"
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE temp_reserved_source LIKE _mylite_source",
        mysql_error_incorrect_table_name,
        "Incorrect table name"
    );
    failures += expect_statement(
        database,
        "CREATE TABLE ai_src(id INT NOT NULL AUTO_INCREMENT PRIMARY KEY)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE ai_tmp LIKE ai_src",
        mysql_error_parse,
        "Temporary AUTO_INCREMENT tables are not yet supported"
    );
    failures += expect_statement(
        database,
        "CREATE TABLE ft_src(name VARCHAR(20), FULLTEXT KEY ft_name(name))",
        (struct expected_statement){0, 0U}
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE ft_tmp LIKE ft_src",
        mysql_error_fulltext_temporary,
        "Cannot create FULLTEXT index on temporary InnoDB table"
    );
    failures += expect_statement(
        database,
        "CREATE TABLE checked_src(value INT CHECK (value > 0))",
        (struct expected_statement){0, 0U}
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE checked_tmp LIKE checked_src",
        mysql_error_parse,
        "Temporary CHECK constraint tables are not yet supported"
    );
    failures += expect_statement(
        database,
        "CREATE TABLE parent_fk(id INT PRIMARY KEY)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TABLE child_fk(id INT PRIMARY KEY, parent_id INT, "
        "CONSTRAINT fk_child_parent FOREIGN KEY(parent_id) REFERENCES parent_fk(id))",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE child_fk_tmp LIKE child_fk",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE child_fk_tmp",
        0U,
        1U,
        "KEY `fk_child_parent` (`parent_id`)",
        "temporary like clones foreign-key supporting index"
    );
    failures += expect_query_not_contains(
        database,
        "SHOW CREATE TABLE child_fk_tmp",
        0U,
        1U,
        "FOREIGN KEY",
        "temporary like skips foreign-key constraint"
    );
    failures += expect_statement(database, "START TRANSACTION", (struct expected_statement){0, 0U});
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE tx_tmp LIKE src",
        mysql_error_parse,
        "Temporary table DDL inside an active transaction is not supported"
    );
    failures += expect_statement(database, "ROLLBACK", (struct expected_statement){0, 0U});
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE mixed_like (LIKE src, extra INT)",
        mysql_error_parse,
        "SQL syntax"
    );

    mylite_close(database);
    remove_related_files(path);
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
                size_t value_index = (row * query.column_count) + column;

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

static int expect_query_not_contains(
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
            failures += expect_size(
                strstr(mylite_result_value_text(result, row, column), needle) == NULL ? 0U : 1U,
                0U,
                context
            );
        }
    } else {
        fprintf(stderr, "%s failed: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_error(
    mylite_db *database,
    const char *sql,
    int expected_code,
    const char *message_part
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected_code, sql);
    failures += expect_size((size_t)(result != NULL), 0U, "error result");
    if (message_part != NULL) {
        failures += expect_contains(mylite_errmsg(database), message_part, sql);
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
    const char *actual = mylite_result_value_text(result, row, column);

    return expect_text_or_null(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_temporary_table_like_%d_%s.mylite",
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
        expected == NULL ? "(null)" : expected,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
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
    if (memcmp(actual, expected, size) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: byte mismatch\n", context);
    return 1;
}
