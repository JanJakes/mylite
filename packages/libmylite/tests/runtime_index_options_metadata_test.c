#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include "runtime/mylite_connection.h"
#include "runtime/mylite_diagnostics.h"
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
    path_suffix_capacity = 16,
    statistics_probe_field_count = 5,
    show_index_index_comment_column = 12,
    show_index_visible_column = 13,
    mysql_error_parse = 1064,
    mysql_error_wrong_usage = 1221,
    mysql_error_spatial_index_non_geometric = 1687,
    mysql_error_index_comment_too_long = 1688,
    mysql_error_primary_key_index_invisible = 3522,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_create_table_index_options_metadata_and_persistence(void);
static int test_create_and_alter_index_options(void);
static int test_primary_key_options_metadata(void);
static int test_index_options_diagnostics(void);
static int create_app_schema(mylite_db *database);
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
static int expect_single_cell_not_contains(
    mylite_db *database,
    const char *sql,
    size_t column_index,
    const char *needle,
    const char *context
);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_ok_with_warning_count(
    mylite_db *database,
    const char *sql,
    size_t warning_count
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static char *make_long_index_comment_sql(const char *prefix, const char *suffix);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_text_not_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_create_table_index_options_metadata_and_persistence();
    failures += test_create_and_alter_index_options();
    failures += test_primary_key_options_metadata();
    failures += test_index_options_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_create_table_index_options_metadata_and_persistence(void) {
    static const char *const statistics_rows[] = {
        "ft_c",
        "FULLTEXT",
        "",
        "text idx",
        "NO",
        "k_a",
        "BTREE",
        "",
        "a comment",
        "NO",
        "k_b",
        "BTREE",
        "",
        "hash note",
        "YES",
    };
    static const char *const show_create_rows[] = {
        "idx_options",
        "CREATE TABLE `idx_options` (\n"
        "  `id` int NOT NULL,\n"
        "  `a` int DEFAULT NULL,\n"
        "  `b` int DEFAULT NULL,\n"
        "  `body` text,\n"
        "  PRIMARY KEY (`id`),\n"
        "  KEY `k_a` (`a`) USING BTREE COMMENT 'a comment' /*!80000 INVISIBLE */,\n"
        "  KEY `k_b` (`b`) COMMENT 'hash note',\n"
        "  FULLTEXT KEY `ft_c` (`body`) COMMENT 'text idx' /*!80000 INVISIBLE */\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "create-table") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open index options file");
    failures += create_app_schema(database);
    failures += expect_statement_ok_with_warning_count(
        database,
        "CREATE TABLE idx_options ("
        "id INT PRIMARY KEY, a INT, b INT, body TEXT, "
        "KEY k_a USING BTREE (a) COMMENT 'a comment' INVISIBLE, "
        "KEY k_b (b) USING HASH COMMENT 'hash note', "
        "FULLTEXT KEY ft_c (body) COMMENT 'text idx' INVISIBLE)",
        1U
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE idx_options",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SHOW CREATE with index options",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, INDEX_TYPE, COMMENT, INDEX_COMMENT, IS_VISIBLE "
                   "FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'idx_options' AND INDEX_NAME <> 'PRIMARY' "
                   "ORDER BY INDEX_NAME",
            .values = statistics_rows,
            .column_count = statistics_probe_field_count,
            .row_count = 3U,
            .context = "I_S STATISTICS with index options",
        }
    );
    failures += expect_single_cell(
        database,
        "SHOW INDEX FROM idx_options WHERE Key_name = 'k_a'",
        show_index_index_comment_column,
        "a comment",
        "SHOW INDEX Index_comment"
    );
    failures += expect_single_cell(
        database,
        "SHOW INDEX FROM idx_options WHERE Key_name = 'k_a'",
        show_index_visible_column,
        "NO",
        "SHOW INDEX visible flag"
    );
    failures += execute_statement_ok(database, "CREATE TABLE idx_clone LIKE idx_options");
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE idx_clone",
        1U,
        "KEY `k_a` (`a`) USING BTREE COMMENT 'a comment' /*!80000 INVISIBLE */",
        "CREATE TABLE LIKE preserves explicit BTREE/comment/visibility"
    );
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE idx_clone",
        1U,
        "KEY `k_b` (`b`) COMMENT 'hash note'",
        "CREATE TABLE LIKE preserves HASH fallback metadata"
    );
    failures += mylite_test_expect_int(
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

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen index options file"
    );
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE idx_options",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "reopen preserves index options",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_create_and_alter_index_options(void) {
    static const char *const rows[] = {
        "k_alt",
        "BTREE",
        "alt hash",
        "YES",
        "k_btree",
        "BTREE",
        "created",
        "NO",
        "k_hash",
        "BTREE",
        "hashed",
        "YES",
        "u_a",
        "BTREE",
        "altered",
        "YES",
    };
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open memory");

    failures += create_app_schema(database);
    failures += execute_statement_ok(database, "CREATE TABLE t (id INT PRIMARY KEY, a INT, b INT)");
    failures += execute_statement_ok(
        database,
        "CREATE INDEX k_btree USING BTREE ON t (a) COMMENT 'created' INVISIBLE"
    );
    failures += expect_statement_ok_with_warning_count(
        database,
        "CREATE INDEX k_hash USING HASH ON t (b) COMMENT 'hashed'",
        1U
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE t ADD UNIQUE INDEX u_a USING BTREE (a) COMMENT 'altered' VISIBLE"
    );
    failures += expect_statement_ok_with_warning_count(
        database,
        "ALTER TABLE t ADD INDEX k_alt (b) USING HASH COMMENT 'alt hash'",
        1U
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, INDEX_TYPE, INDEX_COMMENT, IS_VISIBLE "
                   "FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 't' AND INDEX_NAME <> 'PRIMARY' ORDER BY INDEX_NAME",
            .values = rows,
            .column_count = 4U,
            .row_count = 4U,
            .context = "CREATE/ALTER index options metadata",
        }
    );
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE t",
        1U,
        "UNIQUE KEY `u_a` (`a`) USING BTREE COMMENT 'altered'",
        "ALTER ADD UNIQUE renders options"
    );
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE t",
        1U,
        "KEY `k_btree` (`a`) USING BTREE COMMENT 'created' /*!80000 INVISIBLE */",
        "CREATE INDEX renders options"
    );
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE t",
        1U,
        "KEY `k_hash` (`b`) COMMENT 'hashed'",
        "CREATE INDEX HASH fallback keeps comment"
    );
    failures += expect_single_cell_not_contains(
        database,
        "SHOW CREATE TABLE t",
        1U,
        "USING HASH",
        "HASH fallback is not rendered"
    );

    mylite_close(database);
    return failures;
}

