#include <mylite/mylite.h>
#ifdef MYLITE_ENABLE_PROFILING
#  include "runtime/mylite_profile_internal.h"
#endif

#include "mylite_benchmark_csv.h"
#include "mylite_benchmark_parse_expectations.h"
#include "mylite_benchmark_runtime_stress.h"
#include "mylite_benchmark_sql_mode.h"
#include "sql/mylite_lexer.h"
#include "sql/mylite_parser.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <time.h>
#  include <unistd.h>
#endif

enum {
    default_iterations = 1000,
    default_csv_iterations = 1,
    default_samples = 1,
    default_runtime_warmup_iterations = 1,
    runtime_database_path_capacity = 1024,
    runtime_database_suffix_capacity = 16,
    runtime_query_scenario_name_capacity = 256,
    decimal_option_base = 10,
    nanoseconds_per_second = 1000000000ULL,
    milliseconds_per_second = 1000,
    microseconds_per_second = 1000000,
    nanoseconds_per_microsecond = nanoseconds_per_second / microseconds_per_second,
    parse_status_bucket_count = MYLITE_SQL_PARSE_STACK_OVERFLOW + 1,
    percentile_median = 50,
    percentile_p95 = 95,
    percentile_p99 = 99,
    percentile_max = 100,
    percentile_rounding_offset = percentile_max - 1,
};

static const double percentile_pair_average_divisor = 2.0;

enum benchmark_filter {
    benchmark_filter_all,
    benchmark_filter_lexer,
    benchmark_filter_parse,
    benchmark_filter_runtime,
};

struct benchmark_options {
    enum benchmark_filter filter;
    const char *csv_path;
    const char *parse_failure_dump_path;
    const char *expected_parse_failures_path;
    const char *profile_json_path;
    const char *runtime_scenario_name;
    size_t iterations;
    size_t csv_iterations;
    size_t samples;
    size_t warmup_iterations;
    bool csv_replay_sql_mode;
    bool runtime_per_query;
    bool list_only;
    bool show_usage;
};

struct runtime_repetition_options {
    size_t iterations;
    size_t samples;
    size_t warmup_iterations;
    const char *profile_json_path;
};

struct benchmark_query {
    const char *sql;
    size_t length;
};

struct borrowed_query_list {
    const struct benchmark_query *items;
    size_t count;
};

struct benchmark_measurement {
    uint64_t elapsed_ns;
    uint64_t *request_latency_ns;
    size_t operations;
    size_t bytes;
    size_t ok_count;
    size_t error_count;
    size_t token_count;
    size_t request_latency_count;
    size_t request_latency_capacity;
    size_t parse_status_counts[parse_status_bucket_count];
#ifdef MYLITE_ENABLE_PROFILING
    struct mylite_profile_snapshot profile;
#endif
};

struct text_coordinates {
    unsigned int line;
    unsigned int column;
};

struct expected_parse_failure_summary {
    size_t total_count;
    size_t matched_count;
    size_t unexpected_count;
    size_t mismatched_count;
    size_t missing_count;
};

struct runtime_scenario {
    const char *name;
    const struct benchmark_query *setup_queries;
    size_t setup_query_count;
    const struct benchmark_query *additional_setup_queries;
    size_t additional_setup_query_count;
    const struct benchmark_query *queries;
    size_t query_count;
    const struct benchmark_query *prepared_bindings;
    size_t prepared_binding_count;
};

#define QUERY(sql_literal) {sql_literal, sizeof(sql_literal) - 1U}

static const struct benchmark_query wordpress_setup_queries[] = {
    QUERY("CREATE DATABASE wp"),
    QUERY("USE wp"),
    QUERY("SET sql_mode = ''"),
    QUERY("CREATE TABLE wp_options ("
          "option_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
          "option_name VARCHAR(191) NOT NULL DEFAULT '',"
          "option_value LONGTEXT NOT NULL,"
          "autoload VARCHAR(20) NOT NULL DEFAULT 'yes',"
          "PRIMARY KEY (option_id),"
          "UNIQUE KEY option_name (option_name),"
          "KEY autoload (autoload)"
          ")"),
    QUERY("CREATE TABLE wp_posts ("
          "ID BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
          "post_author BIGINT UNSIGNED NOT NULL DEFAULT 0,"
          "post_date DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00',"
          "post_date_gmt DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00',"
          "post_content LONGTEXT NOT NULL,"
          "post_title TEXT NOT NULL,"
          "post_excerpt TEXT NOT NULL,"
          "post_status VARCHAR(20) NOT NULL DEFAULT 'publish',"
          "comment_status VARCHAR(20) NOT NULL DEFAULT 'open',"
          "ping_status VARCHAR(20) NOT NULL DEFAULT 'open',"
          "post_password VARCHAR(255) NOT NULL DEFAULT '',"
          "post_name VARCHAR(200) NOT NULL DEFAULT '',"
          "to_ping TEXT NOT NULL,"
          "pinged TEXT NOT NULL,"
          "post_modified DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00',"
          "post_modified_gmt DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00',"
          "post_content_filtered LONGTEXT NOT NULL,"
          "post_parent BIGINT UNSIGNED NOT NULL DEFAULT 0,"
          "guid VARCHAR(255) NOT NULL DEFAULT '',"
          "menu_order INT NOT NULL DEFAULT 0,"
          "post_type VARCHAR(20) NOT NULL DEFAULT 'post',"
          "post_mime_type VARCHAR(100) NOT NULL DEFAULT '',"
          "comment_count BIGINT NOT NULL DEFAULT 0,"
          "PRIMARY KEY (ID),"
          "KEY post_name (post_name(191)),"
          "KEY type_status_date (post_type, post_status, post_date, ID),"
          "KEY post_parent (post_parent),"
          "KEY post_author (post_author)"
          ")"),
    QUERY("CREATE TABLE wp_postmeta ("
          "meta_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
          "post_id BIGINT UNSIGNED NOT NULL DEFAULT 0,"
          "meta_key VARCHAR(255) DEFAULT NULL,"
          "meta_value LONGTEXT,"
          "PRIMARY KEY (meta_id),"
          "KEY post_id (post_id),"
          "KEY meta_key (meta_key(191))"
          ")"),
    QUERY("INSERT INTO wp_options (option_name, option_value, autoload) VALUES "
          "('siteurl','https://example.test','yes'),"
          "('home','https://example.test','yes'),"
          "('blogname','MyLite Bench','yes'),"
          "('stylesheet','twentytwentyfive','yes'),"
          "('template','twentytwentyfive','yes'),"
          "('cron','{}','no')"),
    QUERY("INSERT INTO wp_posts "
          "(post_author, post_date, post_date_gmt, post_content, post_title, post_excerpt, "
          "post_status, comment_status, ping_status, post_password, post_name, to_ping, pinged, "
          "post_modified, post_modified_gmt, post_content_filtered, post_parent, guid, menu_order, "
          "post_type, post_mime_type, comment_count) VALUES "
          "(1,'2026-01-01 10:00:00','2026-01-01 10:00:00','content','Hello','',"
          "'publish','open','open','','hello','','','2026-01-01 10:00:00',"
          "'2026-01-01 10:00:00','',0,'https://example.test/?p=1',0,'post','',0),"
          "(1,'2026-01-02 10:00:00','2026-01-02 10:00:00','content','Draft','',"
          "'draft','open','open','','draft','','','2026-01-02 10:00:00',"
          "'2026-01-02 10:00:00','',0,'https://example.test/?p=2',0,'post','',0),"
          "(1,'2026-01-03 10:00:00','2026-01-03 10:00:00','content','Page','',"
          "'publish','closed','closed','','page','','','2026-01-03 10:00:00',"
          "'2026-01-03 10:00:00','',0,'https://example.test/?page_id=3',0,'page','',0)"),
    QUERY("INSERT INTO wp_postmeta (post_id, meta_key, meta_value) VALUES "
          "(1,'_thumbnail_id','10'),"
          "(1,'_edit_lock','1700000000:1'),"
          "(2,'_edit_lock','1700000001:1'),"
          "(3,'_wp_page_template','default')"),
};

#define WORDPRESS_COPY_POSTS_SQL                                                                   \
    "INSERT INTO wp_posts "                                                                        \
    "(post_author, post_date, post_date_gmt, post_content, post_title, post_excerpt, "             \
    "post_status, comment_status, ping_status, post_password, post_name, to_ping, pinged, "        \
    "post_modified, post_modified_gmt, post_content_filtered, post_parent, guid, menu_order, "     \
    "post_type, post_mime_type, comment_count) "                                                   \
    "SELECT post_author, post_date, post_date_gmt, post_content, post_title, post_excerpt, "       \
    "post_status, comment_status, ping_status, post_password, post_name, to_ping, pinged, "        \
    "post_modified, post_modified_gmt, post_content_filtered, post_parent, guid, menu_order, "     \
    "post_type, post_mime_type, comment_count FROM wp_posts"

static const struct benchmark_query wordpress_medium_additional_setup_queries[] = {
    QUERY("CREATE TABLE wp_users ("
          "ID BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
          "user_login VARCHAR(60) NOT NULL DEFAULT '',"
          "user_pass VARCHAR(255) NOT NULL DEFAULT '',"
          "user_nicename VARCHAR(50) NOT NULL DEFAULT '',"
          "user_email VARCHAR(100) NOT NULL DEFAULT '',"
          "user_url VARCHAR(100) NOT NULL DEFAULT '',"
          "user_registered DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00',"
          "user_activation_key VARCHAR(255) NOT NULL DEFAULT '',"
          "user_status INT NOT NULL DEFAULT 0,"
          "display_name VARCHAR(250) NOT NULL DEFAULT '',"
          "PRIMARY KEY (ID),"
          "KEY user_login_key (user_login),"
          "KEY user_nicename (user_nicename),"
          "KEY user_email (user_email)"
          ")"),
    QUERY("CREATE TABLE wp_usermeta ("
          "umeta_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
          "user_id BIGINT UNSIGNED NOT NULL DEFAULT 0,"
          "meta_key VARCHAR(255) DEFAULT NULL,"
          "meta_value LONGTEXT,"
          "PRIMARY KEY (umeta_id),"
          "KEY user_id (user_id),"
          "KEY meta_key (meta_key(191))"
          ")"),
    QUERY("CREATE TABLE wp_terms ("
          "term_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
          "name VARCHAR(200) NOT NULL DEFAULT '',"
          "slug VARCHAR(200) NOT NULL DEFAULT '',"
          "term_group BIGINT NOT NULL DEFAULT 0,"
          "PRIMARY KEY (term_id),"
          "KEY slug (slug(191)),"
          "KEY name (name(191))"
          ")"),
    QUERY("CREATE TABLE wp_term_taxonomy ("
          "term_taxonomy_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
          "term_id BIGINT UNSIGNED NOT NULL DEFAULT 0,"
          "taxonomy VARCHAR(32) NOT NULL DEFAULT '',"
          "description LONGTEXT NOT NULL,"
          "parent BIGINT UNSIGNED NOT NULL DEFAULT 0,"
          "count BIGINT NOT NULL DEFAULT 0,"
          "PRIMARY KEY (term_taxonomy_id),"
          "UNIQUE KEY term_id_taxonomy (term_id,taxonomy),"
          "KEY taxonomy (taxonomy)"
          ")"),
    QUERY("CREATE TABLE wp_term_relationships ("
          "object_id BIGINT UNSIGNED NOT NULL DEFAULT 0,"
          "term_taxonomy_id BIGINT UNSIGNED NOT NULL DEFAULT 0,"
          "term_order INT NOT NULL DEFAULT 0,"
          "PRIMARY KEY (object_id,term_taxonomy_id),"
          "KEY term_taxonomy_id (term_taxonomy_id)"
          ")"),
    QUERY("CREATE TABLE wp_comments ("
          "comment_ID BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
          "comment_post_ID BIGINT UNSIGNED NOT NULL DEFAULT 0,"
          "comment_author TINYTEXT NOT NULL,"
          "comment_author_email VARCHAR(100) NOT NULL DEFAULT '',"
          "comment_author_url VARCHAR(200) NOT NULL DEFAULT '',"
          "comment_author_IP VARCHAR(100) NOT NULL DEFAULT '',"
          "comment_date DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00',"
          "comment_date_gmt DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00',"
          "comment_content TEXT NOT NULL,"
          "comment_karma INT NOT NULL DEFAULT 0,"
          "comment_approved VARCHAR(20) NOT NULL DEFAULT '1',"
          "comment_agent VARCHAR(255) NOT NULL DEFAULT '',"
          "comment_type VARCHAR(20) NOT NULL DEFAULT 'comment',"
          "comment_parent BIGINT UNSIGNED NOT NULL DEFAULT 0,"
          "user_id BIGINT UNSIGNED NOT NULL DEFAULT 0,"
          "PRIMARY KEY (comment_ID),"
          "KEY comment_post_ID (comment_post_ID),"
          "KEY comment_approved_date_gmt (comment_approved,comment_date_gmt),"
          "KEY comment_date_gmt (comment_date_gmt),"
          "KEY comment_parent (comment_parent),"
          "KEY comment_author_email (comment_author_email(10))"
          ")"),
    QUERY("INSERT INTO wp_users "
          "(user_login,user_pass,user_nicename,user_email,user_url,user_registered,"
          "user_activation_key,user_status,display_name) VALUES "
          "('admin','$P$bench','admin','admin@example.test','https://example.test',"
          "'2025-01-01 00:00:00','',0,'Administrator'),"
          "('editor','$P$bench','editor','editor@example.test','',"
          "'2025-02-01 00:00:00','',0,'Site Editor'),"
          "('author','$P$bench','author','author@example.test','',"
          "'2025-03-01 00:00:00','',0,'Post Author')"),
    QUERY("INSERT INTO wp_usermeta (user_id,meta_key,meta_value) VALUES "
          "(1,'wp_capabilities','a:1:{s:13:\"administrator\";b:1;}'),"
          "(1,'wp_user_level','10'),"
          "(2,'wp_capabilities','a:1:{s:6:\"editor\";b:1;}'),"
          "(2,'wp_user_level','7'),"
          "(3,'wp_capabilities','a:1:{s:6:\"author\";b:1;}'),"
          "(3,'wp_user_level','2')"),
    QUERY("INSERT INTO wp_terms (name,slug,term_group) VALUES "
          "('News','news',0),('Performance','performance',0),"
          "('WordPress','wordpress',0),('MyLite','mylite',0)"),
    QUERY("INSERT INTO wp_term_taxonomy (term_id,taxonomy,description,parent,count) VALUES "
          "(1,'category','',0,48),(2,'category','',0,24),"
          "(3,'post_tag','',0,12),(4,'post_tag','',0,12)"),
    QUERY("INSERT INTO wp_term_relationships (object_id,term_taxonomy_id,term_order) VALUES "
          "(1,1,0),(1,3,0),(2,2,0),(3,1,0)"),
    QUERY("INSERT INTO wp_comments "
          "(comment_post_ID,comment_author,comment_author_email,comment_author_url,"
          "comment_author_IP,comment_date,comment_date_gmt,comment_content,comment_karma,"
          "comment_approved,comment_agent,comment_type,comment_parent,user_id) VALUES "
          "(1,'Reader','reader@example.test','','127.0.0.1','2026-01-04 10:00:00',"
          "'2026-01-04 10:00:00','First comment',0,'1','','comment',0,0),"
          "(1,'Editor','editor@example.test','','127.0.0.1','2026-01-05 10:00:00',"
          "'2026-01-05 10:00:00','Second comment',0,'1','','comment',0,2),"
          "(1,'Pending','pending@example.test','','127.0.0.1','2026-01-06 10:00:00',"
          "'2026-01-06 10:00:00','Pending comment',0,'0','','comment',0,0)"),
    QUERY(WORDPRESS_COPY_POSTS_SQL),
    QUERY(WORDPRESS_COPY_POSTS_SQL),
    QUERY(WORDPRESS_COPY_POSTS_SQL),
    QUERY(WORDPRESS_COPY_POSTS_SQL),
    QUERY(WORDPRESS_COPY_POSTS_SQL),
    QUERY("INSERT INTO wp_postmeta (post_id,meta_key,meta_value) "
          "SELECT post_id,meta_key,meta_value FROM wp_postmeta"),
    QUERY("INSERT INTO wp_postmeta (post_id,meta_key,meta_value) "
          "SELECT post_id,meta_key,meta_value FROM wp_postmeta"),
    QUERY("INSERT INTO wp_postmeta (post_id,meta_key,meta_value) "
          "SELECT post_id,meta_key,meta_value FROM wp_postmeta"),
};

