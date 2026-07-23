#include "mylite_test_support.h"

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
    wp_posts_column_row_count = 23,
    wp_posts_index_row_count = 8,
    wp_posts_projection_column_count = 14,
    wp_comments_column_row_count = 15,
    wp_comments_index_row_count = 7,
    wp_comments_projection_column_count = 9,
    wp_posts_comments_create_warning_count = 5,
    wp_postmeta_column_row_count = 4,
    wp_postmeta_index_row_count = 3,
    wp_meta_create_warning_count = 2,
    wp_terms_create_warning_count = 2,
    wp_term_taxonomy_create_warning_count = 4,
    wp_term_relationships_create_warning_count = 3,
    wp_links_create_warning_count = 3,
    wp_remaining_meta_projection_column_count = 4,
    wp_terms_projection_column_count = 4,
    wp_term_taxonomy_projection_column_count = 6,
    wp_term_relationships_projection_column_count = 3,
    wp_links_projection_column_count = 7,
    wp_term_relationships_index_row_count = 3,
    wp_term_taxonomy_index_row_count = 4,
    wp_links_index_row_count = 2,
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
    static const char *const wp_posts_rows[] = {
        "1",
        "0",
        "0000-00-00 00:00:00",
        "publish",
        "open",
        "open",
        "",
        "0",
        "0",
        "post",
        "0",
        "body",
        "Title",
        "filtered",
    };
    static const char *const wp_comments_rows[] = {
        "1",
        "0",
        "Jan",
        "",
        "0000-00-00 00:00:00",
        "hello",
        "1",
        "comment",
        "0",
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
    static const char *const wp_commentmeta_rows[] = {"1", "1", "k", "v"};
    static const char *const wp_usermeta_rows[] = {"1", "2", "u", "uv"};
    static const char *const wp_termmeta_rows[] = {"1", "3", "t", "tv"};
    static const char *const wp_terms_rows[] = {"1", "", "", "0"};
    static const char *const wp_term_taxonomy_rows[] = {
        "1",
        "1",
        "category",
        "desc",
        "0",
        "0",
    };
    static const char *const wp_term_relationships_rows[] = {"10", "1", "0"};
    static const char *const wp_links_rows[] = {
        "1",
        "",
        "Y",
        "1",
        "0",
        "0000-00-00 00:00:00",
        "notes",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "setup") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open fixture file");
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
        "INSERT INTO wp_posts (post_content, post_title, post_excerpt, to_ping, pinged, "
        "post_content_filtered) VALUES ('body', 'Title', '', '', '', 'filtered')",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ID, post_author, post_date, post_status, comment_status, "
                   "ping_status, post_name, post_parent, menu_order, post_type, "
                   "comment_count, post_content, post_title, post_content_filtered "
                   "FROM wp_posts",
            .values = wp_posts_rows,
            .column_count = wp_posts_projection_column_count,
            .row_count = 1U,
            .context = "wp_posts inserted row",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO wp_comments (comment_author, comment_content) VALUES ('Jan', 'hello')",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT comment_ID, comment_post_ID, comment_author, "
                   "comment_author_email, comment_date, comment_content, "
                   "comment_approved, comment_type, user_id FROM wp_comments",
            .values = wp_comments_rows,
            .column_count = wp_comments_projection_column_count,
            .row_count = 1U,
            .context = "wp_comments inserted row",
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
    failures += expect_dml_ok(
        database,
        "INSERT INTO wp_commentmeta (comment_id, meta_key, meta_value) "
        "VALUES (1, 'k', 'v')",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT meta_id, comment_id, meta_key, meta_value FROM wp_commentmeta",
            .values = wp_commentmeta_rows,
            .column_count = wp_remaining_meta_projection_column_count,
            .row_count = 1U,
            .context = "wp_commentmeta inserted row",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO wp_usermeta (user_id, meta_key, meta_value) VALUES (2, 'u', 'uv')",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT umeta_id, user_id, meta_key, meta_value FROM wp_usermeta",
            .values = wp_usermeta_rows,
            .column_count = wp_remaining_meta_projection_column_count,
            .row_count = 1U,
            .context = "wp_usermeta inserted row",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO wp_termmeta (term_id, meta_key, meta_value) VALUES (3, 't', 'tv')",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT meta_id, term_id, meta_key, meta_value FROM wp_termmeta",
            .values = wp_termmeta_rows,
            .column_count = wp_remaining_meta_projection_column_count,
            .row_count = 1U,
            .context = "wp_termmeta inserted row",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO wp_terms () VALUES ()", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT term_id, name, slug, term_group FROM wp_terms",
            .values = wp_terms_rows,
            .column_count = wp_terms_projection_column_count,
            .row_count = 1U,
            .context = "wp_terms inserted row",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO wp_term_taxonomy (term_id, taxonomy, description) "
        "VALUES (1, 'category', 'desc')",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT term_taxonomy_id, term_id, taxonomy, description, parent, "
                   "count FROM wp_term_taxonomy",
            .values = wp_term_taxonomy_rows,
            .column_count = wp_term_taxonomy_projection_column_count,
            .row_count = 1U,
            .context = "wp_term_taxonomy inserted row",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO wp_term_relationships (object_id, term_taxonomy_id) VALUES (10, 1)",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT object_id, term_taxonomy_id, term_order "
                   "FROM wp_term_relationships",
            .values = wp_term_relationships_rows,
            .column_count = wp_term_relationships_projection_column_count,
            .row_count = 1U,
            .context = "wp_term_relationships inserted row",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO wp_links (link_notes) VALUES ('notes')", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT link_id, link_url, link_visible, link_owner, link_rating, "
                   "link_updated, link_notes FROM wp_links",
            .values = wp_links_rows,
            .column_count = wp_links_projection_column_count,
            .row_count = 1U,
            .context = "wp_links inserted row",
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

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen fixture file");
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

    if (mylite_test_make_path(first_path, sizeof(first_path), "first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += mylite_test_expect_int(
        mylite_open(first_path, &first),
        MYLITE_OK,
        "open first fixture file"
    );
    failures += mylite_test_expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second fixture file"
    );
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
        "CREATE TABLE wp_posts ("
        "ID bigint(20) unsigned NOT NULL auto_increment, "
        "post_author bigint(20) unsigned NOT NULL default '0', "
        "post_date datetime NOT NULL default '0000-00-00 00:00:00', "
        "post_date_gmt datetime NOT NULL default '0000-00-00 00:00:00', "
        "post_content longtext NOT NULL, "
        "post_title text NOT NULL, "
        "post_excerpt text NOT NULL, "
        "post_status varchar(20) NOT NULL default 'publish', "
        "comment_status varchar(20) NOT NULL default 'open', "
        "ping_status varchar(20) NOT NULL default 'open', "
        "post_password varchar(255) NOT NULL default '', "
        "post_name varchar(200) NOT NULL default '', "
        "to_ping text NOT NULL, "
        "pinged text NOT NULL, "
        "post_modified datetime NOT NULL default '0000-00-00 00:00:00', "
        "post_modified_gmt datetime NOT NULL default '0000-00-00 00:00:00', "
        "post_content_filtered longtext NOT NULL, "
        "post_parent bigint(20) unsigned NOT NULL default '0', "
        "guid varchar(255) NOT NULL default '', "
        "menu_order int(11) NOT NULL default '0', "
        "post_type varchar(20) NOT NULL default 'post', "
        "post_mime_type varchar(100) NOT NULL default '', "
        "comment_count bigint(20) NOT NULL default '0', "
        "PRIMARY KEY (ID), "
        "KEY post_name (post_name(191)), "
        "KEY type_status_date (post_type, post_status, post_date, ID), "
        "KEY post_parent (post_parent), "
        "KEY post_author (post_author)"
        ") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = wp_posts_comments_create_warning_count,
        }
    );
    failures += expect_statement_result(
        database,
        "CREATE TABLE wp_comments ("
        "comment_ID bigint(20) unsigned NOT NULL auto_increment, "
        "comment_post_ID bigint(20) unsigned NOT NULL default '0', "
        "comment_author tinytext NOT NULL, "
        "comment_author_email varchar(100) NOT NULL default '', "
        "comment_author_url varchar(200) NOT NULL default '', "
        "comment_author_IP varchar(100) NOT NULL default '', "
        "comment_date datetime NOT NULL default '0000-00-00 00:00:00', "
        "comment_date_gmt datetime NOT NULL default '0000-00-00 00:00:00', "
        "comment_content text NOT NULL, "
        "comment_karma int(11) NOT NULL default '0', "
        "comment_approved varchar(20) NOT NULL default '1', "
        "comment_agent varchar(255) NOT NULL default '', "
        "comment_type varchar(20) NOT NULL default 'comment', "
        "comment_parent bigint(20) unsigned NOT NULL default '0', "
        "user_id bigint(20) unsigned NOT NULL default '0', "
        "PRIMARY KEY (comment_ID), "
        "KEY comment_post_ID (comment_post_ID), "
        "KEY comment_approved_date_gmt (comment_approved, comment_date_gmt), "
        "KEY comment_date_gmt (comment_date_gmt), "
        "KEY comment_parent (comment_parent), "
        "KEY comment_author_email (comment_author_email(10))"
        ") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = wp_posts_comments_create_warning_count,
        }
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
    failures += expect_statement_result(
        database,
        "CREATE TABLE wp_commentmeta ("
        "meta_id bigint(20) unsigned NOT NULL auto_increment, "
        "comment_id bigint(20) unsigned NOT NULL default '0', "
        "meta_key varchar(255) default NULL, "
        "meta_value longtext, "
        "PRIMARY KEY (meta_id), "
        "KEY comment_id (comment_id), "
        "KEY meta_key (meta_key(191))"
        ") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = wp_meta_create_warning_count,
        }
    );
    failures += expect_statement_result(
        database,
        "CREATE TABLE wp_usermeta ("
        "umeta_id bigint(20) unsigned NOT NULL auto_increment, "
        "user_id bigint(20) unsigned NOT NULL default '0', "
        "meta_key varchar(255) default NULL, "
        "meta_value longtext, "
        "PRIMARY KEY (umeta_id), "
        "KEY user_id (user_id), "
        "KEY meta_key (meta_key(191))"
        ") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = wp_meta_create_warning_count,
        }
    );
    failures += expect_statement_result(
        database,
        "CREATE TABLE wp_termmeta ("
        "meta_id bigint(20) unsigned NOT NULL auto_increment, "
        "term_id bigint(20) unsigned NOT NULL default '0', "
        "meta_key varchar(255) default NULL, "
        "meta_value longtext, "
        "PRIMARY KEY (meta_id), "
        "KEY term_id (term_id), "
        "KEY meta_key (meta_key(191))"
        ") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = wp_meta_create_warning_count,
        }
    );
    failures += expect_statement_result(
        database,
        "CREATE TABLE wp_terms ("
        "term_id bigint(20) unsigned NOT NULL auto_increment, "
        "name varchar(200) NOT NULL default '', "
        "slug varchar(200) NOT NULL default '', "
        "term_group bigint(10) NOT NULL default 0, "
        "PRIMARY KEY (term_id), "
        "KEY slug (slug(191)), "
        "KEY name (name(191))"
        ") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = wp_terms_create_warning_count,
        }
    );
    failures += expect_statement_result(
        database,
        "CREATE TABLE wp_term_taxonomy ("
        "term_taxonomy_id bigint(20) unsigned NOT NULL auto_increment, "
        "term_id bigint(20) unsigned NOT NULL default 0, "
        "taxonomy varchar(32) NOT NULL default '', "
        "description longtext NOT NULL, "
        "parent bigint(20) unsigned NOT NULL default 0, "
        "count bigint(20) NOT NULL default 0, "
        "PRIMARY KEY (term_taxonomy_id), "
        "UNIQUE KEY term_id_taxonomy (term_id, taxonomy), "
        "KEY taxonomy (taxonomy)"
        ") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = wp_term_taxonomy_create_warning_count,
        }
    );
    failures += expect_statement_result(
        database,
        "CREATE TABLE wp_term_relationships ("
        "object_id bigint(20) unsigned NOT NULL default 0, "
        "term_taxonomy_id bigint(20) unsigned NOT NULL default 0, "
        "term_order int(11) NOT NULL default 0, "
        "PRIMARY KEY (object_id, term_taxonomy_id), "
        "KEY term_taxonomy_id (term_taxonomy_id)"
        ") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = wp_term_relationships_create_warning_count,
        }
    );
    failures += expect_statement_result(
        database,
        "CREATE TABLE wp_links ("
        "link_id bigint(20) unsigned NOT NULL auto_increment, "
        "link_url varchar(255) NOT NULL default '', "
        "link_name varchar(255) NOT NULL default '', "
        "link_image varchar(255) NOT NULL default '', "
        "link_target varchar(25) NOT NULL default '', "
        "link_description varchar(255) NOT NULL default '', "
        "link_visible varchar(20) NOT NULL default 'Y', "
        "link_owner bigint(20) unsigned NOT NULL default '1', "
        "link_rating int(11) NOT NULL default '0', "
        "link_updated datetime NOT NULL default '0000-00-00 00:00:00', "
        "link_rel varchar(255) NOT NULL default '', "
        "link_notes mediumtext NOT NULL, "
        "link_rss varchar(255) NOT NULL default '', "
        "PRIMARY KEY (link_id), "
        "KEY link_visible (link_visible)"
        ") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = wp_links_create_warning_count,
        }
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
    static const char *const wp_posts_columns[] = {
        "ID",
        "bigint unsigned",
        "NO",
        "PRI",
        NULL,
        "auto_increment",
        "post_author",
        "bigint unsigned",
        "NO",
        "MUL",
        "0",
        "",
        "post_date",
        "datetime",
        "NO",
        "",
        "0000-00-00 00:00:00",
        "",
        "post_date_gmt",
        "datetime",
        "NO",
        "",
        "0000-00-00 00:00:00",
        "",
        "post_content",
        "longtext",
        "NO",
        "",
        NULL,
        "",
        "post_title",
        "text",
        "NO",
        "",
        NULL,
        "",
        "post_excerpt",
        "text",
        "NO",
        "",
        NULL,
        "",
        "post_status",
        "varchar(20)",
        "NO",
        "",
        "publish",
        "",
        "comment_status",
        "varchar(20)",
        "NO",
        "",
        "open",
        "",
        "ping_status",
        "varchar(20)",
        "NO",
        "",
        "open",
        "",
        "post_password",
        "varchar(255)",
        "NO",
        "",
        "",
        "",
        "post_name",
        "varchar(200)",
        "NO",
        "MUL",
        "",
        "",
        "to_ping",
        "text",
        "NO",
        "",
        NULL,
        "",
        "pinged",
        "text",
        "NO",
        "",
        NULL,
        "",
        "post_modified",
        "datetime",
        "NO",
        "",
        "0000-00-00 00:00:00",
        "",
        "post_modified_gmt",
        "datetime",
        "NO",
        "",
        "0000-00-00 00:00:00",
        "",
        "post_content_filtered",
        "longtext",
        "NO",
        "",
        NULL,
        "",
        "post_parent",
        "bigint unsigned",
        "NO",
        "MUL",
        "0",
        "",
        "guid",
        "varchar(255)",
        "NO",
        "",
        "",
        "",
        "menu_order",
        "int",
        "NO",
        "",
        "0",
        "",
        "post_type",
        "varchar(20)",
        "NO",
        "MUL",
        "post",
        "",
        "post_mime_type",
        "varchar(100)",
        "NO",
        "",
        "",
        "",
        "comment_count",
        "bigint",
        "NO",
        "",
        "0",
        "",
    };
    static const char *const wp_comments_columns[] = {
        "comment_ID",
        "bigint unsigned",
        "NO",
        "PRI",
        NULL,
        "auto_increment",
        "comment_post_ID",
        "bigint unsigned",
        "NO",
        "MUL",
        "0",
        "",
        "comment_author",
        "tinytext",
        "NO",
        "",
        NULL,
        "",
        "comment_author_email",
        "varchar(100)",
        "NO",
        "MUL",
        "",
        "",
        "comment_author_url",
        "varchar(200)",
        "NO",
        "",
        "",
        "",
        "comment_author_IP",
        "varchar(100)",
        "NO",
        "",
        "",
        "",
        "comment_date",
        "datetime",
        "NO",
        "",
        "0000-00-00 00:00:00",
        "",
        "comment_date_gmt",
        "datetime",
        "NO",
        "MUL",
        "0000-00-00 00:00:00",
        "",
        "comment_content",
        "text",
        "NO",
        "",
        NULL,
        "",
        "comment_karma",
        "int",
        "NO",
        "",
        "0",
        "",
        "comment_approved",
        "varchar(20)",
        "NO",
        "MUL",
        "1",
        "",
        "comment_agent",
        "varchar(255)",
        "NO",
        "",
        "",
        "",
        "comment_type",
        "varchar(20)",
        "NO",
        "",
        "comment",
        "",
        "comment_parent",
        "bigint unsigned",
        "NO",
        "MUL",
        "0",
        "",
        "user_id",
        "bigint unsigned",
        "NO",
        "",
        "0",
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
    static const char *const wp_posts_show_create[] = {
        "wp_posts",
        "CREATE TABLE `wp_posts` (\n"
        "  `ID` bigint unsigned NOT NULL AUTO_INCREMENT,\n"
        "  `post_author` bigint unsigned NOT NULL DEFAULT '0',\n"
        "  `post_date` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',\n"
        "  `post_date_gmt` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',\n"
        "  `post_content` longtext COLLATE utf8mb4_unicode_520_ci NOT NULL,\n"
        "  `post_title` text COLLATE utf8mb4_unicode_520_ci NOT NULL,\n"
        "  `post_excerpt` text COLLATE utf8mb4_unicode_520_ci NOT NULL,\n"
        "  `post_status` varchar(20) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT "
        "'publish',\n"
        "  `comment_status` varchar(20) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT "
        "'open',\n"
        "  `ping_status` varchar(20) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT "
        "'open',\n"
        "  `post_password` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT "
        "'',\n"
        "  `post_name` varchar(200) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  `to_ping` text COLLATE utf8mb4_unicode_520_ci NOT NULL,\n"
        "  `pinged` text COLLATE utf8mb4_unicode_520_ci NOT NULL,\n"
        "  `post_modified` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',\n"
        "  `post_modified_gmt` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',\n"
        "  `post_content_filtered` longtext COLLATE utf8mb4_unicode_520_ci NOT NULL,\n"
        "  `post_parent` bigint unsigned NOT NULL DEFAULT '0',\n"
        "  `guid` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  `menu_order` int NOT NULL DEFAULT '0',\n"
        "  `post_type` varchar(20) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT 'post',\n"
        "  `post_mime_type` varchar(100) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT "
        "'',\n"
        "  `comment_count` bigint NOT NULL DEFAULT '0',\n"
        "  PRIMARY KEY (`ID`),\n"
        "  KEY `post_name` (`post_name`(191)),\n"
        "  KEY `type_status_date` (`post_type`,`post_status`,`post_date`,`ID`),\n"
        "  KEY `post_parent` (`post_parent`),\n"
        "  KEY `post_author` (`post_author`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci",
    };
    static const char *const wp_comments_show_create[] = {
        "wp_comments",
        "CREATE TABLE `wp_comments` (\n"
        "  `comment_ID` bigint unsigned NOT NULL AUTO_INCREMENT,\n"
        "  `comment_post_ID` bigint unsigned NOT NULL DEFAULT '0',\n"
        "  `comment_author` tinytext COLLATE utf8mb4_unicode_520_ci NOT NULL,\n"
        "  `comment_author_email` varchar(100) COLLATE utf8mb4_unicode_520_ci NOT NULL "
        "DEFAULT '',\n"
        "  `comment_author_url` varchar(200) COLLATE utf8mb4_unicode_520_ci NOT NULL "
        "DEFAULT '',\n"
        "  `comment_author_IP` varchar(100) COLLATE utf8mb4_unicode_520_ci NOT NULL "
        "DEFAULT '',\n"
        "  `comment_date` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',\n"
        "  `comment_date_gmt` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',\n"
        "  `comment_content` text COLLATE utf8mb4_unicode_520_ci NOT NULL,\n"
        "  `comment_karma` int NOT NULL DEFAULT '0',\n"
        "  `comment_approved` varchar(20) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT "
        "'1',\n"
        "  `comment_agent` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT "
        "'',\n"
        "  `comment_type` varchar(20) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT "
        "'comment',\n"
        "  `comment_parent` bigint unsigned NOT NULL DEFAULT '0',\n"
        "  `user_id` bigint unsigned NOT NULL DEFAULT '0',\n"
        "  PRIMARY KEY (`comment_ID`),\n"
        "  KEY `comment_post_ID` (`comment_post_ID`),\n"
        "  KEY `comment_approved_date_gmt` (`comment_approved`,`comment_date_gmt`),\n"
        "  KEY `comment_date_gmt` (`comment_date_gmt`),\n"
        "  KEY `comment_parent` (`comment_parent`),\n"
        "  KEY `comment_author_email` (`comment_author_email`(10))\n"
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
    static const char *const wp_commentmeta_show_create[] = {
        "wp_commentmeta",
        "CREATE TABLE `wp_commentmeta` (\n"
        "  `meta_id` bigint unsigned NOT NULL AUTO_INCREMENT,\n"
        "  `comment_id` bigint unsigned NOT NULL DEFAULT '0',\n"
        "  `meta_key` varchar(255) COLLATE utf8mb4_unicode_520_ci DEFAULT NULL,\n"
        "  `meta_value` longtext COLLATE utf8mb4_unicode_520_ci,\n"
        "  PRIMARY KEY (`meta_id`),\n"
        "  KEY `comment_id` (`comment_id`),\n"
        "  KEY `meta_key` (`meta_key`(191))\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci",
    };
    static const char *const wp_terms_show_create[] = {
        "wp_terms",
        "CREATE TABLE `wp_terms` (\n"
        "  `term_id` bigint unsigned NOT NULL AUTO_INCREMENT,\n"
        "  `name` varchar(200) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  `slug` varchar(200) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  `term_group` bigint NOT NULL DEFAULT '0',\n"
        "  PRIMARY KEY (`term_id`),\n"
        "  KEY `slug` (`slug`(191)),\n"
        "  KEY `name` (`name`(191))\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci",
    };
    static const char *const wp_term_taxonomy_show_create[] = {
        "wp_term_taxonomy",
        "CREATE TABLE `wp_term_taxonomy` (\n"
        "  `term_taxonomy_id` bigint unsigned NOT NULL AUTO_INCREMENT,\n"
        "  `term_id` bigint unsigned NOT NULL DEFAULT '0',\n"
        "  `taxonomy` varchar(32) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  `description` longtext COLLATE utf8mb4_unicode_520_ci NOT NULL,\n"
        "  `parent` bigint unsigned NOT NULL DEFAULT '0',\n"
        "  `count` bigint NOT NULL DEFAULT '0',\n"
        "  PRIMARY KEY (`term_taxonomy_id`),\n"
        "  UNIQUE KEY `term_id_taxonomy` (`term_id`,`taxonomy`),\n"
        "  KEY `taxonomy` (`taxonomy`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci",
    };
    static const char *const wp_term_relationships_show_create[] = {
        "wp_term_relationships",
        "CREATE TABLE `wp_term_relationships` (\n"
        "  `object_id` bigint unsigned NOT NULL DEFAULT '0',\n"
        "  `term_taxonomy_id` bigint unsigned NOT NULL DEFAULT '0',\n"
        "  `term_order` int NOT NULL DEFAULT '0',\n"
        "  PRIMARY KEY (`object_id`,`term_taxonomy_id`),\n"
        "  KEY `term_taxonomy_id` (`term_taxonomy_id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci",
    };
    static const char *const wp_links_show_create[] = {
        "wp_links",
        "CREATE TABLE `wp_links` (\n"
        "  `link_id` bigint unsigned NOT NULL AUTO_INCREMENT,\n"
        "  `link_url` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  `link_name` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  `link_image` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  `link_target` varchar(25) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  `link_description` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL "
        "DEFAULT '',\n"
        "  `link_visible` varchar(20) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT "
        "'Y',\n"
        "  `link_owner` bigint unsigned NOT NULL DEFAULT '1',\n"
        "  `link_rating` int NOT NULL DEFAULT '0',\n"
        "  `link_updated` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',\n"
        "  `link_rel` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  `link_notes` mediumtext COLLATE utf8mb4_unicode_520_ci NOT NULL,\n"
        "  `link_rss` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',\n"
        "  PRIMARY KEY (`link_id`),\n"
        "  KEY `link_visible` (`link_visible`)\n"
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
    static const char *const wp_posts_show_index[] = {
        "wp_posts", "0",           "PRIMARY",
        "1",        "ID",          "A",
        "0",        NULL,          NULL,
        "",         "BTREE",       "",
        "",         "YES",         NULL,
        "wp_posts", "1",           "post_name",
        "1",        "post_name",   "A",
        "0",        "191",         NULL,
        "",         "BTREE",       "",
        "",         "YES",         NULL,
        "wp_posts", "1",           "type_status_date",
        "1",        "post_type",   "A",
        "0",        NULL,          NULL,
        "",         "BTREE",       "",
        "",         "YES",         NULL,
        "wp_posts", "1",           "type_status_date",
        "2",        "post_status", "A",
        "0",        NULL,          NULL,
        "",         "BTREE",       "",
        "",         "YES",         NULL,
        "wp_posts", "1",           "type_status_date",
        "3",        "post_date",   "A",
        "0",        NULL,          NULL,
        "",         "BTREE",       "",
        "",         "YES",         NULL,
        "wp_posts", "1",           "type_status_date",
        "4",        "ID",          "A",
        "0",        NULL,          NULL,
        "",         "BTREE",       "",
        "",         "YES",         NULL,
        "wp_posts", "1",           "post_parent",
        "1",        "post_parent", "A",
        "0",        NULL,          NULL,
        "",         "BTREE",       "",
        "",         "YES",         NULL,
        "wp_posts", "1",           "post_author",
        "1",        "post_author", "A",
        "0",        NULL,          NULL,
        "",         "BTREE",       "",
        "",         "YES",         NULL,
    };
    static const char *const wp_comments_show_index[] = {
        "wp_comments",
        "0",
        "PRIMARY",
        "1",
        "comment_ID",
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
        "wp_comments",
        "1",
        "comment_post_ID",
        "1",
        "comment_post_ID",
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
        "wp_comments",
        "1",
        "comment_approved_date_gmt",
        "1",
        "comment_approved",
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
        "wp_comments",
        "1",
        "comment_approved_date_gmt",
        "2",
        "comment_date_gmt",
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
        "wp_comments",
        "1",
        "comment_date_gmt",
        "1",
        "comment_date_gmt",
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
        "wp_comments",
        "1",
        "comment_parent",
        "1",
        "comment_parent",
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
        "wp_comments",
        "1",
        "comment_author_email",
        "1",
        "comment_author_email",
        "A",
        "0",
        "10",
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const wp_commentmeta_show_index[] = {
        "wp_commentmeta",
        "0",
        "PRIMARY",
        "1",
        "meta_id",
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
        "wp_commentmeta",
        "1",
        "comment_id",
        "1",
        "comment_id",
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
        "wp_commentmeta",
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
    static const char *const wp_term_taxonomy_show_index[] = {
        "wp_term_taxonomy",
        "0",
        "PRIMARY",
        "1",
        "term_taxonomy_id",
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
        "wp_term_taxonomy",
        "0",
        "term_id_taxonomy",
        "1",
        "term_id",
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
        "wp_term_taxonomy",
        "0",
        "term_id_taxonomy",
        "2",
        "taxonomy",
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
        "wp_term_taxonomy",
        "1",
        "taxonomy",
        "1",
        "taxonomy",
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
    static const char *const wp_term_relationships_show_index[] = {
        "wp_term_relationships",
        "0",
        "PRIMARY",
        "1",
        "object_id",
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
        "wp_term_relationships",
        "0",
        "PRIMARY",
        "2",
        "term_taxonomy_id",
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
        "wp_term_relationships",
        "1",
        "term_taxonomy_id",
        "1",
        "term_taxonomy_id",
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
    static const char *const wp_links_show_index[] = {
        "wp_links", "0",
        "PRIMARY",  "1",
        "link_id",  "A",
        "0",        NULL,
        NULL,       "",
        "BTREE",    "",
        "",         "YES",
        NULL,       "wp_links",
        "1",        "link_visible",
        "1",        "link_visible",
        "A",        "0",
        NULL,       NULL,
        "",         "BTREE",
        "",         "",
        "YES",      NULL,
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
    static const char *const wp_posts_information_schema_post_content[] = {
        "post_content",
        "longtext",
        "utf8mb4_unicode_520_ci",
        NULL,
        "",
        "",
    };
    static const char *const wp_comments_information_schema_author[] = {
        "comment_author",
        "tinytext",
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
    static const char *const type_status_date_statistics_rows[] = {
        "type_status_date", "1", "post_type",   NULL, "", "YES",
        "type_status_date", "2", "post_status", NULL, "", "YES",
        "type_status_date", "3", "post_date",   NULL, "", "YES",
        "type_status_date", "4", "ID",          NULL, "", "YES",
    };
    static const char *const comment_author_email_statistics_rows[] = {
        "comment_author_email",
        "1",
        "comment_author_email",
        "10",
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
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM wp_posts",
            .values = wp_posts_columns,
            .column_count = show_columns_column_count,
            .row_count = wp_posts_column_row_count,
            .context = "wp_posts columns",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM wp_comments",
            .values = wp_comments_columns,
            .column_count = show_columns_column_count,
            .row_count = wp_comments_column_row_count,
            .context = "wp_comments columns",
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
                .sql = "SHOW CREATE TABLE wp_posts",
                .values = wp_posts_show_create,
                .column_count = show_create_column_count,
                .row_count = 1U,
                .context = "wp_posts show create",
            }
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SHOW CREATE TABLE wp_comments",
                .values = wp_comments_show_create,
                .column_count = show_create_column_count,
                .row_count = 1U,
                .context = "wp_comments show create",
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
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SHOW CREATE TABLE wp_commentmeta",
                .values = wp_commentmeta_show_create,
                .column_count = show_create_column_count,
                .row_count = 1U,
                .context = "wp_commentmeta show create",
            }
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SHOW CREATE TABLE wp_terms",
                .values = wp_terms_show_create,
                .column_count = show_create_column_count,
                .row_count = 1U,
                .context = "wp_terms show create",
            }
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SHOW CREATE TABLE wp_term_taxonomy",
                .values = wp_term_taxonomy_show_create,
                .column_count = show_create_column_count,
                .row_count = 1U,
                .context = "wp_term_taxonomy show create",
            }
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SHOW CREATE TABLE wp_term_relationships",
                .values = wp_term_relationships_show_create,
                .column_count = show_create_column_count,
                .row_count = 1U,
                .context = "wp_term_relationships show create",
            }
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SHOW CREATE TABLE wp_links",
                .values = wp_links_show_create,
                .column_count = show_create_column_count,
                .row_count = 1U,
                .context = "wp_links show create",
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
            .sql = "SHOW INDEX FROM wp_posts",
            .values = wp_posts_show_index,
            .column_count = show_index_column_count,
            .row_count = wp_posts_index_row_count,
            .context = "wp_posts SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM wp_comments",
            .values = wp_comments_show_index,
            .column_count = show_index_column_count,
            .row_count = wp_comments_index_row_count,
            .context = "wp_comments SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM wp_commentmeta",
            .values = wp_commentmeta_show_index,
            .column_count = show_index_column_count,
            .row_count = wp_postmeta_index_row_count,
            .context = "wp_commentmeta SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM wp_term_taxonomy",
            .values = wp_term_taxonomy_show_index,
            .column_count = show_index_column_count,
            .row_count = wp_term_taxonomy_index_row_count,
            .context = "wp_term_taxonomy SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM wp_term_relationships",
            .values = wp_term_relationships_show_index,
            .column_count = show_index_column_count,
            .row_count = wp_term_relationships_index_row_count,
            .context = "wp_term_relationships SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM wp_links",
            .values = wp_links_show_index,
            .column_count = show_index_column_count,
            .row_count = wp_links_index_row_count,
            .context = "wp_links SHOW INDEX",
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
            .sql = "SELECT COLUMN_NAME, COLUMN_TYPE, COLLATION_NAME, COLUMN_DEFAULT, "
                   "COLUMN_KEY, EXTRA FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA='wp' AND TABLE_NAME='wp_posts' "
                   "AND COLUMN_NAME='post_content'",
            .values = wp_posts_information_schema_post_content,
            .column_count = information_schema_columns_column_count,
            .row_count = 1U,
            .context = "wp_posts INFORMATION_SCHEMA.COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_TYPE, COLLATION_NAME, COLUMN_DEFAULT, "
                   "COLUMN_KEY, EXTRA FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA='wp' AND TABLE_NAME='wp_comments' "
                   "AND COLUMN_NAME='comment_author'",
            .values = wp_comments_information_schema_author,
            .column_count = information_schema_columns_column_count,
            .row_count = 1U,
            .context = "wp_comments INFORMATION_SCHEMA.COLUMNS",
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
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART, NULLABLE, "
                   "IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA='wp' "
                   "AND TABLE_NAME='wp_posts' AND INDEX_NAME='type_status_date'",
            .values = type_status_date_statistics_rows,
            .column_count = statistics_column_count,
            .row_count = 4U,
            .context = "wp_posts type_status_date statistics",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART, NULLABLE, "
                   "IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA='wp' "
                   "AND TABLE_NAME='wp_comments' AND INDEX_NAME='comment_author_email'",
            .values = comment_author_email_statistics_rows,
            .column_count = statistics_column_count,
            .row_count = 1U,
            .context = "wp_comments comment_author_email statistics",
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
    static const char *const wp_posts_rows[] = {
        "1",
        "0",
        "0000-00-00 00:00:00",
        "publish",
        "open",
        "open",
        "",
        "0",
        "0",
        "post",
        "0",
        "body",
        "Title",
        "filtered",
    };
    static const char *const wp_comments_rows[] = {
        "1",
        "0",
        "Jan",
        "",
        "0000-00-00 00:00:00",
        "hello",
        "1",
        "comment",
        "0",
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
    static const char *const wp_commentmeta_rows[] = {"1", "1", "k", "v"};
    static const char *const wp_usermeta_rows[] = {"1", "2", "u", "uv"};
    static const char *const wp_termmeta_rows[] = {"1", "3", "t", "tv"};
    static const char *const wp_terms_rows[] = {"1", "", "", "0"};
    static const char *const wp_term_taxonomy_rows[] = {
        "1",
        "1",
        "category",
        "desc",
        "0",
        "0",
    };
    static const char *const wp_term_relationships_rows[] = {"10", "1", "0"};
    static const char *const wp_links_rows[] = {
        "1",
        "",
        "Y",
        "1",
        "0",
        "0000-00-00 00:00:00",
        "notes",
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
            .sql = "SELECT ID, post_author, post_date, post_status, comment_status, "
                   "ping_status, post_name, post_parent, menu_order, post_type, "
                   "comment_count, post_content, post_title, post_content_filtered "
                   "FROM wp_posts",
            .values = wp_posts_rows,
            .column_count = wp_posts_projection_column_count,
            .row_count = 1U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT comment_ID, comment_post_ID, comment_author, "
                   "comment_author_email, comment_date, comment_content, "
                   "comment_approved, comment_type, user_id FROM wp_comments",
            .values = wp_comments_rows,
            .column_count = wp_comments_projection_column_count,
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
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT meta_id, comment_id, meta_key, meta_value FROM wp_commentmeta",
            .values = wp_commentmeta_rows,
            .column_count = wp_remaining_meta_projection_column_count,
            .row_count = 1U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT umeta_id, user_id, meta_key, meta_value FROM wp_usermeta",
            .values = wp_usermeta_rows,
            .column_count = wp_remaining_meta_projection_column_count,
            .row_count = 1U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT meta_id, term_id, meta_key, meta_value FROM wp_termmeta",
            .values = wp_termmeta_rows,
            .column_count = wp_remaining_meta_projection_column_count,
            .row_count = 1U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT term_id, name, slug, term_group FROM wp_terms",
            .values = wp_terms_rows,
            .column_count = wp_terms_projection_column_count,
            .row_count = 1U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT term_taxonomy_id, term_id, taxonomy, description, parent, "
                   "count FROM wp_term_taxonomy",
            .values = wp_term_taxonomy_rows,
            .column_count = wp_term_taxonomy_projection_column_count,
            .row_count = 1U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT object_id, term_taxonomy_id, term_order "
                   "FROM wp_term_relationships",
            .values = wp_term_relationships_rows,
            .column_count = wp_term_relationships_projection_column_count,
            .row_count = 1U,
            .context = context,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT link_id, link_url, link_visible, link_owner, link_rating, "
                   "link_updated, link_notes FROM wp_links",
            .values = wp_links_rows,
            .column_count = wp_links_projection_column_count,
            .row_count = 1U,
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

    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += mylite_test_expect_size(
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

    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, "DML column count");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "DML row count");
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        expected.affected_rows,
        "DML affected"
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        "DML warnings"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, query.context);
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
        return mylite_test_expect_true(actual == NULL, context);
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