static int test_primary_key_options_metadata(void) {
    static const char *const statistics_rows[] = {
        "PRIMARY",
        "BTREE",
        "",
        "pk comment",
        "YES",
    };
    static const char *const show_create_rows[] = {
        "pk_options",
        "CREATE TABLE `pk_options` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`) USING BTREE COMMENT 'pk comment'\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "primary-key-options") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open primary-key options file"
    );
    failures += create_app_schema(database);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE pk_options ("
        "id INT NOT NULL, v INT, PRIMARY KEY USING BTREE (id) COMMENT 'pk comment' VISIBLE)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE pk_options",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SHOW CREATE primary-key options",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, INDEX_TYPE, COMMENT, INDEX_COMMENT, IS_VISIBLE "
                   "FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'pk_options' AND INDEX_NAME = 'PRIMARY'",
            .values = statistics_rows,
            .column_count = statistics_probe_field_count,
            .row_count = 1U,
            .context = "I_S STATISTICS primary-key options",
        }
    );
    failures += expect_single_cell(
        database,
        "SHOW INDEX FROM pk_options WHERE Key_name = 'PRIMARY'",
        show_index_index_comment_column,
        "pk comment",
        "SHOW INDEX primary Index_comment"
    );
    failures += expect_single_cell(
        database,
        "SHOW INDEX FROM pk_options WHERE Key_name = 'PRIMARY'",
        show_index_visible_column,
        "YES",
        "SHOW INDEX primary visible flag"
    );
    failures += execute_statement_ok(database, "CREATE TABLE pk_clone LIKE pk_options");
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE pk_clone",
        1U,
        "PRIMARY KEY (`id`) USING BTREE COMMENT 'pk comment'",
        "CREATE TABLE LIKE preserves primary-key options"
    );
    failures += expect_statement_ok_with_warning_count(
        database,
        "CREATE TABLE pk_hash (id INT NOT NULL, PRIMARY KEY USING HASH (id) COMMENT 'hash pk')",
        1U
    );
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE pk_hash",
        1U,
        "PRIMARY KEY (`id`) COMMENT 'hash pk'",
        "primary HASH fallback keeps comment"
    );
    failures += expect_single_cell_not_contains(
        database,
        "SHOW CREATE TABLE pk_hash",
        1U,
        "USING",
        "primary HASH fallback is not rendered"
    );
    failures += execute_statement_ok(database, "CREATE TABLE pk_alter (id INT NOT NULL, v INT)");
    failures += execute_statement_ok(
        database,
        "ALTER TABLE pk_alter ADD PRIMARY KEY USING BTREE (id) COMMENT 'alter pk'"
    );
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE pk_alter",
        1U,
        "PRIMARY KEY (`id`) USING BTREE COMMENT 'alter pk'",
        "ALTER ADD PRIMARY KEY renders options"
    );
    failures += execute_statement_ok(database, "CREATE TABLE pk_alter_hash (id INT NOT NULL)");
    failures += expect_statement_ok_with_warning_count(
        database,
        "ALTER TABLE pk_alter_hash ADD PRIMARY KEY USING HASH (id) COMMENT 'alter hash'",
        1U
    );
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE pk_alter_hash",
        1U,
        "PRIMARY KEY (`id`) COMMENT 'alter hash'",
        "ALTER ADD PRIMARY KEY HASH fallback keeps comment"
    );
    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read primary-key options preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "primary-key options preamble unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen primary-key options file"
    );
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE pk_options",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "reopen preserves primary-key options",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_index_options_diagnostics(void) {
    char *create_long = NULL;
    char *alter_long = NULL;
    char *create_primary_long = NULL;
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open diagnostics"
    );

    failures += create_app_schema(database);
    failures += execute_statement_ok(database, "CREATE TABLE t (a INT, body TEXT)");
    failures += execute_error(
        database,
        "CREATE INDEX bad_hash USING HASH ON t (a DESC)",
        (struct expected_sql_error){
            .code = mysql_error_wrong_usage,
            .sqlstate = "HY000",
            .message_part =
                "Incorrect usage of spatial/fulltext/hash index and explicit index order",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_primary_hash (id INT NOT NULL, PRIMARY KEY USING HASH (id DESC))",
        (struct expected_sql_error){
            .code = mysql_error_wrong_usage,
            .sqlstate = "HY000",
            .message_part =
                "Incorrect usage of spatial/fulltext/hash index and explicit index order",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_primary_invisible (id INT NOT NULL, PRIMARY KEY (id) INVISIBLE)",
        (struct expected_sql_error){
            .code = mysql_error_primary_key_index_invisible,
            .sqlstate = "HY000",
            .message_part = "A primary key index cannot be invisible.",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_primary_rtree (id INT NOT NULL, PRIMARY KEY USING RTREE (id))",
        (struct expected_sql_error){
            .code = mysql_error_spatial_index_non_geometric,
            .sqlstate = "42000",
            .message_part = "A SPATIAL index may only contain a geometrical type column",
        }
    );
    failures +=
        execute_statement_ok(database, "CREATE TABLE alter_primary_invisible (id INT NOT NULL)");
    failures += execute_error(
        database,
        "ALTER TABLE alter_primary_invisible ADD PRIMARY KEY (id) INVISIBLE",
        (struct expected_sql_error){
            .code = mysql_error_primary_key_index_invisible,
            .sqlstate = "HY000",
            .message_part = "A primary key index cannot be invisible.",
        }
    );
    failures +=
        execute_statement_ok(database, "CREATE TABLE alter_primary_rtree (id INT NOT NULL)");
    failures += execute_error(
        database,
        "ALTER TABLE alter_primary_rtree ADD PRIMARY KEY USING RTREE (id)",
        (struct expected_sql_error){
            .code = mysql_error_spatial_index_non_geometric,
            .sqlstate = "42000",
            .message_part = "A SPATIAL index may only contain a geometrical type column",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_fulltext (body TEXT, FULLTEXT KEY ft_body (body) USING BTREE)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE FULLTEXT INDEX ft_body ON t (body) USING HASH",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX numeric_comment ON t (a) COMMENT 123",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX nul_comment ON t (a) COMMENT 'a\\0b'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "index comments do not support NUL bytes",
        }
    );
    failures += execute_statement_ok(
        database,
        "CREATE INDEX four_byte_comment ON t (a) COMMENT '\360\237\231\202'"
    );
    failures += expect_single_cell(
        database,
        "SHOW INDEX FROM t WHERE Key_name = 'four_byte_comment'",
        show_index_index_comment_column,
        "?",
        "four-byte index comment replaced"
    );

    create_long = make_long_index_comment_sql("CREATE INDEX too_long ON t (a) COMMENT '", "'");
    alter_long = make_long_index_comment_sql("ALTER TABLE t ADD INDEX too_long (a) COMMENT '", "'");
    create_primary_long = make_long_index_comment_sql(
        "CREATE TABLE pk_too_long (id INT NOT NULL, PRIMARY KEY (id) COMMENT '",
        "')"
    );
    if (create_long == NULL || alter_long == NULL || create_primary_long == NULL) {
        fprintf(stderr, "failed to allocate long index comments\n");
        failures += 1;
    } else {
        failures += execute_error(
            database,
            create_long,
            (struct expected_sql_error){
                .code = mysql_error_index_comment_too_long,
                .sqlstate = "HY000",
                .message_part = "Comment for index 'too_long' is too long (max = 1024)",
            }
        );
        failures += execute_error(
            database,
            alter_long,
            (struct expected_sql_error){
                .code = mysql_error_index_comment_too_long,
                .sqlstate = "HY000",
                .message_part = "Comment for index 'too_long' is too long (max = 1024)",
            }
        );
        failures += execute_error(
            database,
            create_primary_long,
            (struct expected_sql_error){
                .code = mysql_error_index_comment_too_long,
                .sqlstate = "HY000",
                .message_part = "Comment for index 'PRIMARY' is too long (max = 1024)",
            }
        );
    }

    free(create_long);
    free(alter_long);
    free(create_primary_long);
    mylite_close(database);
    return failures;
}

static int create_app_schema(mylite_db *database) {
    int failures = execute_statement_ok(database, "CREATE DATABASE app");

    failures += execute_statement_ok(database, "USE app");
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
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
        failures += expect_result_value(result, 0U, column_index, expected, context);
    }
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
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        const char *actual = mylite_result_value_text(result, 0U, column_index);

        failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
        failures += mylite_test_expect_contains(actual, needle, context);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_single_cell_not_contains(
    mylite_db *database,
    const char *sql,
    size_t column_index,
    const char *needle,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        const char *actual = mylite_result_value_text(result, 0U, column_index);

        failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
        failures += expect_text_not_contains(actual, needle, context);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    return expect_statement_ok_with_warning_count(database, sql, 0U);
}

static int expect_statement_ok_with_warning_count(
    mylite_db *database,
    const char *sql,
    size_t warning_count
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), warning_count, sql);
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
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    const struct mylite_diagnostics *diagnostics = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error\n", sql);
        mylite_result_free(result);
        return 1;
    }

    diagnostics = mylite_connection_diagnostics(database);
    failures += mylite_test_expect_int(mylite_diagnostics_errcode(diagnostics), expected.code, sql);
    failures +=
        mylite_test_expect_text(mylite_diagnostics_sqlstate(diagnostics), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(
        mylite_diagnostics_errmsg(diagnostics),
        expected.message_part,
        sql
    );
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            query.column_count,
            query.context
        );
        failures += mylite_test_expect_size(
            mylite_result_row_count(result),
            query.row_count,
            query.context
        );
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, query.context);
    }
    for (size_t row = 0U; failures == 0 && row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }

    mylite_result_free(result);
    return failures;
}

static char *make_long_index_comment_sql(const char *prefix, const char *suffix) {
    enum { too_long_comment_characters = 1025 };

    size_t prefix_length = strlen(prefix);
    size_t suffix_length = strlen(suffix);
    size_t sql_length = prefix_length + too_long_comment_characters + suffix_length;
    char *sql = malloc(sql_length + 1U);

    if (sql == NULL) {
        return NULL;
    }
    memcpy(sql, prefix, prefix_length);
    memset(sql + prefix_length, 'a', too_long_comment_characters);
    memcpy(sql + prefix_length + too_long_comment_characters, suffix, suffix_length);
    sql[sql_length] = '\0';
    return sql;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-journal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return -1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fclose(file);
        return -1;
    }
    return fclose(file);
}

static int expect_text_not_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        fprintf(stderr, "%s: expected \"%s\" not to contain \"%s\"\n", context, actual, needle);
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
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    return mylite_test_expect_text(actual, expected, context);
}