#undef WORDPRESS_COPY_POSTS_SQL

static const struct benchmark_query wordpress_options_queries[] = {
    QUERY("SELECT option_value FROM wp_options WHERE option_name = 'siteurl' LIMIT 1"),
    QUERY("SELECT option_name, option_value FROM wp_options WHERE autoload = 'yes'"),
    QUERY("SELECT option_id FROM wp_options WHERE option_name = 'home'"),
    QUERY(
        "UPDATE wp_options SET option_value = 'MyLite Bench Updated' WHERE option_name = 'blogname'"
    ),
    QUERY("INSERT INTO wp_options (option_name, option_value, autoload) "
          "VALUES ('_transient_bench','1','no') "
          "ON DUPLICATE KEY UPDATE option_name = VALUES(option_name), "
          "option_value = VALUES(option_value), autoload = VALUES(autoload)"),
    QUERY("DELETE FROM wp_options WHERE option_name = '_transient_delete_miss'"),
};

static const struct benchmark_query wordpress_prepared_select_queries[] = {
    QUERY("SELECT option_value FROM wp_options WHERE option_name = ? LIMIT 1"),
};

static const struct benchmark_query wordpress_prepared_select_bindings[] = {
    QUERY("siteurl"),
};

static const struct benchmark_query wordpress_prepared_update_queries[] = {
    QUERY("UPDATE wp_options SET option_value = ? WHERE option_name = ?"),
};

static const struct benchmark_query wordpress_prepared_update_bindings[] = {
    QUERY("https://example.test"),
    QUERY("siteurl"),
};

static const struct benchmark_query wordpress_posts_meta_queries[] = {
    QUERY("SELECT ID, post_title FROM wp_posts "
          "WHERE post_type = 'post' AND post_status = 'publish' "
          "ORDER BY post_date DESC LIMIT 10"),
    QUERY("SELECT p.ID, pm.meta_value FROM wp_posts p "
          "LEFT JOIN wp_postmeta pm ON pm.post_id = p.ID AND pm.meta_key = '_thumbnail_id' "
          "WHERE p.ID = 1"),
    QUERY("SELECT meta_value FROM wp_postmeta WHERE post_id = 1 AND meta_key = '_edit_lock'"),
    QUERY("UPDATE wp_postmeta SET meta_value = '1700000002:1' WHERE post_id = 1 AND meta_key = "
          "'_edit_lock'"),
    QUERY("SELECT COUNT(*) FROM wp_posts WHERE post_type = 'post'"),
};

static const struct benchmark_query wordpress_metadata_queries[] = {
    QUERY("SHOW TABLE STATUS LIKE 'wp_options'"),
    QUERY("SHOW INDEX FROM wp_options"),
    QUERY("DESCRIBE wp_posts"),
    QUERY("SELECT COLUMN_NAME, DATA_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
          "WHERE TABLE_SCHEMA = 'wp' AND TABLE_NAME = 'wp_options'"),
    QUERY("SHOW CREATE TABLE wp_postmeta"),
};

static const struct benchmark_query wordpress_frontend_request_queries[] = {
    QUERY("SELECT option_name, option_value FROM wp_options WHERE autoload = 'yes'"),
    QUERY("SELECT option_value FROM wp_options WHERE option_name = 'siteurl' LIMIT 1"),
    QUERY("SELECT ID, post_title, post_date FROM wp_posts "
          "WHERE post_type = 'post' AND post_status = 'publish' "
          "ORDER BY post_date DESC LIMIT 10"),
    QUERY("SELECT COUNT(*) FROM wp_posts WHERE post_type = 'post' AND post_status = 'publish'"),
    QUERY("SELECT ID, post_content, post_title FROM wp_posts WHERE ID = 1 LIMIT 1"),
    QUERY("SELECT meta_key, meta_value FROM wp_postmeta WHERE post_id = 1"),
};

static const struct benchmark_query wordpress_medium_frontend_request_queries[] = {
    QUERY("SELECT option_name, option_value FROM wp_options WHERE autoload = 'yes'"),
    QUERY("SELECT option_value FROM wp_options WHERE option_name = 'siteurl' LIMIT 1"),
    QUERY("SELECT SQL_CALC_FOUND_ROWS ID, post_author, post_title, post_date FROM wp_posts "
          "WHERE post_type = 'post' AND post_status = 'publish' "
          "ORDER BY post_date DESC LIMIT 10"),
    QUERY("SELECT FOUND_ROWS()"),
    QUERY("SELECT ID, post_author, post_content, post_title, post_date FROM wp_posts "
          "WHERE ID = 1 LIMIT 1"),
    QUERY("SELECT meta_key, meta_value FROM wp_postmeta WHERE post_id = 1"),
    QUERY("SELECT t.term_id, t.name, tt.taxonomy FROM wp_terms t "
          "INNER JOIN wp_term_taxonomy tt ON t.term_id = tt.term_id "
          "INNER JOIN wp_term_relationships tr "
          "ON tr.term_taxonomy_id = tt.term_taxonomy_id WHERE tr.object_id = 1"),
    QUERY("SELECT comment_ID, comment_author, comment_content, comment_date_gmt "
          "FROM wp_comments WHERE comment_post_ID = 1 AND comment_approved = '1' "
          "ORDER BY comment_date_gmt LIMIT 10"),
    QUERY("SELECT COUNT(*) FROM wp_comments WHERE comment_post_ID = 1 "
          "AND comment_approved = '1'"),
    QUERY("SELECT ID, user_login, display_name FROM wp_users WHERE ID = 1 LIMIT 1"),
    QUERY("SELECT meta_key, meta_value FROM wp_usermeta WHERE user_id = 1"),
};

static const struct benchmark_query wordpress_write_request_queries[] = {
    QUERY("SELECT option_value FROM wp_options WHERE option_name = 'cron' LIMIT 1"),
    QUERY("INSERT INTO wp_options (option_name, option_value, autoload) "
          "VALUES ('_transient_request','1','no') "
          "ON DUPLICATE KEY UPDATE option_value = VALUES(option_value), "
          "autoload = VALUES(autoload)"),
    QUERY("UPDATE wp_postmeta SET meta_value = '1700000003:1' "
          "WHERE post_id = 1 AND meta_key = '_edit_lock'"),
    QUERY("UPDATE wp_posts SET post_modified = '2026-01-01 10:01:00', "
          "post_modified_gmt = '2026-01-01 10:01:00' WHERE ID = 1"),
    QUERY("DELETE FROM wp_options WHERE option_name = '_transient_request_miss'"),
};

static const struct benchmark_query information_schema_tables_lookup_queries[] = {
    QUERY("SELECT 1 AS expression FROM INFORMATION_SCHEMA.TABLES "
          "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'wp_options' "
          "AND TABLE_TYPE = 'BASE TABLE'"),
};

