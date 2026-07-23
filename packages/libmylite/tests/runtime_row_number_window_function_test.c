#include "mylite_test_support.h"

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
    path_suffix_capacity = 16,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_error_window_name_not_defined = 3579,
    seed_post_count = 7,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_no_source_and_table_row_number(void);
static int test_row_number_diagnostics(void);
static int open_app_database(mylite_db **out_database, char *path, size_t path_size);
static int seed_posts(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_row_number_metadata(mylite_db *database);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_no_source_and_table_row_number();
    failures += test_row_number_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_and_table_row_number(void) {
    static const char *const column_rn[] = {"rn"};
    static const char *const columns_id_rn[] = {"id", "rn"};
    static const char *const columns_id_two_rn[] = {"id", "rn_rows", "rn_range"};
    static const char *const columns_partitioned[] = {"id", "author_id", "rn"};
    static const char *const columns_category[] = {"id", "category", "rn"};
    static const char *const value_one[] = {"1"};
    static const char *const values_empty_window[] = {
        "1",
        "1",
        "2",
        "2",
        "3",
        "3",
        "4",
        "4",
        "5",
        "5",
        "6",
        "6",
        "7",
        "7",
    };
    static const char *const values_partition_desc[] = {
        "7", NULL, "1",  "6", NULL, "2",  "2", "10", "1",  "3", "10",
        "2", "1",  "10", "3", "5",  "20", "1", "4",  "20", "2",
    };
    static const char *const values_asc_nulls[] = {
        "4",
        "1",
        "6",
        "2",
        "7",
        "3",
        "5",
        "4",
        "1",
        "5",
        "2",
        "6",
        "3",
        "7",
    };
    static const char *const values_frame_clauses[] = {
        "4", "1", "1", "6", "2", "2", "7", "3", "3", "5", "4",
        "4", "1", "5", "5", "2", "6", "6", "3", "7", "7",
    };
    static const char *const values_desc_nulls[] = {
        "2",
        "1",
        "3",
        "2",
        "1",
        "3",
        "5",
        "4",
        "7",
        "5",
        "6",
        "6",
        "4",
        "7",
    };
    static const char *const values_category[] = {
        "4", NULL, "1",     "6", NULL, "2",    "1", "alpha", "1",    "2", "alpha",
        "2", "3",  "Alpha", "3", "5",  "beta", "1", "7",     "beta", "2",
    };
    static const char *const values_filtered_limited[] = {
        "7",
        "1",
        "6",
        "2",
        "2",
        "1",
        "3",
        "2",
    };
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    mylite_file_preamble_init(expected_preamble);
    failures += open_app_database(&database, path, sizeof(path));
    if (failures == 0) {
        failures += seed_posts(database);
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_NUMBER() OVER () AS rn",
            .columns = column_rn,
            .column_count = 1U,
            .values = value_one,
            .row_count = 1U,
            .context = "no-source row_number",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_NUMBER() OVER () AS rn FROM DUAL",
            .columns = column_rn,
            .column_count = 1U,
            .values = value_one,
            .row_count = 1U,
            .context = "dual row_number",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ROW_NUMBER() OVER () AS rn FROM posts ORDER BY id",
            .columns = columns_id_rn,
            .column_count = sizeof(columns_id_rn) / sizeof(columns_id_rn[0]),
            .values = values_empty_window,
            .row_count = seed_post_count,
            .context = "table empty window",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, author_id, "
                   "ROW_NUMBER() OVER (PARTITION BY author_id ORDER BY created_at DESC) "
                   "AS rn FROM posts ORDER BY author_id, created_at DESC, id",
            .columns = columns_partitioned,
            .column_count = sizeof(columns_partitioned) / sizeof(columns_partitioned[0]),
            .values = values_partition_desc,
            .row_count = seed_post_count,
            .context = "partitioned desc window",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ROW_NUMBER() OVER (ORDER BY created_at ASC) AS rn "
                   "FROM posts ORDER BY created_at ASC, id",
            .columns = columns_id_rn,
            .column_count = sizeof(columns_id_rn) / sizeof(columns_id_rn[0]),
            .values = values_asc_nulls,
            .row_count = seed_post_count,
            .context = "ascending nullable order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "ROW_NUMBER() OVER (ORDER BY created_at "
                   "ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS rn_rows, "
                   "ROW_NUMBER() OVER (ORDER BY created_at RANGE CURRENT ROW) AS rn_range "
                   "FROM posts ORDER BY created_at, id",
            .columns = columns_id_two_rn,
            .column_count = sizeof(columns_id_two_rn) / sizeof(columns_id_two_rn[0]),
            .values = values_frame_clauses,
            .row_count = seed_post_count,
            .context = "row_number frame clauses",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ROW_NUMBER() OVER (ORDER BY created_at DESC) AS rn "
                   "FROM posts ORDER BY created_at DESC, id",
            .columns = columns_id_rn,
            .column_count = sizeof(columns_id_rn) / sizeof(columns_id_rn[0]),
            .values = values_desc_nulls,
            .row_count = seed_post_count,
            .context = "descending nullable order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, category, "
                   "ROW_NUMBER() OVER (PARTITION BY category ORDER BY posts.id) AS rn "
                   "FROM posts ORDER BY category, id",
            .columns = columns_category,
            .column_count = sizeof(columns_category) / sizeof(columns_category[0]),
            .values = values_category,
            .row_count = seed_post_count,
            .context = "string partition window",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ROW_NUMBER() OVER "
                   "(PARTITION BY author_id ORDER BY created_at DESC) AS rn "
                   "FROM posts WHERE id >= 2 ORDER BY author_id, created_at DESC, id LIMIT 4",
            .columns = columns_id_rn,
            .column_count = sizeof(columns_id_rn) / sizeof(columns_id_rn[0]),
            .values = values_filtered_limited,
            .row_count = 4U,
            .context = "where order limit window",
        }
    );
    failures += expect_row_number_metadata(database);

    mylite_close(database);
    database = NULL;

    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read file preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "preamble after row_number selects"
    );

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen app database");
    if (failures == 0) {
        failures += execute_ok(database, "USE app", NULL);
    }
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ROW_NUMBER() OVER (ORDER BY created_at DESC) AS rn "
                   "FROM posts ORDER BY created_at DESC, id LIMIT 3",
            .columns = columns_id_rn,
            .column_count = sizeof(columns_id_rn) / sizeof(columns_id_rn[0]),
            .values = values_desc_nulls,
            .row_count = 3U,
            .context = "reopen row_number persistence",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_row_number_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, path, sizeof(path));
    if (failures == 0) {
        failures += seed_posts(database);
    }

    failures += execute_error(
        database,
        "SELECT ROW_NUMBER()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROW_NUMBER(1) OVER ()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROW_NUMBER() OVER (PARTITION BY id)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ROW_NUMBER() without a table source supports only OVER ()",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, ROW_NUMBER() OVER (ORDER BY missing) AS rn FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, ROW_NUMBER() OVER (PARTITION BY missing) AS rn FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, ROW_NUMBER() OVER (ORDER BY 1) AS rn FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "window functions support only descriptor columns in ORDER BY",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, ROW_NUMBER() OVER (PARTITION BY author_id, title) AS rn FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "window functions support only descriptor columns in PARTITION BY",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, ROW_NUMBER() OVER (base_window ORDER BY id) AS rn FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_window_name_not_defined,
            .sqlstate = "HY000",
            .message_part = "Window name 'base_window' is not defined.",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int open_app_database(mylite_db **out_database, char *path, size_t path_size) {
    int failures = 0;

    if (mylite_test_make_path(path, path_size, "row_number") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures +=
        mylite_test_expect_int(mylite_open(path, out_database), MYLITE_OK, "open app database");
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static int seed_posts(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(
        database,
        "CREATE TABLE posts("
        "id INT, author_id INT, created_at INT, category VARCHAR(20), title VARCHAR(20)"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO posts VALUES "
        "(1, 10, 100, 'alpha', 'a'), "
        "(2, 10, 200, 'alpha', 'b'), "
        "(3, 10, 200, 'Alpha', 'c'), "
        "(4, 20, NULL, NULL, 'd'), "
        "(5, 20, 50, 'beta', 'e'), "
        "(6, NULL, 10, NULL, 'f'), "
        "(7, NULL, 20, 'beta', 'g')",
        NULL
    );
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': rc=%d err=%d state=%s message=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[value_index],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_row_number_metadata(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(
        database,
        "SELECT id, ROW_NUMBER() OVER (ORDER BY id) AS rn FROM posts ORDER BY id LIMIT 1",
        &result
    );

    if (failures != 0) {
        return failures;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        2U,
        "row_number metadata columns"
    );
    failures +=
        mylite_test_expect_text(mylite_result_column_name(result, 1U), "rn", "row_number alias");
    failures += mylite_test_expect_int(
        mylite_result_column_type(result, 1U),
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        "row_number type"
    );
    failures +=
        mylite_test_expect_int(mylite_result_column_nullable(result, 1U), 0, "row_number nullable");
    failures += mylite_test_expect_int(
        (int)(mylite_result_column_flags(result, 1U) & MYLITE_RESULT_COLUMN_FLAG_NOT_NULL),
        MYLITE_RESULT_COLUMN_FLAG_NOT_NULL,
        "row_number not-null flag"
    );
    failures += mylite_test_expect_int(
        (int)(mylite_result_column_flags(result, 1U) & MYLITE_RESULT_COLUMN_FLAG_UNSIGNED),
        MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
        "row_number unsigned flag"
    );
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        0,
        "row_number metadata affected"
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        0U,
        "row_number metadata warnings"
    );
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

    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "(null)" : expected,
                actual == NULL ? "(null)" : actual
            );
            return 1;
        }
        return 0;
    }
    return mylite_test_expect_text(actual, expected, context);
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    int status = 0;

    if (file == NULL) {
        return -1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
        status = -1;
    }
    if (fclose(file) != 0) {
        status = -1;
    }
    return status;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
