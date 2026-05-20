#include <mylite/mylite.h>

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
    show_create_column_count = 2,
    show_columns_column_count = 6,
    show_index_column_count = 15,
    information_schema_columns_column_count = 6,
    statistics_column_count = 6,
    wp_users_column_row_count = 10,
    wp_postmeta_column_row_count = 4,
    wp_postmeta_index_row_count = 3,
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_wordpress_core_fixture_setup_persistence_and_preamble(void);
static int test_wordpress_core_fixture_independent_handles(void);
static int create_fixture_schema(mylite_db *database);
static int create_wordpress_fixture_tables(mylite_db *database);
static int verify_fixture_metadata(mylite_db *database, bool check_show_create);
static int verify_fixture_rows(mylite_db *database, const char *context);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
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
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_wordpress_core_fixture_setup_persistence_and_preamble();
    failures += test_wordpress_core_fixture_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_wordpress_core_fixture_setup_persistence_and_preamble(void) {
    static const char *const wp_users_insert_rows[] = {
        "1",
        "",
        "0000-00-00 00:00:00",
        "0",
    };
    static const char *const wp_options_rows[] = {
        "1",
        "siteurl",
        "https://example.test",
        "yes",
    };
    static const char *const wp_postmeta_rows[] = {
        "1",
        "1",
        "k",
        "v",
        "2",
        "2",
        NULL,
        NULL,
        "3",
        "0",
        "omitted",
        "default",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "setup") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open fixture file");
    failures += create_fixture_schema(database);
    failures += create_wordpress_fixture_tables(database);
    failures += verify_fixture_metadata(database, true);
    failures += expect_dml_ok(database, "INSERT INTO wp_users () VALUES ()", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ID, user_login, user_registered, user_status FROM wp_users",
            .values = wp_users_insert_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "wp_users omitted defaults insert",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO wp_options (option_name, option_value) "
        "VALUES ('siteurl', 'https://example.test')",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT option_id, option_name, option_value, autoload FROM wp_options",
            .values = wp_options_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "wp_options inserted row",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO wp_postmeta (post_id, meta_key, meta_value) "
        "VALUES (1, 'k', 'v'), (2, NULL, NULL)",
        2
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO wp_postmeta (meta_key, meta_value) VALUES ('omitted', 'default')",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT meta_id, post_id, meta_key, meta_value FROM wp_postmeta "
                   "ORDER BY meta_id",
            .values = wp_postmeta_rows,
            .column_count = 4U,
            .row_count = 3U,
            .context = "wp_postmeta inserted rows",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "fixture preamble after close"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen fixture file");
    failures += expect_statement_ok(database, "USE wp");
    failures += verify_fixture_metadata(database, false);
    failures += verify_fixture_rows(database, "reopened fixture rows");

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_wordpress_core_fixture_independent_handles(void) {
    static const char *const first_rows[] = {"1", "first"};
    static const char *const second_rows[] = {"1", "second"};
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first fixture file");
    failures +=
        expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second fixture file");
    failures += create_fixture_schema(first);
    failures += create_fixture_schema(second);
    failures += create_wordpress_fixture_tables(first);
    failures += create_wordpress_fixture_tables(second);
    failures += expect_dml_ok(
        first,
        "INSERT INTO wp_options (option_name, option_value) VALUES ('name', 'first')",
        1
    );
    failures += expect_dml_ok(
        second,
        "INSERT INTO wp_options (option_name, option_value) VALUES ('name', 'second')",
        1
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT option_id, option_value FROM wp_options",
            .values = first_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first fixture handle row",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT option_id, option_value FROM wp_options",
            .values = second_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second fixture handle row",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int create_fixture_schema(mylite_db *database) {
    int failures = expect_statement_ok(
        database,
        "CREATE DATABASE wp DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci"
    );

    failures += expect_statement_ok(database, "USE wp");
    failures += expect_statement_ok(database, "SET sql_mode = ''");

    return failures;
}

static int create_wordpress_fixture_tables(mylite_db *database) {
    int failures = expect_statement_result(
        database,
        "CREATE TABLE wp_users ("
        "ID bigint(20) unsigned NOT NULL auto_increment, "
        "user_login varchar(60) NOT NULL default '', "
        "user_pass varchar(255) NOT NULL default '', "
        "user_nicename varchar(50) NOT NULL default '', "
        "user_email varchar(100) NOT NULL default '', "
        "user_url varchar(100) NOT NULL default '', "
        "user_registered datetime NOT NULL default '0000-00-00 00:00:00', "
        "user_activation_key varchar(255) NOT NULL default '', "
        "user_status int(11) NOT NULL default '0', "
        "display_name varchar(250) NOT NULL default '', "
        "PRIMARY KEY (ID), "
        "KEY user_login_key (user_login), "
        "KEY user_nicename (user_nicename), "
        "KEY user_email (user_email)"
        ") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 2U}
    );
    failures += expect_statement_result(
        database,
        "CREATE TABLE wp_options ("
        "option_id bigint(20) unsigned NOT NULL auto_increment, "
        "option_name varchar(191) NOT NULL default '', "
        "option_value longtext NOT NULL, "
        "autoload varchar(20) NOT NULL default 'yes', "
        "PRIMARY KEY (option_id), "
        "UNIQUE KEY option_name (option_name), "
        "KEY autoload (autoload)"
        ") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_statement_result(
        database,
        "CREATE TABLE wp_postmeta ("
        "meta_id bigint(20) unsigned NOT NULL auto_increment, "
        "post_id bigint(20) unsigned NOT NULL default '0', "
        "meta_key varchar(255) default NULL, "
        "meta_value longtext, "
        "PRIMARY KEY (meta_id), "
        "KEY post_id (post_id), "
        "KEY meta_key (meta_key(191))"
        ") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 2U}
    );

    return failures;
}

static int verify_fixture_metadata(mylite_db *database, bool check_show_create) {
    static const char *const wp_users_columns[] = {
        "ID",
        "bigint unsigned",
        "NO",
        "PRI",
        NULL,
        "auto_increment",
        "user_login",
        "varchar(60)",
        "NO",
        "MUL",
        "",
        "",
        "user_pass",
        "varchar(255)",
        "NO",
        "",
        "",
        "",
        "user_nicename",
        "varchar(50)",
        "NO",
        "MUL",
        "",
        "",
        "user_email",
        "varchar(100)",
        "NO",
        "MUL",
        "",
        "",
        "user_url",
        "varchar(100)",
        "NO",
        "",
        "",
        "",
        "user_registered",
        "datetime",
        "NO",
        "",
        "0000-00-00 00:00:00",
        "",
        "user_activation_key",
        "varchar(255)",
        "NO",
        "",
        "",
        "",
        "user_status",
        "int",
        "NO",
        "",
        "0",
        "",
        "display_name",
        "varchar(250)",
        "NO",
        "",
        "",
        "",
    };
    static const char *const wp_users_show_create[] = {
        "wp_users",
        "CREATE TABLE `wp_users` (\n"
        "  `ID` bigint unsigned NOT NULL AUTO_INCREMENT,\n"
        "  `user_login` varchar(60) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  `user_pass` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  `user_nicename` varchar(50) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  `user_email` varchar(100) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  `user_url` varchar(100) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  `user_registered` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',\n"
        "  `user_activation_key` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL "
        "DEFAULT '',\n"
        "  `user_status` int NOT NULL DEFAULT '0',\n"
        "  `display_name` varchar(250) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  PRIMARY KEY (`ID`),\n"
        "  KEY `user_login_key` (`user_login`),\n"
        "  KEY `user_nicename` (`user_nicename`),\n"
        "  KEY `user_email` (`user_email`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci",
    };
    static const char *const wp_options_show_create[] = {
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
    static const char *const wp_postmeta_show_create[] = {
        "wp_postmeta",
        "CREATE TABLE `wp_postmeta` (\n"
        "  `meta_id` bigint unsigned NOT NULL AUTO_INCREMENT,\n"
        "  `post_id` bigint unsigned NOT NULL DEFAULT '0',\n"
        "  `meta_key` varchar(255) COLLATE utf8mb4_unicode_520_ci DEFAULT NULL,\n"
        "  `meta_value` longtext COLLATE utf8mb4_unicode_520_ci,\n"
        "  PRIMARY KEY (`meta_id`),\n"
        "  KEY `post_id` (`post_id`),\n"
        "  KEY `meta_key` (`meta_key`(191))\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci",
    };
    static const char *const wp_postmeta_show_index[] = {
        "wp_postmeta", "0", "PRIMARY",  "1",   "meta_id",
        "A",           "0", NULL,       NULL,  "",
        "BTREE",       "",  "",         "YES", NULL,
        "wp_postmeta", "1", "post_id",  "1",   "post_id",
        "A",           "0", NULL,       NULL,  "",
        "BTREE",       "",  "",         "YES", NULL,
        "wp_postmeta", "1", "meta_key", "1",   "meta_key",
        "A",           "0", "191",      NULL,  "YES",
        "BTREE",       "",  "",         "YES", NULL,
    };
    static const char *const wp_postmeta_information_schema_columns[] = {
        "meta_id",
        "bigint unsigned",
        NULL,
        NULL,
        "PRI",
        "auto_increment",
        "post_id",
        "bigint unsigned",
        NULL,
        "0",
        "MUL",
        "",
        "meta_key",
        "varchar(255)",
        "utf8mb4_unicode_520_ci",
        NULL,
        "MUL",
        "",
        "meta_value",
        "longtext",
        "utf8mb4_unicode_520_ci",
        NULL,
        "",
        "",
    };
    static const char *const primary_statistics_rows[] = {
        "PRIMARY",
        "1",
        "meta_id",
        NULL,
        "",
        "YES",
    };
    static const char *const meta_key_statistics_rows[] = {
        "meta_key",
        "1",
        "meta_key",
        "191",
        "YES",
        "YES",
    };
    static const char *const post_id_statistics_rows[] = {
        "post_id",
        "1",
        "post_id",
        NULL,
        "",
        "YES",
    };
    int failures = expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM wp_users",
            .values = wp_users_columns,
            .column_count = show_columns_column_count,
            .row_count = wp_users_column_row_count,
            .context = "wp_users columns",
        }
    );

    if (check_show_create) {
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SHOW CREATE TABLE wp_users",
                .values = wp_users_show_create,
                .column_count = show_create_column_count,
                .row_count = 1U,
                .context = "wp_users show create",
            }
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SHOW CREATE TABLE wp_options",
                .values = wp_options_show_create,
                .column_count = show_create_column_count,
                .row_count = 1U,
                .context = "wp_options show create",
            }
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SHOW CREATE TABLE wp_postmeta",
                .values = wp_postmeta_show_create,
                .column_count = show_create_column_count,
                .row_count = 1U,
                .context = "wp_postmeta show create",
            }
        );
    }
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM wp_postmeta",
            .values = wp_postmeta_show_index,
            .column_count = show_index_column_count,
            .row_count = wp_postmeta_index_row_count,
            .context = "wp_postmeta SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_TYPE, COLLATION_NAME, COLUMN_DEFAULT, "
                   "COLUMN_KEY, EXTRA FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA='wp' AND TABLE_NAME='wp_postmeta' "
                   "ORDER BY ORDINAL_POSITION",
            .values = wp_postmeta_information_schema_columns,
            .column_count = information_schema_columns_column_count,
            .row_count = wp_postmeta_column_row_count,
            .context = "wp_postmeta INFORMATION_SCHEMA.COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART, NULLABLE, "
                   "IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA='wp' "
                   "AND TABLE_NAME='wp_postmeta' AND INDEX_NAME='PRIMARY'",
            .values = primary_statistics_rows,
            .column_count = statistics_column_count,
            .row_count = 1U,
            .context = "wp_postmeta primary statistics",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART, NULLABLE, "
                   "IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA='wp' "
                   "AND TABLE_NAME='wp_postmeta' AND INDEX_NAME='meta_key'",
            .values = meta_key_statistics_rows,
            .column_count = statistics_column_count,
            .row_count = 1U,
            .context = "wp_postmeta meta_key statistics",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART, NULLABLE, "
                   "IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA='wp' "
                   "AND TABLE_NAME='wp_postmeta' AND INDEX_NAME='post_id'",
            .values = post_id_statistics_rows,
            .column_count = statistics_column_count,
            .row_count = 1U,
            .context = "wp_postmeta post_id statistics",
        }
    );

    return failures;
}