static const struct benchmark_query group_concat_distinct_setup_queries[] = {
    QUERY("CREATE DATABASE aggregate_bench"),
    QUERY("USE aggregate_bench"),
    QUERY("SET SESSION group_concat_max_len = 65535"),
    QUERY("CREATE TABLE distinct_values (id INT NOT NULL, name VARCHAR(32) NOT NULL)"),
    QUERY(
        "INSERT INTO distinct_values VALUES "
        "(1,'value0001'),(2,'value0002'),(3,'value0003'),(4,'value0004'),(5,'value0005'),(6,'"
        "value0006'),(7,'value0007'),(8,'value0008'),"
        "(9,'value0009'),(10,'value0010'),(11,'value0011'),(12,'value0012'),(13,'value0013'),(14,'"
        "value0014'),(15,'value0015'),(16,'value0016'),"
        "(17,'value0017'),(18,'value0018'),(19,'value0019'),(20,'value0020'),(21,'value0021'),(22,'"
        "value0022'),(23,'value0023'),(24,'value0024'),"
        "(25,'value0025'),(26,'value0026'),(27,'value0027'),(28,'value0028'),(29,'value0029'),(30,'"
        "value0030'),(31,'value0031'),(32,'value0032'),"
        "(33,'value0033'),(34,'value0034'),(35,'value0035'),(36,'value0036'),(37,'value0037'),(38,'"
        "value0038'),(39,'value0039'),(40,'value0040'),"
        "(41,'value0041'),(42,'value0042'),(43,'value0043'),(44,'value0044'),(45,'value0045'),(46,'"
        "value0046'),(47,'value0047'),(48,'value0048'),"
        "(49,'value0049'),(50,'value0050'),(51,'value0051'),(52,'value0052'),(53,'value0053'),(54,'"
        "value0054'),(55,'value0055'),(56,'value0056'),"
        "(57,'value0057'),(58,'value0058'),(59,'value0059'),(60,'value0060'),(61,'value0061'),(62,'"
        "value0062'),(63,'value0063'),(64,'value0064'),"
        "(65,'value0065'),(66,'value0066'),(67,'value0067'),(68,'value0068'),(69,'value0069'),(70,'"
        "value0070'),(71,'value0071'),(72,'value0072'),"
        "(73,'value0073'),(74,'value0074'),(75,'value0075'),(76,'value0076'),(77,'value0077'),(78,'"
        "value0078'),(79,'value0079'),(80,'value0080'),"
        "(81,'value0081'),(82,'value0082'),(83,'value0083'),(84,'value0084'),(85,'value0085'),(86,'"
        "value0086'),(87,'value0087'),(88,'value0088'),"
        "(89,'value0089'),(90,'value0090'),(91,'value0091'),(92,'value0092'),(93,'value0093'),(94,'"
        "value0094'),(95,'value0095'),(96,'value0096'),"
        "(97,'value0097'),(98,'value0098'),(99,'value0099'),(100,'value0100'),(101,'value0101'),("
        "102,'value0102'),(103,'value0103'),(104,'value0104'),"
        "(105,'value0105'),(106,'value0106'),(107,'value0107'),(108,'value0108'),(109,'value0109'),"
        "(110,'value0110'),(111,'value0111'),(112,'value0112'),"
        "(113,'value0113'),(114,'value0114'),(115,'value0115'),(116,'value0116'),(117,'value0117'),"
        "(118,'value0118'),(119,'value0119'),(120,'value0120'),"
        "(121,'value0121'),(122,'value0122'),(123,'value0123'),(124,'value0124'),(125,'value0125'),"
        "(126,'value0126'),(127,'value0127'),(128,'value0128')"
    ),
    QUERY(
        "INSERT INTO distinct_values VALUES "
        "(129,'value0129'),(130,'value0130'),(131,'value0131'),(132,'value0132'),(133,'value0133'),"
        "(134,'value0134'),(135,'value0135'),(136,'value0136'),"
        "(137,'value0137'),(138,'value0138'),(139,'value0139'),(140,'value0140'),(141,'value0141'),"
        "(142,'value0142'),(143,'value0143'),(144,'value0144'),"
        "(145,'value0145'),(146,'value0146'),(147,'value0147'),(148,'value0148'),(149,'value0149'),"
        "(150,'value0150'),(151,'value0151'),(152,'value0152'),"
        "(153,'value0153'),(154,'value0154'),(155,'value0155'),(156,'value0156'),(157,'value0157'),"
        "(158,'value0158'),(159,'value0159'),(160,'value0160'),"
        "(161,'value0161'),(162,'value0162'),(163,'value0163'),(164,'value0164'),(165,'value0165'),"
        "(166,'value0166'),(167,'value0167'),(168,'value0168'),"
        "(169,'value0169'),(170,'value0170'),(171,'value0171'),(172,'value0172'),(173,'value0173'),"
        "(174,'value0174'),(175,'value0175'),(176,'value0176'),"
        "(177,'value0177'),(178,'value0178'),(179,'value0179'),(180,'value0180'),(181,'value0181'),"
        "(182,'value0182'),(183,'value0183'),(184,'value0184'),"
        "(185,'value0185'),(186,'value0186'),(187,'value0187'),(188,'value0188'),(189,'value0189'),"
        "(190,'value0190'),(191,'value0191'),(192,'value0192'),"
        "(193,'value0193'),(194,'value0194'),(195,'value0195'),(196,'value0196'),(197,'value0197'),"
        "(198,'value0198'),(199,'value0199'),(200,'value0200'),"
        "(201,'value0201'),(202,'value0202'),(203,'value0203'),(204,'value0204'),(205,'value0205'),"
        "(206,'value0206'),(207,'value0207'),(208,'value0208'),"
        "(209,'value0209'),(210,'value0210'),(211,'value0211'),(212,'value0212'),(213,'value0213'),"
        "(214,'value0214'),(215,'value0215'),(216,'value0216'),"
        "(217,'value0217'),(218,'value0218'),(219,'value0219'),(220,'value0220'),(221,'value0221'),"
        "(222,'value0222'),(223,'value0223'),(224,'value0224'),"
        "(225,'value0225'),(226,'value0226'),(227,'value0227'),(228,'value0228'),(229,'value0229'),"
        "(230,'value0230'),(231,'value0231'),(232,'value0232'),"
        "(233,'value0233'),(234,'value0234'),(235,'value0235'),(236,'value0236'),(237,'value0237'),"
        "(238,'value0238'),(239,'value0239'),(240,'value0240'),"
        "(241,'value0241'),(242,'value0242'),(243,'value0243'),(244,'value0244'),(245,'value0245'),"
        "(246,'value0246'),(247,'value0247'),(248,'value0248'),"
        "(249,'value0249'),(250,'value0250'),(251,'value0251'),(252,'value0252'),(253,'value0253'),"
        "(254,'value0254'),(255,'value0255'),(256,'value0256')"
    ),
    QUERY(
        "INSERT INTO distinct_values VALUES "
        "(257,'value0257'),(258,'value0258'),(259,'value0259'),(260,'value0260'),(261,'value0261'),"
        "(262,'value0262'),(263,'value0263'),(264,'value0264'),"
        "(265,'value0265'),(266,'value0266'),(267,'value0267'),(268,'value0268'),(269,'value0269'),"
        "(270,'value0270'),(271,'value0271'),(272,'value0272'),"
        "(273,'value0273'),(274,'value0274'),(275,'value0275'),(276,'value0276'),(277,'value0277'),"
        "(278,'value0278'),(279,'value0279'),(280,'value0280'),"
        "(281,'value0281'),(282,'value0282'),(283,'value0283'),(284,'value0284'),(285,'value0285'),"
        "(286,'value0286'),(287,'value0287'),(288,'value0288'),"
        "(289,'value0289'),(290,'value0290'),(291,'value0291'),(292,'value0292'),(293,'value0293'),"
        "(294,'value0294'),(295,'value0295'),(296,'value0296'),"
        "(297,'value0297'),(298,'value0298'),(299,'value0299'),(300,'value0300'),(301,'value0301'),"
        "(302,'value0302'),(303,'value0303'),(304,'value0304'),"
        "(305,'value0305'),(306,'value0306'),(307,'value0307'),(308,'value0308'),(309,'value0309'),"
        "(310,'value0310'),(311,'value0311'),(312,'value0312'),"
        "(313,'value0313'),(314,'value0314'),(315,'value0315'),(316,'value0316'),(317,'value0317'),"
        "(318,'value0318'),(319,'value0319'),(320,'value0320'),"
        "(321,'value0321'),(322,'value0322'),(323,'value0323'),(324,'value0324'),(325,'value0325'),"
        "(326,'value0326'),(327,'value0327'),(328,'value0328'),"
        "(329,'value0329'),(330,'value0330'),(331,'value0331'),(332,'value0332'),(333,'value0333'),"
        "(334,'value0334'),(335,'value0335'),(336,'value0336'),"
        "(337,'value0337'),(338,'value0338'),(339,'value0339'),(340,'value0340'),(341,'value0341'),"
        "(342,'value0342'),(343,'value0343'),(344,'value0344'),"
        "(345,'value0345'),(346,'value0346'),(347,'value0347'),(348,'value0348'),(349,'value0349'),"
        "(350,'value0350'),(351,'value0351'),(352,'value0352'),"
        "(353,'value0353'),(354,'value0354'),(355,'value0355'),(356,'value0356'),(357,'value0357'),"
        "(358,'value0358'),(359,'value0359'),(360,'value0360'),"
        "(361,'value0361'),(362,'value0362'),(363,'value0363'),(364,'value0364'),(365,'value0365'),"
        "(366,'value0366'),(367,'value0367'),(368,'value0368'),"
        "(369,'value0369'),(370,'value0370'),(371,'value0371'),(372,'value0372'),(373,'value0373'),"
        "(374,'value0374'),(375,'value0375'),(376,'value0376'),"
        "(377,'value0377'),(378,'value0378'),(379,'value0379'),(380,'value0380'),(381,'value0381'),"
        "(382,'value0382'),(383,'value0383'),(384,'value0384')"
    ),
    QUERY(
        "INSERT INTO distinct_values VALUES "
        "(385,'value0385'),(386,'value0386'),(387,'value0387'),(388,'value0388'),(389,'value0389'),"
        "(390,'value0390'),(391,'value0391'),(392,'value0392'),"
        "(393,'value0393'),(394,'value0394'),(395,'value0395'),(396,'value0396'),(397,'value0397'),"
        "(398,'value0398'),(399,'value0399'),(400,'value0400'),"
        "(401,'value0401'),(402,'value0402'),(403,'value0403'),(404,'value0404'),(405,'value0405'),"
        "(406,'value0406'),(407,'value0407'),(408,'value0408'),"
        "(409,'value0409'),(410,'value0410'),(411,'value0411'),(412,'value0412'),(413,'value0413'),"
        "(414,'value0414'),(415,'value0415'),(416,'value0416'),"
        "(417,'value0417'),(418,'value0418'),(419,'value0419'),(420,'value0420'),(421,'value0421'),"
        "(422,'value0422'),(423,'value0423'),(424,'value0424'),"
        "(425,'value0425'),(426,'value0426'),(427,'value0427'),(428,'value0428'),(429,'value0429'),"
        "(430,'value0430'),(431,'value0431'),(432,'value0432'),"
        "(433,'value0433'),(434,'value0434'),(435,'value0435'),(436,'value0436'),(437,'value0437'),"
        "(438,'value0438'),(439,'value0439'),(440,'value0440'),"
        "(441,'value0441'),(442,'value0442'),(443,'value0443'),(444,'value0444'),(445,'value0445'),"
        "(446,'value0446'),(447,'value0447'),(448,'value0448'),"
        "(449,'value0449'),(450,'value0450'),(451,'value0451'),(452,'value0452'),(453,'value0453'),"
        "(454,'value0454'),(455,'value0455'),(456,'value0456'),"
        "(457,'value0457'),(458,'value0458'),(459,'value0459'),(460,'value0460'),(461,'value0461'),"
        "(462,'value0462'),(463,'value0463'),(464,'value0464'),"
        "(465,'value0465'),(466,'value0466'),(467,'value0467'),(468,'value0468'),(469,'value0469'),"
        "(470,'value0470'),(471,'value0471'),(472,'value0472'),"
        "(473,'value0473'),(474,'value0474'),(475,'value0475'),(476,'value0476'),(477,'value0477'),"
        "(478,'value0478'),(479,'value0479'),(480,'value0480'),"
        "(481,'value0481'),(482,'value0482'),(483,'value0483'),(484,'value0484'),(485,'value0485'),"
        "(486,'value0486'),(487,'value0487'),(488,'value0488'),"
        "(489,'value0489'),(490,'value0490'),(491,'value0491'),(492,'value0492'),(493,'value0493'),"
        "(494,'value0494'),(495,'value0495'),(496,'value0496'),"
        "(497,'value0497'),(498,'value0498'),(499,'value0499'),(500,'value0500'),(501,'value0501'),"
        "(502,'value0502'),(503,'value0503'),(504,'value0504'),"
        "(505,'value0505'),(506,'value0506'),(507,'value0507'),(508,'value0508'),(509,'value0509'),"
        "(510,'value0510'),(511,'value0511'),(512,'value0512')"
    ),
    QUERY(
        "INSERT INTO distinct_values VALUES "
        "(513,'value0513'),(514,'value0514'),(515,'value0515'),(516,'value0516'),(517,'value0517'),"
        "(518,'value0518'),(519,'value0519'),(520,'value0520'),"
        "(521,'value0521'),(522,'value0522'),(523,'value0523'),(524,'value0524'),(525,'value0525'),"
        "(526,'value0526'),(527,'value0527'),(528,'value0528'),"
        "(529,'value0529'),(530,'value0530'),(531,'value0531'),(532,'value0532'),(533,'value0533'),"
        "(534,'value0534'),(535,'value0535'),(536,'value0536'),"
        "(537,'value0537'),(538,'value0538'),(539,'value0539'),(540,'value0540'),(541,'value0541'),"
        "(542,'value0542'),(543,'value0543'),(544,'value0544'),"
        "(545,'value0545'),(546,'value0546'),(547,'value0547'),(548,'value0548'),(549,'value0549'),"
        "(550,'value0550'),(551,'value0551'),(552,'value0552'),"
        "(553,'value0553'),(554,'value0554'),(555,'value0555'),(556,'value0556'),(557,'value0557'),"
        "(558,'value0558'),(559,'value0559'),(560,'value0560'),"
        "(561,'value0561'),(562,'value0562'),(563,'value0563'),(564,'value0564'),(565,'value0565'),"
        "(566,'value0566'),(567,'value0567'),(568,'value0568'),"
        "(569,'value0569'),(570,'value0570'),(571,'value0571'),(572,'value0572'),(573,'value0573'),"
        "(574,'value0574'),(575,'value0575'),(576,'value0576'),"
        "(577,'value0577'),(578,'value0578'),(579,'value0579'),(580,'value0580'),(581,'value0581'),"
        "(582,'value0582'),(583,'value0583'),(584,'value0584'),"
        "(585,'value0585'),(586,'value0586'),(587,'value0587'),(588,'value0588'),(589,'value0589'),"
        "(590,'value0590'),(591,'value0591'),(592,'value0592'),"
        "(593,'value0593'),(594,'value0594'),(595,'value0595'),(596,'value0596'),(597,'value0597'),"
        "(598,'value0598'),(599,'value0599'),(600,'value0600'),"
        "(601,'value0601'),(602,'value0602'),(603,'value0603'),(604,'value0604'),(605,'value0605'),"
        "(606,'value0606'),(607,'value0607'),(608,'value0608'),"
        "(609,'value0609'),(610,'value0610'),(611,'value0611'),(612,'value0612'),(613,'value0613'),"
        "(614,'value0614'),(615,'value0615'),(616,'value0616'),"
        "(617,'value0617'),(618,'value0618'),(619,'value0619'),(620,'value0620'),(621,'value0621'),"
        "(622,'value0622'),(623,'value0623'),(624,'value0624'),"
        "(625,'value0625'),(626,'value0626'),(627,'value0627'),(628,'value0628'),(629,'value0629'),"
        "(630,'value0630'),(631,'value0631'),(632,'value0632'),"
        "(633,'value0633'),(634,'value0634'),(635,'value0635'),(636,'value0636'),(637,'value0637'),"
        "(638,'value0638'),(639,'value0639'),(640,'value0640')"
    ),
    QUERY(
        "INSERT INTO distinct_values VALUES "
        "(641,'value0641'),(642,'value0642'),(643,'value0643'),(644,'value0644'),(645,'value0645'),"
        "(646,'value0646'),(647,'value0647'),(648,'value0648'),"
        "(649,'value0649'),(650,'value0650'),(651,'value0651'),(652,'value0652'),(653,'value0653'),"
        "(654,'value0654'),(655,'value0655'),(656,'value0656'),"
        "(657,'value0657'),(658,'value0658'),(659,'value0659'),(660,'value0660'),(661,'value0661'),"
        "(662,'value0662'),(663,'value0663'),(664,'value0664'),"
        "(665,'value0665'),(666,'value0666'),(667,'value0667'),(668,'value0668'),(669,'value0669'),"
        "(670,'value0670'),(671,'value0671'),(672,'value0672'),"
        "(673,'value0673'),(674,'value0674'),(675,'value0675'),(676,'value0676'),(677,'value0677'),"
        "(678,'value0678'),(679,'value0679'),(680,'value0680'),"
        "(681,'value0681'),(682,'value0682'),(683,'value0683'),(684,'value0684'),(685,'value0685'),"
        "(686,'value0686'),(687,'value0687'),(688,'value0688'),"
        "(689,'value0689'),(690,'value0690'),(691,'value0691'),(692,'value0692'),(693,'value0693'),"
        "(694,'value0694'),(695,'value0695'),(696,'value0696'),"
        "(697,'value0697'),(698,'value0698'),(699,'value0699'),(700,'value0700'),(701,'value0701'),"
        "(702,'value0702'),(703,'value0703'),(704,'value0704'),"
        "(705,'value0705'),(706,'value0706'),(707,'value0707'),(708,'value0708'),(709,'value0709'),"
        "(710,'value0710'),(711,'value0711'),(712,'value0712'),"
        "(713,'value0713'),(714,'value0714'),(715,'value0715'),(716,'value0716'),(717,'value0717'),"
        "(718,'value0718'),(719,'value0719'),(720,'value0720'),"
        "(721,'value0721'),(722,'value0722'),(723,'value0723'),(724,'value0724'),(725,'value0725'),"
        "(726,'value0726'),(727,'value0727'),(728,'value0728'),"
        "(729,'value0729'),(730,'value0730'),(731,'value0731'),(732,'value0732'),(733,'value0733'),"
        "(734,'value0734'),(735,'value0735'),(736,'value0736'),"
        "(737,'value0737'),(738,'value0738'),(739,'value0739'),(740,'value0740'),(741,'value0741'),"
        "(742,'value0742'),(743,'value0743'),(744,'value0744'),"
        "(745,'value0745'),(746,'value0746'),(747,'value0747'),(748,'value0748'),(749,'value0749'),"
        "(750,'value0750'),(751,'value0751'),(752,'value0752'),"
        "(753,'value0753'),(754,'value0754'),(755,'value0755'),(756,'value0756'),(757,'value0757'),"
        "(758,'value0758'),(759,'value0759'),(760,'value0760'),"
        "(761,'value0761'),(762,'value0762'),(763,'value0763'),(764,'value0764'),(765,'value0765'),"
        "(766,'value0766'),(767,'value0767'),(768,'value0768')"
    ),
    QUERY(
        "INSERT INTO distinct_values VALUES "
        "(769,'value0769'),(770,'value0770'),(771,'value0771'),(772,'value0772'),(773,'value0773'),"
        "(774,'value0774'),(775,'value0775'),(776,'value0776'),"
        "(777,'value0777'),(778,'value0778'),(779,'value0779'),(780,'value0780'),(781,'value0781'),"
        "(782,'value0782'),(783,'value0783'),(784,'value0784'),"
        "(785,'value0785'),(786,'value0786'),(787,'value0787'),(788,'value0788'),(789,'value0789'),"
        "(790,'value0790'),(791,'value0791'),(792,'value0792'),"
        "(793,'value0793'),(794,'value0794'),(795,'value0795'),(796,'value0796'),(797,'value0797'),"
        "(798,'value0798'),(799,'value0799'),(800,'value0800'),"
        "(801,'value0801'),(802,'value0802'),(803,'value0803'),(804,'value0804'),(805,'value0805'),"
        "(806,'value0806'),(807,'value0807'),(808,'value0808'),"
        "(809,'value0809'),(810,'value0810'),(811,'value0811'),(812,'value0812'),(813,'value0813'),"
        "(814,'value0814'),(815,'value0815'),(816,'value0816'),"
        "(817,'value0817'),(818,'value0818'),(819,'value0819'),(820,'value0820'),(821,'value0821'),"
        "(822,'value0822'),(823,'value0823'),(824,'value0824'),"
        "(825,'value0825'),(826,'value0826'),(827,'value0827'),(828,'value0828'),(829,'value0829'),"
        "(830,'value0830'),(831,'value0831'),(832,'value0832'),"
        "(833,'value0833'),(834,'value0834'),(835,'value0835'),(836,'value0836'),(837,'value0837'),"
        "(838,'value0838'),(839,'value0839'),(840,'value0840'),"
        "(841,'value0841'),(842,'value0842'),(843,'value0843'),(844,'value0844'),(845,'value0845'),"
        "(846,'value0846'),(847,'value0847'),(848,'value0848'),"
        "(849,'value0849'),(850,'value0850'),(851,'value0851'),(852,'value0852'),(853,'value0853'),"
        "(854,'value0854'),(855,'value0855'),(856,'value0856'),"
        "(857,'value0857'),(858,'value0858'),(859,'value0859'),(860,'value0860'),(861,'value0861'),"
        "(862,'value0862'),(863,'value0863'),(864,'value0864'),"
        "(865,'value0865'),(866,'value0866'),(867,'value0867'),(868,'value0868'),(869,'value0869'),"
        "(870,'value0870'),(871,'value0871'),(872,'value0872'),"
        "(873,'value0873'),(874,'value0874'),(875,'value0875'),(876,'value0876'),(877,'value0877'),"
        "(878,'value0878'),(879,'value0879'),(880,'value0880'),"
        "(881,'value0881'),(882,'value0882'),(883,'value0883'),(884,'value0884'),(885,'value0885'),"
        "(886,'value0886'),(887,'value0887'),(888,'value0888'),"
        "(889,'value0889'),(890,'value0890'),(891,'value0891'),(892,'value0892'),(893,'value0893'),"
        "(894,'value0894'),(895,'value0895'),(896,'value0896')"
    ),
    QUERY(
        "INSERT INTO distinct_values VALUES "
        "(897,'value0897'),(898,'value0898'),(899,'value0899'),(900,'value0900'),(901,'value0901'),"
        "(902,'value0902'),(903,'value0903'),(904,'value0904'),"
        "(905,'value0905'),(906,'value0906'),(907,'value0907'),(908,'value0908'),(909,'value0909'),"
        "(910,'value0910'),(911,'value0911'),(912,'value0912'),"
        "(913,'value0913'),(914,'value0914'),(915,'value0915'),(916,'value0916'),(917,'value0917'),"
        "(918,'value0918'),(919,'value0919'),(920,'value0920'),"
        "(921,'value0921'),(922,'value0922'),(923,'value0923'),(924,'value0924'),(925,'value0925'),"
        "(926,'value0926'),(927,'value0927'),(928,'value0928'),"
        "(929,'value0929'),(930,'value0930'),(931,'value0931'),(932,'value0932'),(933,'value0933'),"
        "(934,'value0934'),(935,'value0935'),(936,'value0936'),"
        "(937,'value0937'),(938,'value0938'),(939,'value0939'),(940,'value0940'),(941,'value0941'),"
        "(942,'value0942'),(943,'value0943'),(944,'value0944'),"
        "(945,'value0945'),(946,'value0946'),(947,'value0947'),(948,'value0948'),(949,'value0949'),"
        "(950,'value0950'),(951,'value0951'),(952,'value0952'),"
        "(953,'value0953'),(954,'value0954'),(955,'value0955'),(956,'value0956'),(957,'value0957'),"
        "(958,'value0958'),(959,'value0959'),(960,'value0960'),"
        "(961,'value0961'),(962,'value0962'),(963,'value0963'),(964,'value0964'),(965,'value0965'),"
        "(966,'value0966'),(967,'value0967'),(968,'value0968'),"
        "(969,'value0969'),(970,'value0970'),(971,'value0971'),(972,'value0972'),(973,'value0973'),"
        "(974,'value0974'),(975,'value0975'),(976,'value0976'),"
        "(977,'value0977'),(978,'value0978'),(979,'value0979'),(980,'value0980'),(981,'value0981'),"
        "(982,'value0982'),(983,'value0983'),(984,'value0984'),"
        "(985,'value0985'),(986,'value0986'),(987,'value0987'),(988,'value0988'),(989,'value0989'),"
        "(990,'value0990'),(991,'value0991'),(992,'value0992'),"
        "(993,'value0993'),(994,'value0994'),(995,'value0995'),(996,'value0996'),(997,'value0997'),"
        "(998,'value0998'),(999,'value0999'),(1000,'value1000'),"
        "(1001,'value1001'),(1002,'value1002'),(1003,'value1003'),(1004,'value1004'),(1005,'"
        "value1005'),(1006,'value1006'),(1007,'value1007'),(1008,'value1008'),"
        "(1009,'value1009'),(1010,'value1010'),(1011,'value1011'),(1012,'value1012'),(1013,'"
        "value1013'),(1014,'value1014'),(1015,'value1015'),(1016,'value1016'),"
        "(1017,'value1017'),(1018,'value1018'),(1019,'value1019'),(1020,'value1020'),(1021,'"
        "value1021'),(1022,'value1022'),(1023,'value1023'),(1024,'value1024')"
    ),
};

static const struct benchmark_query group_concat_distinct_queries[] = {
    QUERY("SELECT GROUP_CONCAT(DISTINCT name ORDER BY id SEPARATOR '|') FROM distinct_values"),
};

static const struct benchmark_query wordpress_cursor_queries[] = {
    QUERY("SELECT option_value FROM wp_options WHERE option_name = 'siteurl' LIMIT 1"),
    QUERY("SELECT option_name, option_value FROM wp_options WHERE autoload = 'yes'"),
    QUERY("SELECT ID, post_title FROM wp_posts "
          "WHERE post_type = 'post' AND post_status = 'publish' "
          "ORDER BY post_date DESC LIMIT 10"),
    QUERY("SELECT p.ID, pm.meta_value FROM wp_posts p "
          "LEFT JOIN wp_postmeta pm ON pm.post_id = p.ID AND pm.meta_key = '_thumbnail_id' "
          "WHERE p.ID = 1"),
    QUERY("SELECT COLUMN_NAME, DATA_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
          "WHERE TABLE_SCHEMA = 'wp' AND TABLE_NAME = 'wp_options'"),
};

static const struct benchmark_query parser_wordpress_queries[] = {
    QUERY("SELECT option_value FROM wp_options WHERE option_name = 'siteurl' LIMIT 1"),
    QUERY("SELECT option_name, option_value FROM wp_options WHERE autoload = 'yes'"),
    QUERY(
        "UPDATE wp_options SET option_value = 'MyLite Bench Updated' WHERE option_name = 'blogname'"
    ),
    QUERY("INSERT INTO wp_options (option_name, option_value, autoload) "
          "VALUES ('_transient_bench','1','no') "
          "ON DUPLICATE KEY UPDATE option_value = '1'"),
    QUERY("SELECT ID, post_title FROM wp_posts "
          "WHERE post_type = 'post' AND post_status = 'publish' "
          "ORDER BY post_date DESC LIMIT 10"),
    QUERY("SELECT p.ID, pm.meta_value FROM wp_posts p "
          "LEFT JOIN wp_postmeta pm ON pm.post_id = p.ID AND pm.meta_key = '_thumbnail_id' "
          "WHERE p.ID = 1"),
    QUERY("SHOW TABLE STATUS LIKE 'wp_options'"),
    QUERY("SHOW INDEX FROM wp_options"),
    QUERY("DESCRIBE wp_posts"),
    QUERY("SELECT COLUMN_NAME, DATA_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
          "WHERE TABLE_SCHEMA = 'wp' AND TABLE_NAME = 'wp_options'"),
    QUERY("SHOW CREATE TABLE wp_postmeta"),
};

static const struct runtime_scenario runtime_scenarios[] = {
    {
        .name = "runtime.wp_options_hot",
        .setup_queries = wordpress_setup_queries,
        .setup_query_count = sizeof(wordpress_setup_queries) / sizeof(wordpress_setup_queries[0]),
        .queries = wordpress_options_queries,
        .query_count = sizeof(wordpress_options_queries) / sizeof(wordpress_options_queries[0]),
    },
    {
        .name = "runtime.wp_posts_meta_hot",
        .setup_queries = wordpress_setup_queries,
        .setup_query_count = sizeof(wordpress_setup_queries) / sizeof(wordpress_setup_queries[0]),
        .queries = wordpress_posts_meta_queries,
        .query_count =
            sizeof(wordpress_posts_meta_queries) / sizeof(wordpress_posts_meta_queries[0]),
    },
    {
        .name = "runtime.wp_metadata_hot",
        .setup_queries = wordpress_setup_queries,
        .setup_query_count = sizeof(wordpress_setup_queries) / sizeof(wordpress_setup_queries[0]),
        .queries = wordpress_metadata_queries,
        .query_count = sizeof(wordpress_metadata_queries) / sizeof(wordpress_metadata_queries[0]),
    },
    {
        .name = "runtime.wp_frontend_request",
        .setup_queries = wordpress_setup_queries,
        .setup_query_count = sizeof(wordpress_setup_queries) / sizeof(wordpress_setup_queries[0]),
        .queries = wordpress_frontend_request_queries,
        .query_count = sizeof(wordpress_frontend_request_queries) /
                       sizeof(wordpress_frontend_request_queries[0]),
    },
    {
        .name = "runtime.wp_medium_frontend_request",
        .setup_queries = wordpress_setup_queries,
        .setup_query_count = sizeof(wordpress_setup_queries) / sizeof(wordpress_setup_queries[0]),
        .additional_setup_queries = wordpress_medium_additional_setup_queries,
        .additional_setup_query_count = sizeof(wordpress_medium_additional_setup_queries) /
                                        sizeof(wordpress_medium_additional_setup_queries[0]),
        .queries = wordpress_medium_frontend_request_queries,
        .query_count = sizeof(wordpress_medium_frontend_request_queries) /
                       sizeof(wordpress_medium_frontend_request_queries[0]),
    },
    {
        .name = "runtime.wp_write_request",
        .setup_queries = wordpress_setup_queries,
        .setup_query_count = sizeof(wordpress_setup_queries) / sizeof(wordpress_setup_queries[0]),
        .queries = wordpress_write_request_queries,
        .query_count =
            sizeof(wordpress_write_request_queries) / sizeof(wordpress_write_request_queries[0]),
    },
    {
        .name = "runtime.information_schema_tables_lookup",
        .setup_queries = wordpress_setup_queries,
        .setup_query_count = sizeof(wordpress_setup_queries) / sizeof(wordpress_setup_queries[0]),
        .queries = information_schema_tables_lookup_queries,
        .query_count = sizeof(information_schema_tables_lookup_queries) /
                       sizeof(information_schema_tables_lookup_queries[0]),
    },
    {
        .name = "runtime.group_concat_distinct_1024",
        .setup_queries = group_concat_distinct_setup_queries,
        .setup_query_count = sizeof(group_concat_distinct_setup_queries) /
                             sizeof(group_concat_distinct_setup_queries[0]),
        .queries = group_concat_distinct_queries,
        .query_count =
            sizeof(group_concat_distinct_queries) / sizeof(group_concat_distinct_queries[0]),
    },
};

static const struct runtime_scenario runtime_cursor_scenarios[] = {
    {
        .name = "runtime.wp_select_cursor",
        .setup_queries = wordpress_setup_queries,
        .setup_query_count = sizeof(wordpress_setup_queries) / sizeof(wordpress_setup_queries[0]),
        .queries = wordpress_cursor_queries,
        .query_count = sizeof(wordpress_cursor_queries) / sizeof(wordpress_cursor_queries[0]),
    },
};

static const struct runtime_scenario runtime_prepared_scenarios[] = {
    {
        .name = "runtime.wp_prepared_select",
        .setup_queries = wordpress_setup_queries,
        .setup_query_count = sizeof(wordpress_setup_queries) / sizeof(wordpress_setup_queries[0]),
        .queries = wordpress_prepared_select_queries,
        .query_count = sizeof(wordpress_prepared_select_queries) /
                       sizeof(wordpress_prepared_select_queries[0]),
        .prepared_bindings = wordpress_prepared_select_bindings,
        .prepared_binding_count = sizeof(wordpress_prepared_select_bindings) /
                                  sizeof(wordpress_prepared_select_bindings[0]),
    },
    {
        .name = "runtime.wp_prepared_update",
        .setup_queries = wordpress_setup_queries,
        .setup_query_count = sizeof(wordpress_setup_queries) / sizeof(wordpress_setup_queries[0]),
        .queries = wordpress_prepared_update_queries,
        .query_count = sizeof(wordpress_prepared_update_queries) /
                       sizeof(wordpress_prepared_update_queries[0]),
        .prepared_bindings = wordpress_prepared_update_bindings,
        .prepared_binding_count = sizeof(wordpress_prepared_update_bindings) /
                                  sizeof(wordpress_prepared_update_bindings[0]),
    },
};

static int parse_options(int argc, char **argv, struct benchmark_options *out_options);
static int parse_option(
    int argc,
    char **argv,
    int *index,
    const char *program_name,
    struct benchmark_options *out_options
);
static int parse_size_argument(
    int argc,
    char **argv,
    int *index,
    const char *option_name,
    size_t *out_value
);
static int parse_nonnegative_size_argument(
    int argc,
    char **argv,
    int *index,
    const char *option_name,
    size_t *out_value
);
static int parse_text_argument(
    int argc,
    char **argv,
    int *index,
    const char *option_name,
    const char **out_value
);
static int parse_filter_argument(
    int argc,
    char **argv,
    int *index,
    enum benchmark_filter *out_filter
);
static const char *consume_option_value(int argc, char **argv, int *index, const char *option_name);
static int parse_filter_option(const char *text, enum benchmark_filter *out_filter);
static int parse_size_option(const char *text, bool allow_zero, size_t *out_value);
static void print_usage(const char *program_name, FILE *stream);
static void print_scenario_list(void);
static void print_runtime_scenario_list(
    const struct runtime_scenario *scenarios,
    size_t scenario_count
);
static bool filter_includes(enum benchmark_filter filter, enum benchmark_filter candidate);
static int run_benchmarks(const struct benchmark_options *options);
static int run_builtin_lexer_benchmark(const struct benchmark_options *options);
static int run_builtin_parse_benchmark(const struct benchmark_options *options);
static int run_csv_benchmarks(const struct benchmark_options *options);
static int run_runtime_benchmarks(const struct benchmark_options *options);
static bool runtime_scenario_filter_exists(const struct benchmark_options *options);
typedef int (*runtime_scenario_runner)(
    const struct runtime_scenario *scenario,
    const struct runtime_repetition_options *repeat_options,
    struct benchmark_measurement *out_measurement
);
static int run_filtered_runtime_scenario(
    const struct benchmark_options *options,
    const struct runtime_scenario *scenario,
    const char *kind,
    runtime_scenario_runner runner,
    struct runtime_repetition_options repeat_options,
    bool *inout_matched_scenario
);
static bool runtime_scenario_matches_base_filter(
    const struct benchmark_options *options,
    const struct runtime_scenario *scenario
);
static bool runtime_scenario_filter_query_index(
    const struct benchmark_options *options,
    const struct runtime_scenario *scenario,
    size_t *out_query_index
);
static bool runtime_query_name_index(
    const char *name,
    const struct runtime_scenario *scenario,
    size_t *out_query_index
);
static int run_runtime_query_samples(
    const struct runtime_scenario *scenario,
    const char *kind,
    runtime_scenario_runner runner,
    struct runtime_repetition_options repeat_options
);
static int run_runtime_query_sample_at(
    const struct runtime_scenario *scenario,
    const char *kind,
    runtime_scenario_runner runner,
    size_t query_index,
    struct runtime_repetition_options repeat_options
);
static int make_runtime_query_scenario_name(
    const struct runtime_scenario *scenario,
    size_t query_index,
    char *name,
    size_t name_size
);
static int run_repeated_runtime_scenario(
    const struct runtime_scenario *scenario,
    const char *kind,
    runtime_scenario_runner runner,
    struct runtime_repetition_options repeat_options
);
static int run_runtime_scenario(
    const struct runtime_scenario *scenario,
    const struct runtime_repetition_options *repeat_options,
    struct benchmark_measurement *out_measurement
);
static int run_runtime_cursor_scenario(
    const struct runtime_scenario *scenario,
    const struct runtime_repetition_options *repeat_options,
    struct benchmark_measurement *out_measurement
);
static int run_runtime_prepared_scenario(
    const struct runtime_scenario *scenario,
    const struct runtime_repetition_options *repeat_options,
    struct benchmark_measurement *out_measurement
);
static int run_prepared_statement_iterations(
    mylite_stmt *statement,
    const struct runtime_scenario *scenario,
    size_t iterations,
    struct benchmark_measurement *measurement
);
static int bind_prepared_statement(mylite_stmt *statement, const struct runtime_scenario *scenario);
static int step_prepared_statement(mylite_stmt *statement, size_t *out_value_bytes);
static int run_runtime_stress_scenario(
    const struct runtime_scenario *scenario,
    const struct runtime_repetition_options *repeat_options,
    struct benchmark_measurement *out_measurement
);
static int benchmark_measurement_prepare_request_latencies(
    struct benchmark_measurement *measurement,
    size_t count,
    const char *scenario_name
);
static void benchmark_measurement_deinit(struct benchmark_measurement *measurement);
static int run_runtime_query_iterations(
    mylite_db *database,
    const struct runtime_scenario *scenario,
    size_t iterations,
    bool cursor,
    struct benchmark_measurement *measurement
);
static int setup_runtime_database(mylite_db *database, const struct runtime_scenario *scenario);
static int execute_query(mylite_db *database, const struct benchmark_query *query);
static int execute_cursor_query(mylite_db *database, const struct benchmark_query *query);
static int printf_precision_from_size(size_t length);
static int make_runtime_database_path(char *path, size_t path_size, const char *scenario_name);
static int current_process_id(void);
static const char *temporary_directory(void);
static void remove_related_database_files(const char *path);
static void remove_database_file_with_suffix(const char *path, const char *suffix);
static int benchmark_lexer_queries(
    struct borrowed_query_list queries,
    size_t iterations,
    struct benchmark_measurement *out_measurement
);
static int benchmark_owned_lexer_queries(
    const struct mylite_benchmark_owned_query_list *queries,
    size_t iterations,
    struct benchmark_measurement *out_measurement
);
static int lex_query(
    const char *sql,
    size_t length,
    unsigned int modes,
    size_t *out_token_count,
    bool *out_has_error
);
static int benchmark_parse_queries(
    struct borrowed_query_list queries,
    size_t iterations,
    struct benchmark_measurement *out_measurement
);
static int benchmark_owned_parse_queries(
    const struct mylite_benchmark_owned_query_list *queries,
    size_t iterations,
    struct benchmark_measurement *out_measurement
);
static int dump_parse_failures(
    const struct mylite_benchmark_owned_query_list *queries,
    const char *path
);
static int report_expected_parse_failures(
    const struct mylite_benchmark_owned_query_list *queries,
    const char *path
);
static int classify_expected_parse_failures(
    const struct mylite_benchmark_owned_query_list *queries,
    const struct mylite_benchmark_expected_parse_failure_list *expectations,
    struct expected_parse_failure_summary *out_summary
);
static void print_parse_expectation_mismatch(
    const struct mylite_benchmark_expected_parse_failure *expectation,
    size_t query_index,
    enum mylite_sql_parse_status status,
    enum mylite_sql_token_kind token_kind
);
static void print_parse_failure_row(
    FILE *file,
    size_t query_index,
    enum mylite_sql_parse_status status,
    const struct mylite_sql_parse_result *result,
    const struct mylite_benchmark_owned_query *query
);
static struct text_coordinates compute_offset_coordinates(
    const char *text,
    size_t length,
    size_t offset
);
static void print_tsv_escaped_field(FILE *file, const char *text, size_t length);
static void record_parse_status(
    struct benchmark_measurement *measurement,
    enum mylite_sql_parse_status status
);
static uint64_t monotonic_now_ns(void);
static void print_result(
    const char *scenario,
    const char *kind,
    size_t iterations,
    size_t query_count,
    const struct benchmark_measurement *measurement
);
static void print_runtime_sample_marker(
    const char *scenario,
    const char *kind,
    size_t sample_index,
    size_t samples
);
static void print_runtime_summary(
    const char *scenario,
    const char *kind,
    size_t samples,
    const double *average_us_values
);
static void print_runtime_latency_summary(
    const char *scenario,
    const char *kind,
    const struct benchmark_measurement *measurement
);
static int initialize_profile_output(const struct benchmark_options *options);
static int append_profile_json(
    const char *path,
    const struct runtime_scenario *scenario,
    const char *kind,
    size_t sample_index,
    const struct benchmark_measurement *measurement
);
#ifdef MYLITE_ENABLE_PROFILING
static uint64_t subtract_saturating(uint64_t value, uint64_t amount);
#endif
static double sorted_percentile_value(const double *values, size_t count, size_t percentile);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): qsort fixes this callback shape.
static int compare_double_values(const void *left, const void *right);
static void print_parse_status_counts(const struct benchmark_measurement *measurement);
static double ns_to_ms(uint64_t ns);
static double ns_to_average_us(uint64_t ns, size_t operations);
static double ns_to_ops_per_second(uint64_t ns, size_t operations);

int main(int argc, char **argv) {
    struct benchmark_options options = {
        .filter = benchmark_filter_all,
        .csv_path = NULL,
        .parse_failure_dump_path = NULL,
        .expected_parse_failures_path = NULL,
        .profile_json_path = NULL,
        .runtime_scenario_name = NULL,
        .iterations = default_iterations,
        .csv_iterations = default_csv_iterations,
        .samples = default_samples,
        .warmup_iterations = default_runtime_warmup_iterations,
        .csv_replay_sql_mode = false,
        .runtime_per_query = false,
        .list_only = false,
        .show_usage = false,
    };
    int rc = parse_options(argc, argv, &options);

    if (rc != 0) {
        return rc;
    }
    if (options.show_usage) {
        print_usage(argv[0], stdout);
        return 0;
    }
    if (options.list_only) {
        print_scenario_list();
        return 0;
    }

    return run_benchmarks(&options);
}