static int verify_fixture_rows(mylite_db *database, const char *context) {
    static const char *const wp_users_rows[] = {"1", "", "0000-00-00 00:00:00", "0"};
    static const char *const wp_options_rows[] = {
        "1",
        "siteurl",
        "https://example.test",
        "yes",
    };
    static const char *const wp_postmeta_rows[] = {
        "1",
        "1",
        "k",
        "v",
        "2",
        "2",
        NULL,
        NULL,
        "3",
        "0",
        "omitted",
        "default",
    };
    int failures = expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ID, user_login, user_registered, user_status FROM wp_users",
            .values = wp_users_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = context,
        }
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT option_id, option_name, option_value, autoload FROM wp_options",
            .values = wp_options_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT meta_id, post_id, meta_key, meta_value FROM wp_postmeta "
                   "ORDER BY meta_id",
            .values = wp_postmeta_rows,
            .column_count = 4U,
            .row_count = 3U,
            .context = context,
        }
    );

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int status = mylite_execute(database, sql, strlen(sql), &result);

    if (status != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }

    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }

    return 0;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return expect_statement_result(
        database,
        sql,
        (struct expected_dml_result){
            .affected_rows = 0,
            .warning_count = 0U,
        }
    );
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        "statement warning count"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    return expect_dml_result(
        database,
        sql,
        (struct expected_dml_result){
            .affected_rows = affected_rows,
            .warning_count = 0U,
        }
    );
}

static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "DML column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "DML row count");
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, "DML affected");
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, "DML warnings");
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
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
        return expect_true(actual == NULL, context);
    }

    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_wordpress_core_ddl_fixtures_%d_%s.mylite",
        directory,
        current_process_id(),
        name
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
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
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

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
        return 1;
    }

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

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text '%s', got '%s'\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
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