static int parse_options(int argc, char **argv, struct benchmark_options *out_options) {
    for (int index = 1; index < argc; ++index) {
        int rc = parse_option(argc, argv, &index, argv[0], out_options);

        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

static int parse_option(
    int argc,
    char **argv,
    int *index,
    const char *program_name,
    struct benchmark_options *out_options
) {
    const char *argument = argv[*index];

    if (strcmp(argument, "--help") == 0) {
        out_options->show_usage = true;
        return 0;
    }
    if (strcmp(argument, "--list") == 0) {
        out_options->list_only = true;
        return 0;
    }
    if (strcmp(argument, "--iterations") == 0) {
        return parse_size_argument(argc, argv, index, "--iterations", &out_options->iterations);
    }
    if (strcmp(argument, "--csv-iterations") == 0) {
        return parse_size_argument(
            argc,
            argv,
            index,
            "--csv-iterations",
            &out_options->csv_iterations
        );
    }
    if (strcmp(argument, "--samples") == 0) {
        return parse_size_argument(argc, argv, index, "--samples", &out_options->samples);
    }
    if (strcmp(argument, "--warmup") == 0) {
        return parse_nonnegative_size_argument(
            argc,
            argv,
            index,
            "--warmup",
            &out_options->warmup_iterations
        );
    }
    if (strcmp(argument, "--csv") == 0) {
        return parse_text_argument(argc, argv, index, "--csv", &out_options->csv_path);
    }
    if (strcmp(argument, "--csv-replay-sql-mode") == 0) {
        out_options->csv_replay_sql_mode = true;
        return 0;
    }
    if (strcmp(argument, "--per-query") == 0) {
        out_options->runtime_per_query = true;
        return 0;
    }
    if (strcmp(argument, "--scenario") == 0) {
        return parse_text_argument(
            argc,
            argv,
            index,
            "--scenario",
            &out_options->runtime_scenario_name
        );
    }
    if (strcmp(argument, "--profile-json") == 0) {
        return parse_text_argument(
            argc,
            argv,
            index,
            "--profile-json",
            &out_options->profile_json_path
        );
    }
    if (strcmp(argument, "--dump-parse-failures") == 0) {
        return parse_text_argument(
            argc,
            argv,
            index,
            "--dump-parse-failures",
            &out_options->parse_failure_dump_path
        );
    }
    if (strcmp(argument, "--expected-parse-failures") == 0) {
        return parse_text_argument(
            argc,
            argv,
            index,
            "--expected-parse-failures",
            &out_options->expected_parse_failures_path
        );
    }
    if (strcmp(argument, "--only") == 0) {
        return parse_filter_argument(argc, argv, index, &out_options->filter);
    }

    fprintf(stderr, "unknown argument: %s\n", argument);
    print_usage(program_name, stderr);
    return 1;
}

static int parse_size_argument(
    int argc,
    char **argv,
    int *index,
    const char *option_name,
    size_t *out_value
) {
    const char *value = consume_option_value(argc, argv, index, option_name);

    if (value == NULL || parse_size_option(value, false, out_value) != 0) {
        return 1;
    }
    return 0;
}

static int parse_nonnegative_size_argument(
    int argc,
    char **argv,
    int *index,
    const char *option_name,
    size_t *out_value
) {
    const char *value = consume_option_value(argc, argv, index, option_name);

    if (value == NULL || parse_size_option(value, true, out_value) != 0) {
        return 1;
    }
    return 0;
}

static int parse_text_argument(
    int argc,
    char **argv,
    int *index,
    const char *option_name,
    const char **out_value
) {
    const char *value = consume_option_value(argc, argv, index, option_name);

    if (value == NULL) {
        return 1;
    }
    *out_value = value;
    return 0;
}

static int parse_filter_argument(
    int argc,
    char **argv,
    int *index,
    enum benchmark_filter *out_filter
) {
    const char *value = consume_option_value(argc, argv, index, "--only");

    if (value == NULL || parse_filter_option(value, out_filter) != 0) {
        return 1;
    }
    return 0;
}

static const char *consume_option_value(
    int argc,
    char **argv,
    int *index,
    const char *option_name
) {
    if (*index + 1 >= argc) {
        fprintf(stderr, "%s requires a value\n", option_name);
        return NULL;
    }
    ++*index;
    return argv[*index];
}

static int parse_filter_option(const char *text, enum benchmark_filter *out_filter) {
    if (strcmp(text, "all") == 0) {
        *out_filter = benchmark_filter_all;
        return 0;
    }
    if (strcmp(text, "lexer") == 0) {
        *out_filter = benchmark_filter_lexer;
        return 0;
    }
    if (strcmp(text, "parse") == 0) {
        *out_filter = benchmark_filter_parse;
        return 0;
    }
    if (strcmp(text, "runtime") == 0) {
        *out_filter = benchmark_filter_runtime;
        return 0;
    }

    fprintf(stderr, "--only requires one of all, lexer, parse, runtime\n");
    return 1;
}

static int parse_size_option(const char *text, bool allow_zero, size_t *out_value) {
    char *end = NULL;
    unsigned long long value = 0ULL;

    if (text == NULL || text[0] == '\0' || out_value == NULL) {
        return 1;
    }

    errno = 0;
    value = strtoull(text, &end, decimal_option_base);
    if (errno != 0 || end == text || *end != '\0' || (!allow_zero && value == 0ULL)) {
        return 1;
    }
    if (value > (unsigned long long)SIZE_MAX) {
        return 1;
    }
    *out_value = (size_t)value;
    return 0;
}

static void print_usage(const char *program_name, FILE *stream) {
    fprintf(
        stream,
        "usage: %s [--iterations N] [--samples N] [--warmup N] [--csv PATH] "
        "[--csv-iterations N] [--csv-replay-sql-mode] [--per-query] [--scenario NAME] "
        "[--profile-json PATH] "
        "[--dump-parse-failures PATH] [--expected-parse-failures PATH] "
        "[--only all|lexer|parse|runtime] [--list]\n",
        program_name
    );
}

static void print_scenario_list(void) {
    puts("lexer.wp_builtin");
    puts("parse.wp_builtin");
    print_runtime_scenario_list(
        runtime_scenarios,
        sizeof(runtime_scenarios) / sizeof(runtime_scenarios[0])
    );
    print_runtime_scenario_list(
        runtime_cursor_scenarios,
        sizeof(runtime_cursor_scenarios) / sizeof(runtime_cursor_scenarios[0])
    );
    print_runtime_scenario_list(
        runtime_prepared_scenarios,
        sizeof(runtime_prepared_scenarios) / sizeof(runtime_prepared_scenarios[0])
    );
    for (size_t index = 0U; index < mylite_benchmark_runtime_stress_scenario_count(); ++index) {
        puts(mylite_benchmark_runtime_stress_scenario_name(index));
    }
    puts("lexer.csv.mysql_server_tests");
    puts("parse.csv.mysql_server_tests");
}

static void print_runtime_scenario_list(
    const struct runtime_scenario *scenarios,
    size_t scenario_count
) {
    for (size_t scenario_index = 0U; scenario_index < scenario_count; ++scenario_index) {
        const struct runtime_scenario *scenario = &scenarios[scenario_index];

        puts(scenario->name);
        for (size_t query_index = 0U; query_index < scenario->query_count; ++query_index) {
            char query_scenario_name[runtime_query_scenario_name_capacity];

            if (make_runtime_query_scenario_name(
                    scenario,
                    query_index,
                    query_scenario_name,
                    sizeof(query_scenario_name)
                ) == 0) {
                puts(query_scenario_name);
            }
        }
    }
}

static bool filter_includes(enum benchmark_filter filter, enum benchmark_filter candidate) {
    return filter == benchmark_filter_all || filter == candidate;
}

static int run_benchmarks(const struct benchmark_options *options) {
    int rc = 0;

    if (options->parse_failure_dump_path != NULL &&
        (options->csv_path == NULL || !filter_includes(options->filter, benchmark_filter_parse))) {
        fprintf(stderr, "--dump-parse-failures requires --csv and a parse benchmark filter\n");
        return 1;
    }
    if (options->expected_parse_failures_path != NULL &&
        (options->csv_path == NULL || !filter_includes(options->filter, benchmark_filter_parse))) {
        fprintf(stderr, "--expected-parse-failures requires --csv and a parse benchmark filter\n");
        return 1;
    }
    puts("scenario,kind,iterations,queries,operations,ok,errors,tokens,bytes,total_ms,avg_us,"
         "ops_per_sec");

    if (filter_includes(options->filter, benchmark_filter_lexer)) {
        rc = run_builtin_lexer_benchmark(options);
        if (rc != 0) {
            return rc;
        }
    }
    if (filter_includes(options->filter, benchmark_filter_parse)) {
        rc = run_builtin_parse_benchmark(options);
        if (rc != 0) {
            return rc;
        }
    }
    if (filter_includes(options->filter, benchmark_filter_runtime)) {
        rc = run_runtime_benchmarks(options);
        if (rc != 0) {
            return rc;
        }
    }
    if (options->csv_path != NULL && (filter_includes(options->filter, benchmark_filter_lexer) ||
                                      filter_includes(options->filter, benchmark_filter_parse))) {
        rc = run_csv_benchmarks(options);
    }

    return rc;
}

static int run_builtin_lexer_benchmark(const struct benchmark_options *options) {
    struct benchmark_measurement measurement = {0};
    const struct borrowed_query_list queries = {
        .items = parser_wordpress_queries,
        .count = sizeof(parser_wordpress_queries) / sizeof(parser_wordpress_queries[0]),
    };
    int rc = benchmark_lexer_queries(queries, options->iterations, &measurement);

    if (rc != 0) {
        return rc;
    }
    print_result("lexer.wp_builtin", "lexer", options->iterations, queries.count, &measurement);
    return 0;
}

static int initialize_profile_output(const struct benchmark_options *options) {
    if (options->profile_json_path == NULL) {
        return 0;
    }
    if (!filter_includes(options->filter, benchmark_filter_runtime)) {
        fprintf(stderr, "--profile-json requires a runtime benchmark filter\n");
        return 1;
    }
#ifndef MYLITE_ENABLE_PROFILING
    fprintf(stderr, "--profile-json requires a MYLITE_ENABLE_PROFILING build\n");
    return 1;
#else
    FILE *file = NULL;

    file = fopen(options->profile_json_path, "wb");
    if (file == NULL) {
        fprintf(
            stderr,
            "%s: failed to create profile output: %s\n",
            options->profile_json_path,
            strerror(errno)
        );
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "%s: failed to close profile output\n", options->profile_json_path);
        return 1;
    }
    return 0;
#endif
}

static int run_builtin_parse_benchmark(const struct benchmark_options *options) {
    struct benchmark_measurement measurement = {0};
    const struct borrowed_query_list queries = {
        .items = parser_wordpress_queries,
        .count = sizeof(parser_wordpress_queries) / sizeof(parser_wordpress_queries[0]),
    };
    int rc = benchmark_parse_queries(queries, options->iterations, &measurement);

    if (rc != 0) {
        return rc;
    }
    print_result("parse.wp_builtin", "parse", options->iterations, queries.count, &measurement);
    print_parse_status_counts(&measurement);
    return 0;
}

static int run_csv_benchmarks(const struct benchmark_options *options) {
    struct mylite_benchmark_owned_query_list queries = {0};
    int rc = mylite_benchmark_load_csv_queries(options->csv_path, &queries);

    if (rc != 0) {
        return rc;
    }
    if (options->csv_replay_sql_mode) {
        rc = mylite_benchmark_assign_sql_modes(&queries);
        if (rc != 0) {
            mylite_benchmark_owned_query_list_deinit(&queries);
            return rc;
        }
    }
    if (filter_includes(options->filter, benchmark_filter_lexer)) {
        struct benchmark_measurement measurement = {0};

        rc = benchmark_owned_lexer_queries(&queries, options->csv_iterations, &measurement);
        if (rc != 0) {
            mylite_benchmark_owned_query_list_deinit(&queries);
            return rc;
        }
        print_result(
            "lexer.csv.mysql_server_tests",
            "lexer",
            options->csv_iterations,
            queries.count,
            &measurement
        );
    }
    if (filter_includes(options->filter, benchmark_filter_parse)) {
        struct benchmark_measurement measurement = {0};

        rc = benchmark_owned_parse_queries(&queries, options->csv_iterations, &measurement);
        if (rc != 0) {
            mylite_benchmark_owned_query_list_deinit(&queries);
            return rc;
        }
        print_result(
            "parse.csv.mysql_server_tests",
            "parse",
            options->csv_iterations,
            queries.count,
            &measurement
        );
        print_parse_status_counts(&measurement);
        if (options->parse_failure_dump_path != NULL) {
            rc = dump_parse_failures(&queries, options->parse_failure_dump_path);
            if (rc != 0) {
                mylite_benchmark_owned_query_list_deinit(&queries);
                return rc;
            }
        }
        if (options->expected_parse_failures_path != NULL) {
            rc = report_expected_parse_failures(&queries, options->expected_parse_failures_path);
            if (rc != 0) {
                mylite_benchmark_owned_query_list_deinit(&queries);
                return rc;
            }
        }
    }

    mylite_benchmark_owned_query_list_deinit(&queries);
    return 0;
}

static int run_runtime_benchmarks(const struct benchmark_options *options) {
    bool matched_scenario = false;
    struct runtime_repetition_options repeat_options = {
        .iterations = options->iterations,
        .samples = options->samples,
        .warmup_iterations = options->warmup_iterations,
        .profile_json_path = options->profile_json_path,
    };
    int rc = 0;

    if (!runtime_scenario_filter_exists(options)) {
        fprintf(stderr, "unknown runtime scenario: %s\n", options->runtime_scenario_name);
        return 1;
    }
    rc = initialize_profile_output(options);
    if (rc != 0) {
        return rc;
    }

    for (size_t scenario_index = 0U;
         scenario_index < sizeof(runtime_scenarios) / sizeof(runtime_scenarios[0]);
         ++scenario_index) {
        rc = run_filtered_runtime_scenario(
            options,
            &runtime_scenarios[scenario_index],
            "execute",
            run_runtime_scenario,
            repeat_options,
            &matched_scenario
        );

        if (rc != 0) {
            return rc;
        }
    }
    for (size_t scenario_index = 0U;
         scenario_index < sizeof(runtime_cursor_scenarios) / sizeof(runtime_cursor_scenarios[0]);
         ++scenario_index) {
        rc = run_filtered_runtime_scenario(
            options,
            &runtime_cursor_scenarios[scenario_index],
            "cursor",
            run_runtime_cursor_scenario,
            repeat_options,
            &matched_scenario
        );

        if (rc != 0) {
            return rc;
        }
    }
    for (size_t scenario_index = 0U; scenario_index < sizeof(runtime_prepared_scenarios) /
                                                          sizeof(runtime_prepared_scenarios[0]);
         ++scenario_index) {
        rc = run_filtered_runtime_scenario(
            options,
            &runtime_prepared_scenarios[scenario_index],
            "prepared",
            run_runtime_prepared_scenario,
            repeat_options,
            &matched_scenario
        );

        if (rc != 0) {
            return rc;
        }
    }
    for (size_t scenario_index = 0U;
         scenario_index < mylite_benchmark_runtime_stress_scenario_count();
         ++scenario_index) {
        const char *scenario_name = mylite_benchmark_runtime_stress_scenario_name(scenario_index);
        struct runtime_scenario scenario = {
            .name = scenario_name,
            .query_count = 1U,
        };

        if (options->runtime_scenario_name == NULL ||
            strcmp(options->runtime_scenario_name, scenario_name) != 0) {
            continue;
        }
        matched_scenario = true;
        if (options->profile_json_path != NULL) {
            fprintf(
                stderr,
                "%s: runtime profiling is not available for stress scenarios\n",
                scenario_name
            );
            return 1;
        }
        rc = run_repeated_runtime_scenario(
            &scenario,
            "stress",
            run_runtime_stress_scenario,
            repeat_options
        );
        if (rc != 0) {
            return rc;
        }
    }
    if (options->runtime_scenario_name != NULL && !matched_scenario) {
        fprintf(stderr, "unknown runtime scenario: %s\n", options->runtime_scenario_name);
        return 1;
    }
    return 0;
}

static bool runtime_scenario_filter_exists(const struct benchmark_options *options) {
    size_t query_index = 0U;

    if (options == NULL || options->runtime_scenario_name == NULL) {
        return true;
    }
    for (size_t scenario_index = 0U;
         scenario_index < sizeof(runtime_scenarios) / sizeof(runtime_scenarios[0]);
         ++scenario_index) {
        if (runtime_scenario_matches_base_filter(options, &runtime_scenarios[scenario_index]) ||
            runtime_scenario_filter_query_index(
                options,
                &runtime_scenarios[scenario_index],
                &query_index
            )) {
            return true;
        }
    }
    for (size_t scenario_index = 0U;
         scenario_index < sizeof(runtime_cursor_scenarios) / sizeof(runtime_cursor_scenarios[0]);
         ++scenario_index) {
        if (runtime_scenario_matches_base_filter(
                options,
                &runtime_cursor_scenarios[scenario_index]
            ) ||
            runtime_scenario_filter_query_index(
                options,
                &runtime_cursor_scenarios[scenario_index],
                &query_index
            )) {
            return true;
        }
    }
    for (size_t scenario_index = 0U; scenario_index < sizeof(runtime_prepared_scenarios) /
                                                          sizeof(runtime_prepared_scenarios[0]);
         ++scenario_index) {
        if (runtime_scenario_matches_base_filter(
                options,
                &runtime_prepared_scenarios[scenario_index]
            ) ||
            runtime_scenario_filter_query_index(
                options,
                &runtime_prepared_scenarios[scenario_index],
                &query_index
            )) {
            return true;
        }
    }
    for (size_t scenario_index = 0U;
         scenario_index < mylite_benchmark_runtime_stress_scenario_count();
         ++scenario_index) {
        if (strcmp(
                options->runtime_scenario_name,
                mylite_benchmark_runtime_stress_scenario_name(scenario_index)
            ) == 0) {
            return true;
        }
    }
    return false;
}

static int run_filtered_runtime_scenario(
    const struct benchmark_options *options,
    const struct runtime_scenario *scenario,
    const char *kind,
    runtime_scenario_runner runner,
    struct runtime_repetition_options repeat_options,
    bool *inout_matched_scenario
) {
    size_t query_index = 0U;
    bool has_query_filter = runtime_scenario_filter_query_index(options, scenario, &query_index);
    int rc = 0;

    if (!runtime_scenario_matches_base_filter(options, scenario) && !has_query_filter) {
        return 0;
    }
    *inout_matched_scenario = true;
    if (has_query_filter) {
        return run_runtime_query_sample_at(scenario, kind, runner, query_index, repeat_options);
    }

    rc = run_repeated_runtime_scenario(scenario, kind, runner, repeat_options);
    if (rc != 0) {
        return rc;
    }
    if (options->runtime_per_query) {
        return run_runtime_query_samples(scenario, kind, runner, repeat_options);
    }
    return 0;
}

static bool runtime_scenario_matches_base_filter(
    const struct benchmark_options *options,
    const struct runtime_scenario *scenario
) {
    if (options == NULL || scenario == NULL || options->runtime_scenario_name == NULL) {
        return true;
    }

    return strcmp(options->runtime_scenario_name, scenario->name) == 0;
}

static bool runtime_scenario_filter_query_index(
    const struct benchmark_options *options,
    const struct runtime_scenario *scenario,
    size_t *out_query_index
) {
    if (options == NULL || options->runtime_scenario_name == NULL) {
        return false;
    }
    return runtime_query_name_index(options->runtime_scenario_name, scenario, out_query_index);
}

static bool runtime_query_name_index(
    const char *name,
    const struct runtime_scenario *scenario,
    size_t *out_query_index
) {
    const size_t scenario_name_length = scenario == NULL ? 0U : strlen(scenario->name);
    const char query_suffix[] = ".query";
    const size_t query_suffix_length = sizeof(query_suffix) - 1U;
    const size_t name_length = name == NULL ? 0U : strlen(name);
    const char *query_text = NULL;
    char *end = NULL;
    unsigned long long query_number = 0ULL;

    if (name == NULL || scenario == NULL || out_query_index == NULL ||
        name_length <= scenario_name_length + query_suffix_length ||
        strncmp(name, scenario->name, scenario_name_length) != 0 ||
        strncmp(name + scenario_name_length, query_suffix, query_suffix_length) != 0) {
        return false;
    }
    query_text = name + scenario_name_length + query_suffix_length;
    if (query_text[0] == '\0') {
        return false;
    }

    errno = 0;
    query_number = strtoull(query_text, &end, decimal_option_base);
    if (errno != 0 || end == query_text || *end != '\0' || query_number == 0ULL ||
        query_number > (unsigned long long)scenario->query_count) {
        return false;
    }
    *out_query_index = (size_t)(query_number - 1ULL);
    return true;
}

static int run_runtime_query_samples(
    const struct runtime_scenario *scenario,
    const char *kind,
    runtime_scenario_runner runner,
    struct runtime_repetition_options repeat_options
) {
    for (size_t query_index = 0U; query_index < scenario->query_count; ++query_index) {
        if (run_runtime_query_sample_at(scenario, kind, runner, query_index, repeat_options) != 0) {
            return 1;
        }
    }
    return 0;
}

static int run_runtime_query_sample_at(
    const struct runtime_scenario *scenario,
    const char *kind,
    runtime_scenario_runner runner,
    size_t query_index,
    struct runtime_repetition_options repeat_options
) {
    char query_scenario_name[runtime_query_scenario_name_capacity];
    struct runtime_scenario query_scenario = *scenario;

    if (make_runtime_query_scenario_name(
            scenario,
            query_index,
            query_scenario_name,
            sizeof(query_scenario_name)
        ) != 0) {
        fprintf(stderr, "%s: per-query scenario name is too long\n", scenario->name);
        return 1;
    }
    query_scenario.name = query_scenario_name;
    query_scenario.queries = &scenario->queries[query_index];
    query_scenario.query_count = 1U;
    return run_repeated_runtime_scenario(&query_scenario, kind, runner, repeat_options);
}

static int make_runtime_query_scenario_name(
    const struct runtime_scenario *scenario,
    size_t query_index,
    char *name,
    size_t name_size
) {
    int written = snprintf(name, name_size, "%s.query%zu", scenario->name, query_index + 1U);

    return written < 0 || (size_t)written >= name_size ? 1 : 0;
}

static int run_repeated_runtime_scenario(
    const struct runtime_scenario *scenario,
    const char *kind,
    runtime_scenario_runner runner,
    struct runtime_repetition_options repeat_options
) {
    double *average_us_values = NULL;

    if (repeat_options.samples > 1U) {
        average_us_values = (double *)calloc(repeat_options.samples, sizeof(*average_us_values));
        if (average_us_values == NULL) {
            fprintf(stderr, "%s: failed to allocate benchmark sample summary\n", scenario->name);
            return 1;
        }
    }
    for (size_t sample_index = 0U; sample_index < repeat_options.samples; ++sample_index) {
        struct benchmark_measurement measurement = {0};
        int rc = runner(scenario, &repeat_options, &measurement);

        if (rc != 0) {
            benchmark_measurement_deinit(&measurement);
            free(average_us_values);
            return rc;
        }
        if (average_us_values != NULL) {
            average_us_values[sample_index] =
                ns_to_average_us(measurement.elapsed_ns, measurement.operations);
            print_runtime_sample_marker(scenario->name, kind, sample_index, repeat_options.samples);
        }
        print_result(
            scenario->name,
            kind,
            repeat_options.iterations,
            scenario->query_count,
            &measurement
        );
        print_runtime_latency_summary(scenario->name, kind, &measurement);
        rc = append_profile_json(
            repeat_options.profile_json_path,
            scenario,
            kind,
            sample_index,
            &measurement
        );
        if (rc != 0) {
            benchmark_measurement_deinit(&measurement);
            free(average_us_values);
            return rc;
        }
        benchmark_measurement_deinit(&measurement);
    }
    if (average_us_values != NULL) {
        print_runtime_summary(scenario->name, kind, repeat_options.samples, average_us_values);
    }
    free(average_us_values);
    return 0;
}

static int run_runtime_scenario(
    const struct runtime_scenario *scenario,
    const struct runtime_repetition_options *repeat_options,
    struct benchmark_measurement *out_measurement
) {
    mylite_db *database = NULL;
    char path[runtime_database_path_capacity];
    uint64_t started = 0U;
    uint64_t ended = 0U;
#ifdef MYLITE_ENABLE_PROFILING
    bool profile_started = false;
#endif
    int rc = make_runtime_database_path(path, sizeof(path), scenario->name);

    if (rc != 0) {
        return 1;
    }
    remove_related_database_files(path);
    rc = mylite_open(path, &database);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: failed to open benchmark database: %d\n", scenario->name, rc);
        remove_related_database_files(path);
        return 1;
    }
    if (setup_runtime_database(database, scenario) != 0) {
        mylite_close(database);
        remove_related_database_files(path);
        return 1;
    }
    if (benchmark_measurement_prepare_request_latencies(
            out_measurement,
            repeat_options->iterations,
            scenario->name
        ) != 0) {
        mylite_close(database);
        remove_related_database_files(path);
        return 1;
    }
    if (run_runtime_query_iterations(
            database,
            scenario,
            repeat_options->warmup_iterations,
            false,
            NULL
        ) != 0) {
        mylite_close(database);
        remove_related_database_files(path);
        return 1;
    }
#ifdef MYLITE_ENABLE_PROFILING
    if (repeat_options->profile_json_path != NULL) {
        rc = mylite_profile_start(database);
        if (rc != MYLITE_OK) {
            fprintf(stderr, "%s: failed to start runtime profile: %d\n", scenario->name, rc);
            mylite_close(database);
            remove_related_database_files(path);
            return 1;
        }
        profile_started = true;
    }
#endif
    started = monotonic_now_ns();
    rc = run_runtime_query_iterations(
        database,
        scenario,
        repeat_options->iterations,
        false,
        out_measurement
    );
    ended = monotonic_now_ns();
    if (rc != 0) {
#ifdef MYLITE_ENABLE_PROFILING
        if (profile_started) {
            (void)mylite_profile_stop(database, &out_measurement->profile);
        }
#endif
        mylite_close(database);
        remove_related_database_files(path);
        return 1;
    }
    out_measurement->elapsed_ns = ended - started;
#ifdef MYLITE_ENABLE_PROFILING
    if (profile_started && mylite_profile_stop(database, &out_measurement->profile) != MYLITE_OK) {
        fprintf(stderr, "%s: failed to stop runtime profile\n", scenario->name);
        mylite_close(database);
        remove_related_database_files(path);
        return 1;
    }
#endif

    mylite_close(database);
    remove_related_database_files(path);
    return 0;
}

static int run_runtime_cursor_scenario(
    const struct runtime_scenario *scenario,
    const struct runtime_repetition_options *repeat_options,
    struct benchmark_measurement *out_measurement
) {
    mylite_db *database = NULL;
    char path[runtime_database_path_capacity];
    uint64_t started = 0U;
    uint64_t ended = 0U;
#ifdef MYLITE_ENABLE_PROFILING
    bool profile_started = false;
#endif
    int rc = make_runtime_database_path(path, sizeof(path), scenario->name);

    if (rc != 0) {
        return 1;
    }
    remove_related_database_files(path);
    rc = mylite_open(path, &database);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: failed to open benchmark database: %d\n", scenario->name, rc);
        remove_related_database_files(path);
        return 1;
    }
    if (setup_runtime_database(database, scenario) != 0) {
        mylite_close(database);
        remove_related_database_files(path);
        return 1;
    }
    if (benchmark_measurement_prepare_request_latencies(
            out_measurement,
            repeat_options->iterations,
            scenario->name
        ) != 0) {
        mylite_close(database);
        remove_related_database_files(path);
        return 1;
    }
    if (run_runtime_query_iterations(
            database,
            scenario,
            repeat_options->warmup_iterations,
            true,
            NULL
        ) != 0) {
        mylite_close(database);
        remove_related_database_files(path);
        return 1;
    }
#ifdef MYLITE_ENABLE_PROFILING
    if (repeat_options->profile_json_path != NULL) {
        rc = mylite_profile_start(database);
        if (rc != MYLITE_OK) {
            fprintf(stderr, "%s: failed to start cursor profile: %d\n", scenario->name, rc);
            mylite_close(database);
            remove_related_database_files(path);
            return 1;
        }
        profile_started = true;
    }
#endif
    started = monotonic_now_ns();
    rc = run_runtime_query_iterations(
        database,
        scenario,
        repeat_options->iterations,
        true,
        out_measurement
    );
    ended = monotonic_now_ns();
    if (rc != 0) {
#ifdef MYLITE_ENABLE_PROFILING
        if (profile_started) {
            (void)mylite_profile_stop(database, &out_measurement->profile);
        }
#endif
        mylite_close(database);
        remove_related_database_files(path);
        return 1;
    }
    out_measurement->elapsed_ns = ended - started;
#ifdef MYLITE_ENABLE_PROFILING
    if (profile_started && mylite_profile_stop(database, &out_measurement->profile) != MYLITE_OK) {
        fprintf(stderr, "%s: failed to stop cursor profile\n", scenario->name);
        mylite_close(database);
        remove_related_database_files(path);
        return 1;
    }
#endif

    mylite_close(database);
    remove_related_database_files(path);
    return 0;
}

static int run_runtime_prepared_scenario(
    const struct runtime_scenario *scenario,
    const struct runtime_repetition_options *repeat_options,
    struct benchmark_measurement *out_measurement
) {
    mylite_db *database = NULL;
    mylite_stmt *statement = NULL;
    char path[runtime_database_path_capacity];
    uint64_t started = 0U;
    uint64_t ended = 0U;
#ifdef MYLITE_ENABLE_PROFILING
    bool profile_started = false;
#endif
    int rc = make_runtime_database_path(path, sizeof(path), scenario->name);
    int result = 1;

    if (rc != 0 || scenario->query_count != 1U) {
        return 1;
    }
    remove_related_database_files(path);
    rc = mylite_open(path, &database);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: failed to open benchmark database: %d\n", scenario->name, rc);
        goto cleanup;
    }
    if (setup_runtime_database(database, scenario) != 0) {
        goto cleanup;
    }
    rc =
        mylite_prepare(database, scenario->queries[0].sql, scenario->queries[0].length, &statement);
    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: prepared benchmark failed to prepare: rc=%d err=%d state=%s message=%s\n",
            scenario->name,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        goto cleanup;
    }
    if (mylite_stmt_parameter_count(statement) != scenario->prepared_binding_count) {
        fprintf(stderr, "%s: prepared benchmark binding count mismatch\n", scenario->name);
        goto cleanup;
    }
    if (benchmark_measurement_prepare_request_latencies(
            out_measurement,
            repeat_options->iterations,
            scenario->name
        ) != 0 ||
        run_prepared_statement_iterations(
            statement,
            scenario,
            repeat_options->warmup_iterations,
            NULL
        ) != 0) {
        goto cleanup;
    }
#ifdef MYLITE_ENABLE_PROFILING
    if (repeat_options->profile_json_path != NULL) {
        rc = mylite_profile_start(database);
        if (rc != MYLITE_OK) {
            fprintf(stderr, "%s: failed to start prepared profile: %d\n", scenario->name, rc);
            goto cleanup;
        }
        profile_started = true;
    }
#endif
    started = monotonic_now_ns();
    rc = run_prepared_statement_iterations(
        statement,
        scenario,
        repeat_options->iterations,
        out_measurement
    );
    ended = monotonic_now_ns();
    if (rc != 0) {
        goto cleanup;
    }
    out_measurement->elapsed_ns = ended - started;
#ifdef MYLITE_ENABLE_PROFILING
    if (profile_started) {
        if (mylite_profile_stop(database, &out_measurement->profile) != MYLITE_OK) {
            fprintf(stderr, "%s: failed to stop prepared profile\n", scenario->name);
            goto cleanup;
        }
        profile_started = false;
    }
#endif
    result = 0;

cleanup:
#ifdef MYLITE_ENABLE_PROFILING
    if (profile_started) {
        (void)mylite_profile_stop(database, &out_measurement->profile);
    }
#endif
    if (mylite_stmt_finalize(statement) != MYLITE_OK) {
        result = 1;
    }
    mylite_close(database);
    remove_related_database_files(path);
    return result;
}

static int run_prepared_statement_iterations(
    mylite_stmt *statement,
    const struct runtime_scenario *scenario,
    size_t iterations,
    struct benchmark_measurement *measurement
) {
    const struct benchmark_query *query = &scenario->queries[0];

    for (size_t iteration = 0U; iteration < iterations; ++iteration) {
        uint64_t request_started = measurement == NULL ? 0U : monotonic_now_ns();
        size_t value_bytes = 0U;
        int rc = mylite_stmt_reset(statement);

        if (rc == MYLITE_OK) {
            rc = bind_prepared_statement(statement, scenario);
        }
        if (rc == MYLITE_OK) {
            rc = step_prepared_statement(statement, &value_bytes);
        }
        if (measurement != NULL) {
            ++measurement->operations;
            measurement->bytes += query->length;
            if (rc == MYLITE_DONE) {
                ++measurement->ok_count;
            } else {
                ++measurement->error_count;
            }
        }
        if (rc != MYLITE_DONE) {
            fprintf(stderr, "%s: prepared benchmark execution failed: rc=%d\n", scenario->name, rc);
            return 1;
        }
        if (measurement != NULL &&
            measurement->request_latency_count < measurement->request_latency_capacity) {
            measurement->request_latency_ns[measurement->request_latency_count] =
                monotonic_now_ns() - request_started;
            ++measurement->request_latency_count;
        }
        (void)value_bytes;
    }
    return 0;
}

static int bind_prepared_statement(
    mylite_stmt *statement,
    const struct runtime_scenario *scenario
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; index < scenario->prepared_binding_count && rc == MYLITE_OK; ++index) {
        const struct benchmark_query *binding = &scenario->prepared_bindings[index];

        rc = mylite_stmt_bind_text(statement, index, binding->sql, binding->length);
    }
    return rc;
}

static int step_prepared_statement(mylite_stmt *statement, size_t *out_value_bytes) {
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK || rc == MYLITE_ROW) {
        rc = mylite_stmt_step(statement);
        if (rc == MYLITE_ROW) {
            size_t column_count = mylite_stmt_column_count(statement);

            for (size_t column = 0U; column < column_count; ++column) {
                *out_value_bytes += mylite_stmt_value_size(statement, column);
            }
        }
    }
    return rc;
}

static int run_runtime_stress_scenario(
    const struct runtime_scenario *scenario,
    const struct runtime_repetition_options *repeat_options,
    struct benchmark_measurement *out_measurement
) {
    struct mylite_benchmark_runtime_stress_measurement measurement = {0};
    int rc = mylite_benchmark_run_runtime_stress_scenario(
        scenario->name,
        repeat_options->iterations,
        repeat_options->warmup_iterations,
        &measurement
    );

    out_measurement->elapsed_ns = measurement.elapsed_ns;
    out_measurement->operations = measurement.operations;
    out_measurement->bytes = measurement.bytes;
    out_measurement->ok_count = measurement.ok_count;
    out_measurement->error_count = measurement.error_count;
    if (measurement.peak_retained_bytes != 0U) {
        fprintf(
            stderr,
            "%s: peak_retained_bytes=%zu\n",
            scenario->name,
            measurement.peak_retained_bytes
        );
    }
    if (rc != 0) {
        fprintf(stderr, "%s: stress benchmark failed\n", scenario->name);
    }
    return rc;
}

static int benchmark_measurement_prepare_request_latencies(
    struct benchmark_measurement *measurement,
    size_t count,
    const char *scenario_name
) {
    if (measurement == NULL || count == 0U) {
        return 0;
    }
    if (count > SIZE_MAX / sizeof(*measurement->request_latency_ns)) {
        fprintf(stderr, "%s: request-latency allocation is too large\n", scenario_name);
        return 1;
    }
    measurement->request_latency_ns =
        (uint64_t *)calloc(count, sizeof(*measurement->request_latency_ns));
    if (measurement->request_latency_ns == NULL) {
        fprintf(stderr, "%s: failed to allocate request-latency samples\n", scenario_name);
        return 1;
    }
    measurement->request_latency_capacity = count;
    return 0;
}

static void benchmark_measurement_deinit(struct benchmark_measurement *measurement) {
    if (measurement == NULL) {
        return;
    }
    free(measurement->request_latency_ns);
    measurement->request_latency_ns = NULL;
    measurement->request_latency_count = 0U;
    measurement->request_latency_capacity = 0U;
}

static int run_runtime_query_iterations(
    mylite_db *database,
    const struct runtime_scenario *scenario,
    size_t iterations,
    bool cursor,
    struct benchmark_measurement *measurement
) {
    for (size_t iteration = 0U; iteration < iterations; ++iteration) {
        uint64_t request_started = measurement == NULL ? 0U : monotonic_now_ns();

        for (size_t query_index = 0U; query_index < scenario->query_count; ++query_index) {
            const struct benchmark_query *query = &scenario->queries[query_index];
            int rc =
                cursor ? execute_cursor_query(database, query) : execute_query(database, query);

            if (measurement != NULL) {
                ++measurement->operations;
                measurement->bytes += query->length;
                if (rc == 0) {
                    ++measurement->ok_count;
                } else {
                    ++measurement->error_count;
                }
            }
            if (rc != 0) {
                return 1;
            }
        }
        if (measurement != NULL &&
            measurement->request_latency_count < measurement->request_latency_capacity) {
            measurement->request_latency_ns[measurement->request_latency_count] =
                monotonic_now_ns() - request_started;
            ++measurement->request_latency_count;
        }
    }
    return 0;
}

static int setup_runtime_database(mylite_db *database, const struct runtime_scenario *scenario) {
    for (size_t query_index = 0U; query_index < scenario->setup_query_count; ++query_index) {
        if (execute_query(database, &scenario->setup_queries[query_index]) != 0) {
            fprintf(stderr, "%s: setup query %zu failed\n", scenario->name, query_index + 1U);
            return 1;
        }
    }
    for (size_t query_index = 0U; query_index < scenario->additional_setup_query_count;
         ++query_index) {
        if (execute_query(database, &scenario->additional_setup_queries[query_index]) != 0) {
            fprintf(
                stderr,
                "%s: additional setup query %zu failed\n",
                scenario->name,
                query_index + 1U
            );
            return 1;
        }
    }
    return 0;
}

static int execute_query(mylite_db *database, const struct benchmark_query *query) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query->sql, query->length, &result);

    if (rc != MYLITE_OK) {
        int display_length = printf_precision_from_size(query->length);

        fprintf(
            stderr,
            "query failed: rc=%d err=%d state=%s message=%s\n",
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        fprintf(stderr, "sql: %.*s\n", display_length, query->sql);
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int execute_cursor_query(mylite_db *database, const struct benchmark_query *query) {
    mylite_stmt *stmt = NULL;
    size_t value_bytes = 0U;
    int rc = mylite_prepare(database, query->sql, query->length, &stmt);

    if (rc != MYLITE_OK) {
        int display_length = printf_precision_from_size(query->length);

        fprintf(
            stderr,
            "cursor prepare failed: rc=%d err=%d state=%s message=%s\n",
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        fprintf(stderr, "sql: %.*s\n", display_length, query->sql);
        return 1;
    }
    for (;;) {
        rc = mylite_stmt_step(stmt);
        if (rc == MYLITE_ROW) {
            size_t column_count = mylite_stmt_column_count(stmt);

            for (size_t column = 0U; column < column_count; ++column) {
                value_bytes += mylite_stmt_value_size(stmt, column);
            }
            continue;
        }
        if (rc == MYLITE_DONE) {
            break;
        }
        {
            int display_length = printf_precision_from_size(query->length);

            fprintf(
                stderr,
                "cursor step failed: rc=%d err=%d state=%s message=%s\n",
                rc,
                mylite_errcode(database),
                mylite_sqlstate(database),
                mylite_errmsg(database)
            );
            fprintf(stderr, "sql: %.*s\n", display_length, query->sql);
        }
        (void)mylite_stmt_finalize(stmt);
        return 1;
    }
    rc = mylite_stmt_finalize(stmt);
    if (rc != MYLITE_OK) {
        int display_length = printf_precision_from_size(query->length);

        fprintf(
            stderr,
            "cursor finalize failed: rc=%d err=%d state=%s message=%s\n",
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        fprintf(stderr, "sql: %.*s\n", display_length, query->sql);
        return 1;
    }
    (void)value_bytes;
    return 0;
}

static int printf_precision_from_size(size_t length) {
    if (length > (size_t)INT_MAX) {
        return INT_MAX;
    }
    return (int)length;
}

static int make_runtime_database_path(char *path, size_t path_size, const char *scenario_name) {
    const char *directory = temporary_directory();
#if defined(_WIN32)
    const char separator = '\\';
#else
    const char separator = '/';
#endif
    int written = snprintf(
        path,
        path_size,
        "%s%cmylite_benchmark_%d_%s.mylite",
        directory,
        separator,
        current_process_id(),
        scenario_name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "%s: benchmark database path is too long\n", scenario_name);
        return 1;
    }
    return 0;
}

static int current_process_id(void) {
#if defined(_WIN32)
    return (int)GetCurrentProcessId();
#else
    return getpid();
#endif
}

static const char *temporary_directory(void) {
    const char *directory = getenv("TMPDIR");

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
#if defined(_WIN32)
        directory = ".";
#else
        directory = "/tmp";
#endif
    }
    return directory;
}

static void remove_related_database_files(const char *path) {
    remove_database_file_with_suffix(path, "");
    remove_database_file_with_suffix(path, "-journal");
    remove_database_file_with_suffix(path, "-wal");
    remove_database_file_with_suffix(path, "-shm");
}

static void remove_database_file_with_suffix(const char *path, const char *suffix) {
    char related_path[runtime_database_path_capacity + runtime_database_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int benchmark_lexer_queries(
    struct borrowed_query_list queries,
    size_t iterations,
    struct benchmark_measurement *out_measurement
) {
    uint64_t started = monotonic_now_ns();

    for (size_t iteration = 0U; iteration < iterations; ++iteration) {
        for (size_t query_index = 0U; query_index < queries.count; ++query_index) {
            const struct benchmark_query *query = &queries.items[query_index];
            bool has_error = false;
            size_t token_count = 0U;
            int rc = lex_query(query->sql, query->length, 0U, &token_count, &has_error);

            if (rc != 0) {
                return rc;
            }
            ++out_measurement->operations;
            out_measurement->bytes += query->length;
            out_measurement->token_count += token_count;
            if (has_error) {
                ++out_measurement->error_count;
            } else {
                ++out_measurement->ok_count;
            }
        }
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
    return 0;
}

static int benchmark_owned_lexer_queries(
    const struct mylite_benchmark_owned_query_list *queries,
    size_t iterations,
    struct benchmark_measurement *out_measurement
) {
    uint64_t started = monotonic_now_ns();

    for (size_t iteration = 0U; iteration < iterations; ++iteration) {
        for (size_t query_index = 0U; query_index < queries->count; ++query_index) {
            const struct mylite_benchmark_owned_query *query = &queries->items[query_index];
            bool has_error = false;
            size_t token_count = 0U;
            int rc = lex_query(query->sql, query->length, query->modes, &token_count, &has_error);

            if (rc != 0) {
                return rc;
            }
            ++out_measurement->operations;
            out_measurement->bytes += query->length;
            out_measurement->token_count += token_count;
            if (has_error) {
                ++out_measurement->error_count;
            } else {
                ++out_measurement->ok_count;
            }
        }
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
    return 0;
}

static int lex_query(
    const char *sql,
    size_t length,
    unsigned int modes,
    size_t *out_token_count,
    bool *out_has_error
) {
    struct mylite_sql_lexer lexer = {0};
    struct mylite_sql_token token = {0};

    *out_token_count = 0U;
    *out_has_error = false;
    mylite_sql_lexer_init(
        &lexer,
        (struct mylite_sql_lexer_config){
            .input = sql,
            .length = length,
            .modes = modes,
        }
    );
    for (;;) {
        if (mylite_sql_lexer_next(&lexer, &token) != 0) {
            return 1;
        }
        if (token.kind == MYLITE_SQL_TOKEN_ERROR) {
            *out_has_error = true;
        }
        if (token.kind == MYLITE_SQL_TOKEN_EOF) {
            break;
        }
        ++*out_token_count;
    }
    return 0;
}

static int benchmark_parse_queries(
    struct borrowed_query_list queries,
    size_t iterations,
    struct benchmark_measurement *out_measurement
) {
    uint64_t started = monotonic_now_ns();

    for (size_t iteration = 0U; iteration < iterations; ++iteration) {
        for (size_t query_index = 0U; query_index < queries.count; ++query_index) {
            struct mylite_sql_parse_result result = {0};
            const struct benchmark_query *query = &queries.items[query_index];
            enum mylite_sql_parse_status status = mylite_sql_parse(
                (struct mylite_sql_parse_config){
                    .input = query->sql,
                    .length = query->length,
                    .modes = 0U,
                },
                &result
            );

            ++out_measurement->operations;
            out_measurement->bytes += query->length;
            record_parse_status(out_measurement, status);
            mylite_sql_parse_result_deinit(&result);
        }
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
    return 0;
}

static int benchmark_owned_parse_queries(
    const struct mylite_benchmark_owned_query_list *queries,
    size_t iterations,
    struct benchmark_measurement *out_measurement
) {
    uint64_t started = monotonic_now_ns();

    for (size_t iteration = 0U; iteration < iterations; ++iteration) {
        for (size_t query_index = 0U; query_index < queries->count; ++query_index) {
            struct mylite_sql_parse_result result = {0};
            const struct mylite_benchmark_owned_query *query = &queries->items[query_index];
            enum mylite_sql_parse_status status = mylite_sql_parse(
                (struct mylite_sql_parse_config){
                    .input = query->sql,
                    .length = query->length,
                    .modes = query->modes,
                },
                &result
            );

            ++out_measurement->operations;
            out_measurement->bytes += query->length;
            record_parse_status(out_measurement, status);
            mylite_sql_parse_result_deinit(&result);
        }
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
    return 0;
}

static int dump_parse_failures(
    const struct mylite_benchmark_owned_query_list *queries,
    const char *path
) {
    FILE *file = fopen(path, "wb");

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open parse failure dump: %s\n", path, strerror(errno));
        return 1;
    }
    for (size_t query_index = 0U; query_index < queries->count; ++query_index) {
        struct mylite_sql_parse_result result = {0};
        const struct mylite_benchmark_owned_query *query = &queries->items[query_index];
        enum mylite_sql_parse_status status = mylite_sql_parse(
            (struct mylite_sql_parse_config){
                .input = query->sql,
                .length = query->length,
                .modes = query->modes,
            },
            &result
        );

        if (status != MYLITE_SQL_PARSE_OK) {
            print_parse_failure_row(file, query_index + 1U, status, &result, query);
        }
        mylite_sql_parse_result_deinit(&result);
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "%s: failed to close parse failure dump\n", path);
        return 1;
    }
    return 0;
}

static int report_expected_parse_failures(
    const struct mylite_benchmark_owned_query_list *queries,
    const char *path
) {
    struct mylite_benchmark_expected_parse_failure_list expectations = {0};
    struct expected_parse_failure_summary summary = {0};
    int rc = mylite_benchmark_load_expected_parse_failures(path, &expectations);

    if (rc != 0) {
        return rc;
    }
    rc = classify_expected_parse_failures(queries, &expectations, &summary);
    printf(
        "# expected_parse_failures total=%zu matched=%zu unexpected=%zu mismatched=%zu "
        "missing=%zu\n",
        summary.total_count,
        summary.matched_count,
        summary.unexpected_count,
        summary.mismatched_count,
        summary.missing_count
    );
    mylite_benchmark_expected_parse_failure_list_deinit(&expectations);
    return rc;
}

static int classify_expected_parse_failures(
    const struct mylite_benchmark_owned_query_list *queries,
    const struct mylite_benchmark_expected_parse_failure_list *expectations,
    struct expected_parse_failure_summary *out_summary
) {
    bool *seen = NULL;

    out_summary->total_count = expectations->count;
    if (expectations->count > 0U) {
        seen = (bool *)calloc(expectations->count, sizeof(*seen));
        if (seen == NULL) {
            fprintf(stderr, "out of memory while classifying expected parse failures\n");
            return 1;
        }
    }
    for (size_t query_index = 0U; query_index < queries->count; ++query_index) {
        struct mylite_sql_parse_result result = {0};
        const struct mylite_benchmark_owned_query *query = &queries->items[query_index];
        size_t one_based_query_index = query_index + 1U;
        const struct mylite_benchmark_expected_parse_failure *expectation =
            mylite_benchmark_expected_parse_failure_find(expectations, one_based_query_index);
        enum mylite_sql_parse_status status = mylite_sql_parse(
            (struct mylite_sql_parse_config){
                .input = query->sql,
                .length = query->length,
                .modes = query->modes,
            },
            &result
        );

        if (status != MYLITE_SQL_PARSE_OK) {
            if (expectation == NULL || seen == NULL) {
                ++out_summary->unexpected_count;
                print_parse_failure_row(stderr, one_based_query_index, status, &result, query);
            } else {
                size_t expectation_index = (size_t)(expectation - expectations->items);

                seen[expectation_index] = true;
                if (mylite_benchmark_expected_parse_failure_matches(
                        expectation,
                        status,
                        result.error_token.kind
                    )) {
                    ++out_summary->matched_count;
                } else {
                    ++out_summary->mismatched_count;
                    print_parse_expectation_mismatch(
                        expectation,
                        one_based_query_index,
                        status,
                        result.error_token.kind
                    );
                }
            }
        }
        mylite_sql_parse_result_deinit(&result);
    }
    for (size_t index = 0U; index < expectations->count; ++index) {
        if (!seen[index]) {
            ++out_summary->missing_count;
            fprintf(
                stderr,
                "expected parse failure is now missing: query=%zu status=%s token=%s reason=%s\n",
                expectations->items[index].query_index,
                expectations->items[index].status_name,
                expectations->items[index].token_kind_name,
                expectations->items[index].reason
            );
        }
    }
    free(seen);
    return out_summary->unexpected_count == 0U && out_summary->mismatched_count == 0U &&
                   out_summary->missing_count == 0U
               ? 0
               : 1;
}

static void print_parse_expectation_mismatch(
    const struct mylite_benchmark_expected_parse_failure *expectation,
    size_t query_index,
    enum mylite_sql_parse_status status,
    enum mylite_sql_token_kind token_kind
) {
    fprintf(
        stderr,
        "expected parse failure mismatch: query=%zu expected=%s/%s actual=%s/%s reason=%s\n",
        query_index,
        expectation->status_name,
        expectation->token_kind_name,
        mylite_sql_parse_status_name(status),
        mylite_sql_token_kind_name(token_kind),
        expectation->reason
    );
}

static void print_parse_failure_row(
    FILE *file,
    size_t query_index,
    enum mylite_sql_parse_status status,
    const struct mylite_sql_parse_result *result,
    const struct mylite_benchmark_owned_query *query
) {
    struct text_coordinates coordinates =
        compute_offset_coordinates(query->sql, query->length, result->error_token.offset);

    fprintf(
        file,
        "%zu\t%s\t%s\t%zu\t%u\t%u\t",
        query_index,
        mylite_sql_parse_status_name(status),
        mylite_sql_token_kind_name(result->error_token.kind),
        result->error_token.offset,
        coordinates.line,
        coordinates.column
    );
    print_tsv_escaped_field(file, result->error_token.text, result->error_token.length);
    fputc('\t', file);
    print_tsv_escaped_field(file, query->sql, query->length);
    fputc('\n', file);
}

static struct text_coordinates compute_offset_coordinates(
    const char *text,
    size_t length,
    size_t offset
) {
    struct text_coordinates coordinates = {
        .line = 1U,
        .column = 1U,
    };
    size_t limit = offset < length ? offset : length;

    for (size_t index = 0U; text != NULL && index < limit; ++index) {
        if (text[index] == '\r' ||
            (text[index] == '\n' && (index == 0U || text[index - 1U] != '\r'))) {
            ++coordinates.line;
            coordinates.column = 1U;
        } else {
            ++coordinates.column;
        }
    }

    return coordinates;
}

static void print_tsv_escaped_field(FILE *file, const char *text, size_t length) {
    if (text == NULL) {
        return;
    }
    for (size_t index = 0U; index < length; ++index) {
        switch (text[index]) {
        case '\\':
            fputs("\\\\", file);
            break;
        case '\t':
            fputs("\\t", file);
            break;
        case '\n':
            fputs("\\n", file);
            break;
        case '\r':
            fputs("\\r", file);
            break;
        default:
            fputc((unsigned char)text[index], file);
            break;
        }
    }
}

static void record_parse_status(
    struct benchmark_measurement *measurement,
    enum mylite_sql_parse_status status
) {
    if (status == MYLITE_SQL_PARSE_OK) {
        ++measurement->ok_count;
    } else {
        ++measurement->error_count;
    }
    if (status >= 0 && status <= MYLITE_SQL_PARSE_STACK_OVERFLOW) {
        ++measurement->parse_status_counts[(size_t)status];
    }
}

static uint64_t monotonic_now_ns(void) {
#if defined(_WIN32)
    LARGE_INTEGER frequency = {0};
    LARGE_INTEGER counter = {0};

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * (LONGLONG)nanoseconds_per_second) / frequency.QuadPart);
#elif defined(CLOCK_MONOTONIC)
    struct timespec timestamp = {0};

    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    return ((uint64_t)timestamp.tv_sec * nanoseconds_per_second) + (uint64_t)timestamp.tv_nsec;
#else
    struct timespec timestamp = {0};

    timespec_get(&timestamp, TIME_UTC);
    return ((uint64_t)timestamp.tv_sec * nanoseconds_per_second) + (uint64_t)timestamp.tv_nsec;
#endif
}

static void print_result(
    const char *scenario,
    const char *kind,
    size_t iterations,
    size_t query_count,
    const struct benchmark_measurement *measurement
) {
    printf(
        "%s,%s,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%.3f,%.3f,%.3f\n",
        scenario,
        kind,
        iterations,
        query_count,
        measurement->operations,
        measurement->ok_count,
        measurement->error_count,
        measurement->token_count,
        measurement->bytes,
        ns_to_ms(measurement->elapsed_ns),
        ns_to_average_us(measurement->elapsed_ns, measurement->operations),
        ns_to_ops_per_second(measurement->elapsed_ns, measurement->operations)
    );
}

static void print_runtime_sample_marker(
    const char *scenario,
    const char *kind,
    size_t sample_index,
    size_t samples
) {
    printf(
        "# sample scenario=%s kind=%s index=%zu/%zu\n",
        scenario,
        kind,
        sample_index + 1U,
        samples
    );
}

static void print_runtime_summary(
    const char *scenario,
    const char *kind,
    size_t samples,
    const double *average_us_values
) {
    double *sorted_values = NULL;

    if (samples == 0U) {
        return;
    }
    sorted_values = (double *)malloc(samples * sizeof(*sorted_values));
    if (sorted_values == NULL) {
        fprintf(stderr, "%s: failed to allocate benchmark summary sorting buffer\n", scenario);
        return;
    }
    memcpy(sorted_values, average_us_values, samples * sizeof(*sorted_values));
    qsort(sorted_values, samples, sizeof(*sorted_values), compare_double_values);
    printf(
        "# summary scenario=%s kind=%s samples=%zu min_avg_us=%.3f median_avg_us=%.3f "
        "p95_avg_us=%.3f max_avg_us=%.3f\n",
        scenario,
        kind,
        samples,
        sorted_values[0],
        sorted_percentile_value(sorted_values, samples, percentile_median),
        sorted_percentile_value(sorted_values, samples, percentile_p95),
        sorted_values[samples - 1U]
    );
    free(sorted_values);
}

static void print_runtime_latency_summary(
    const char *scenario,
    const char *kind,
    const struct benchmark_measurement *measurement
) {
    double *sorted_values = NULL;

    if (measurement->request_latency_count == 0U) {
        return;
    }
    sorted_values = (double *)malloc(measurement->request_latency_count * sizeof(*sorted_values));
    if (sorted_values == NULL) {
        fprintf(stderr, "%s: failed to allocate request-latency sorting buffer\n", scenario);
        return;
    }
    for (size_t index = 0U; index < measurement->request_latency_count; ++index) {
        sorted_values[index] =
            (double)measurement->request_latency_ns[index] / (double)nanoseconds_per_microsecond;
    }
    qsort(
        sorted_values,
        measurement->request_latency_count,
        sizeof(*sorted_values),
        compare_double_values
    );
    printf(
        "# latency scenario=%s kind=%s requests=%zu p50_us=%.3f p95_us=%.3f p99_us=%.3f "
        "max_us=%.3f\n",
        scenario,
        kind,
        measurement->request_latency_count,
        sorted_percentile_value(
            sorted_values,
            measurement->request_latency_count,
            percentile_median
        ),
        sorted_percentile_value(sorted_values, measurement->request_latency_count, percentile_p95),
        sorted_percentile_value(sorted_values, measurement->request_latency_count, percentile_p99),
        sorted_values[measurement->request_latency_count - 1U]
    );
    free(sorted_values);
}

static int append_profile_json(
    const char *path,
    const struct runtime_scenario *scenario,
    const char *kind,
    size_t sample_index,
    const struct benchmark_measurement *measurement
) {
    if (path == NULL) {
        return 0;
    }
#ifndef MYLITE_ENABLE_PROFILING
    (void)scenario;
    (void)kind;
    (void)sample_index;
    (void)measurement;
    return 1;
#else
    const struct mylite_profile_snapshot *profile = &measurement->profile;
    uint64_t profiled_library_ns =
        profile->statement_api_ns + profile->cursor_step_ns + profile->cursor_finalize_ns;
    uint64_t unattributed_ns = profiled_library_ns;
    FILE *file = NULL;

    unattributed_ns = subtract_saturating(unattributed_ns, profile->normalization_ns);
    unattributed_ns = subtract_saturating(unattributed_ns, profile->parse_ns);
    unattributed_ns = subtract_saturating(unattributed_ns, profile->select_plan_ns);
    unattributed_ns = subtract_saturating(unattributed_ns, profile->select_lowering_ns);
    unattributed_ns = subtract_saturating(unattributed_ns, profile->sqlite_step_ns);
    unattributed_ns = subtract_saturating(unattributed_ns, profile->result_buffer_ns);
    file = fopen(path, "ab");
    if (file == NULL) {
        fprintf(stderr, "%s: failed to append profile output: %s\n", path, strerror(errno));
        return 1;
    }
    fprintf(
        file,
        "{\"scenario\":\"%s\",\"kind\":\"%s\",\"sample\":%zu,"
        "\"operations\":%zu,\"wall_ns\":%" PRIu64 ",\"statement_api_ns\":%" PRIu64
        ",\"normalization_ns\":%" PRIu64 ",\"parse_ns\":%" PRIu64 ",\"select_plan_ns\":%" PRIu64
        ",\"select_lowering_ns\":%" PRIu64 ",\"sqlite_step_ns\":%" PRIu64
        ",\"metadata_step_ns\":%" PRIu64 ",\"result_buffer_ns\":%" PRIu64
        ",\"cursor_step_ns\":%" PRIu64 ",\"cursor_finalize_ns\":%" PRIu64
        ",\"unattributed_ns\":%" PRIu64 ",\"statement_count\":%" PRIu64
        ",\"normalization_count\":%" PRIu64 ",\"parse_count\":%" PRIu64
        ",\"select_plan_count\":%" PRIu64 ",\"parser_retry_callback_count\":%" PRIu64
        ",\"parser_retry_handled_count\":%" PRIu64 ",\"select_plan_cache_hit_count\":%" PRIu64
        ",\"select_lowering_count\":%" PRIu64 ",\"select_lowering_cache_hit_count\":%" PRIu64
        ",\"sqlite_step_count\":%" PRIu64 ",\"metadata_step_count\":%" PRIu64
        ",\"allocation_count\":%" PRIu64 ",\"allocation_bytes\":%" PRIu64
        ",\"descriptor_copy_count\":%" PRIu64 ",\"descriptor_copy_bytes\":%" PRIu64
        ",\"execution_statement_cache_hit_count\":%" PRIu64
        ",\"execution_statement_cache_miss_count\":%" PRIu64
        ",\"execution_statement_cache_eviction_count\":%" PRIu64
        ",\"execution_statement_cache_uncached_prepare_count\":%" PRIu64
        ",\"catalog_statement_cache_hit_count\":%" PRIu64
        ",\"catalog_statement_cache_miss_count\":%" PRIu64
        ",\"catalog_statement_cache_uncached_prepare_count\":%" PRIu64
        ",\"result_row_count\":%" PRIu64 ",\"result_value_bytes\":%" PRIu64
        ",\"cursor_row_count\":%" PRIu64 ",\"cursor_value_bytes\":%" PRIu64
        ",\"cursor_finalize_count\":%" PRIu64 "}\n",
        scenario->name,
        kind,
        sample_index + 1U,
        measurement->operations,
        measurement->elapsed_ns,
        profile->statement_api_ns,
        profile->normalization_ns,
        profile->parse_ns,
        profile->select_plan_ns,
        profile->select_lowering_ns,
        profile->sqlite_step_ns,
        profile->metadata_step_ns,
        profile->result_buffer_ns,
        profile->cursor_step_ns,
        profile->cursor_finalize_ns,
        unattributed_ns,
        profile->statement_count,
        profile->normalization_count,
        profile->parse_count,
        profile->select_plan_count,
        profile->parser_retry_callback_count,
        profile->parser_retry_handled_count,
        profile->select_plan_cache_hit_count,
        profile->select_lowering_count,
        profile->select_lowering_cache_hit_count,
        profile->sqlite_step_count,
        profile->metadata_step_count,
        profile->allocation_count,
        profile->allocation_bytes,
        profile->descriptor_copy_count,
        profile->descriptor_copy_bytes,
        profile->execution_statement_cache_hit_count,
        profile->execution_statement_cache_miss_count,
        profile->execution_statement_cache_eviction_count,
        profile->execution_statement_cache_uncached_prepare_count,
        profile->catalog_statement_cache_hit_count,
        profile->catalog_statement_cache_miss_count,
        profile->catalog_statement_cache_uncached_prepare_count,
        profile->result_row_count,
        profile->result_value_bytes,
        profile->cursor_row_count,
        profile->cursor_value_bytes,
        profile->cursor_finalize_count
    );
    if (fclose(file) != 0) {
        fprintf(stderr, "%s: failed to close profile output\n", path);
        return 1;
    }
    return 0;
#endif
}

#ifdef MYLITE_ENABLE_PROFILING
static uint64_t subtract_saturating(uint64_t value, uint64_t amount) {
    return amount < value ? value - amount : 0U;
}
#endif

static double sorted_percentile_value(const double *values, size_t count, size_t percentile) {
    size_t index = 0U;

    if (count == 0U) {
        return 0.0;
    }
    if (percentile <= percentile_median && (count % 2U) == 0U) {
        size_t upper = count / 2U;

        return (values[upper - 1U] + values[upper]) / percentile_pair_average_divisor;
    }
    if (percentile > percentile_max) {
        percentile = percentile_max;
    }
    index = ((count * percentile) + percentile_rounding_offset) / percentile_max;
    if (index == 0U) {
        return values[0];
    }
    --index;
    if (index >= count) {
        index = count - 1U;
    }
    return values[index];
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): qsort fixes this callback shape.
static int compare_double_values(const void *left, const void *right) {
    const double left_value = *(const double *)left;
    const double right_value = *(const double *)right;

    if (left_value < right_value) {
        return -1;
    }
    if (left_value > right_value) {
        return 1;
    }
    return 0;
}

static void print_parse_status_counts(const struct benchmark_measurement *measurement) {
    printf(
        "# parse_status ok=%zu misuse=%zu nomem=%zu lexer_error=%zu syntax_error=%zu "
        "stack_overflow=%zu\n",
        measurement->parse_status_counts[MYLITE_SQL_PARSE_OK],
        measurement->parse_status_counts[MYLITE_SQL_PARSE_MISUSE],
        measurement->parse_status_counts[MYLITE_SQL_PARSE_NOMEM],
        measurement->parse_status_counts[MYLITE_SQL_PARSE_LEXER_ERROR],
        measurement->parse_status_counts[MYLITE_SQL_PARSE_SYNTAX_ERROR],
        measurement->parse_status_counts[MYLITE_SQL_PARSE_STACK_OVERFLOW]
    );
}

static double ns_to_ms(uint64_t ns) {
    return (double)ns / ((double)nanoseconds_per_second / (double)milliseconds_per_second);
}

static double ns_to_average_us(uint64_t ns, size_t operations) {
    if (operations == 0U) {
        return 0.0;
    }
    return ((double)ns / ((double)nanoseconds_per_second / (double)microseconds_per_second)) /
           (double)operations;
}

static double ns_to_ops_per_second(uint64_t ns, size_t operations) {
    if (operations == 0U || ns == 0U) {
        return 0.0;
    }
    return ((double)operations * (double)nanoseconds_per_second) / (double)ns;
}
