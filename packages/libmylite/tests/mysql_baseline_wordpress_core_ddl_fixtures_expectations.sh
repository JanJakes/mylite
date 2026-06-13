#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_wordpress_core_ddl_fixtures_$$"

fail() {
    printf '%s\n' "mysql_baseline_wordpress_core_ddl_fixtures_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | mysql --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot --batch --raw \
                --skip-column-names --init-command="SET SESSION sql_mode=''" "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --init-command="SET SESSION sql_mode=''" "$@"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_output_rstrip() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@" | sed 's/[[:space:]]*$//')
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_error() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status=$?
    set -e
    if [ "$status" -eq 0 ]; then
        fail "$label: expected error [$expected], got success [$output]"
    fi
    case "$output" in
        *"$expected"*) ;;
        *) fail "$label: expected error [$expected], got [$output]" ;;
    esac
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql \
    "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_unicode_520_ci;" >/dev/null

setup_sql="SET sql_mode = ''; "\
"CREATE TABLE wp_users ("\
"ID bigint(20) unsigned NOT NULL auto_increment, "\
"user_login varchar(60) NOT NULL default '', "\
"user_pass varchar(255) NOT NULL default '', "\
"user_nicename varchar(50) NOT NULL default '', "\
"user_email varchar(100) NOT NULL default '', "\
"user_url varchar(100) NOT NULL default '', "\
"user_registered datetime NOT NULL default '0000-00-00 00:00:00', "\
"user_activation_key varchar(255) NOT NULL default '', "\
"user_status int(11) NOT NULL default '0', "\
"display_name varchar(250) NOT NULL default '', "\
"PRIMARY KEY (ID), "\
"KEY user_login_key (user_login), "\
"KEY user_nicename (user_nicename), "\
"KEY user_email (user_email)"\
") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci; "\
"CREATE TABLE wp_options ("\
"option_id bigint(20) unsigned NOT NULL auto_increment, "\
"option_name varchar(191) NOT NULL default '', "\
"option_value longtext NOT NULL, "\
"autoload varchar(20) NOT NULL default 'yes', "\
"PRIMARY KEY (option_id), "\
"UNIQUE KEY option_name (option_name), "\
"KEY autoload (autoload)"\
") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci; "\
"CREATE TABLE wp_posts ("\
"ID bigint(20) unsigned NOT NULL auto_increment, "\
"post_author bigint(20) unsigned NOT NULL default '0', "\
"post_date datetime NOT NULL default '0000-00-00 00:00:00', "\
"post_date_gmt datetime NOT NULL default '0000-00-00 00:00:00', "\
"post_content longtext NOT NULL, "\
"post_title text NOT NULL, "\
"post_excerpt text NOT NULL, "\
"post_status varchar(20) NOT NULL default 'publish', "\
"comment_status varchar(20) NOT NULL default 'open', "\
"ping_status varchar(20) NOT NULL default 'open', "\
"post_password varchar(255) NOT NULL default '', "\
"post_name varchar(200) NOT NULL default '', "\
"to_ping text NOT NULL, "\
"pinged text NOT NULL, "\
"post_modified datetime NOT NULL default '0000-00-00 00:00:00', "\
"post_modified_gmt datetime NOT NULL default '0000-00-00 00:00:00', "\
"post_content_filtered longtext NOT NULL, "\
"post_parent bigint(20) unsigned NOT NULL default '0', "\
"guid varchar(255) NOT NULL default '', "\
"menu_order int(11) NOT NULL default '0', "\
"post_type varchar(20) NOT NULL default 'post', "\
"post_mime_type varchar(100) NOT NULL default '', "\
"comment_count bigint(20) NOT NULL default '0', "\
"PRIMARY KEY (ID), "\
"KEY post_name (post_name(191)), "\
"KEY type_status_date (post_type,post_status,post_date,ID), "\
"KEY post_parent (post_parent), "\
"KEY post_author (post_author)"\
") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci; "\
"CREATE TABLE wp_comments ("\
"comment_ID bigint(20) unsigned NOT NULL auto_increment, "\
"comment_post_ID bigint(20) unsigned NOT NULL default '0', "\
"comment_author tinytext NOT NULL, "\
"comment_author_email varchar(100) NOT NULL default '', "\
"comment_author_url varchar(200) NOT NULL default '', "\
"comment_author_IP varchar(100) NOT NULL default '', "\
"comment_date datetime NOT NULL default '0000-00-00 00:00:00', "\
"comment_date_gmt datetime NOT NULL default '0000-00-00 00:00:00', "\
"comment_content text NOT NULL, "\
"comment_karma int(11) NOT NULL default '0', "\
"comment_approved varchar(20) NOT NULL default '1', "\
"comment_agent varchar(255) NOT NULL default '', "\
"comment_type varchar(20) NOT NULL default 'comment', "\
"comment_parent bigint(20) unsigned NOT NULL default '0', "\
"user_id bigint(20) unsigned NOT NULL default '0', "\
"PRIMARY KEY (comment_ID), "\
"KEY comment_post_ID (comment_post_ID), "\
"KEY comment_approved_date_gmt (comment_approved,comment_date_gmt), "\
"KEY comment_date_gmt (comment_date_gmt), "\
"KEY comment_parent (comment_parent), "\
"KEY comment_author_email (comment_author_email(10))"\
") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci; "\
"CREATE TABLE wp_postmeta ("\
"meta_id bigint(20) unsigned NOT NULL auto_increment, "\
"post_id bigint(20) unsigned NOT NULL default '0', "\
"meta_key varchar(255) default NULL, "\
"meta_value longtext, "\
"PRIMARY KEY (meta_id), "\
"KEY post_id (post_id), "\
"KEY meta_key (meta_key(191))"\
") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci; "\
"CREATE TABLE wp_commentmeta ("\
"meta_id bigint(20) unsigned NOT NULL auto_increment, "\
"comment_id bigint(20) unsigned NOT NULL default '0', "\
"meta_key varchar(255) default NULL, "\
"meta_value longtext, "\
"PRIMARY KEY (meta_id), "\
"KEY comment_id (comment_id), "\
"KEY meta_key (meta_key(191))"\
") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci; "\
"SELECT 'wp_commentmeta', @@warning_count; "\
"CREATE TABLE wp_usermeta ("\
"umeta_id bigint(20) unsigned NOT NULL auto_increment, "\
"user_id bigint(20) unsigned NOT NULL default '0', "\
"meta_key varchar(255) default NULL, "\
"meta_value longtext, "\
"PRIMARY KEY (umeta_id), "\
"KEY user_id (user_id), "\
"KEY meta_key (meta_key(191))"\
") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci; "\
"SELECT 'wp_usermeta', @@warning_count; "\
"CREATE TABLE wp_termmeta ("\
"meta_id bigint(20) unsigned NOT NULL auto_increment, "\
"term_id bigint(20) unsigned NOT NULL default '0', "\
"meta_key varchar(255) default NULL, "\
"meta_value longtext, "\
"PRIMARY KEY (meta_id), "\
"KEY term_id (term_id), "\
"KEY meta_key (meta_key(191))"\
") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci; "\
"SELECT 'wp_termmeta', @@warning_count; "\
"CREATE TABLE wp_terms ("\
"term_id bigint(20) unsigned NOT NULL auto_increment, "\
"name varchar(200) NOT NULL default '', "\
"slug varchar(200) NOT NULL default '', "\
"term_group bigint(10) NOT NULL default 0, "\
"PRIMARY KEY (term_id), "\
"KEY slug (slug(191)), "\
"KEY name (name(191))"\
") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci; "\
"SELECT 'wp_terms', @@warning_count; "\
"CREATE TABLE wp_term_taxonomy ("\
"term_taxonomy_id bigint(20) unsigned NOT NULL auto_increment, "\
"term_id bigint(20) unsigned NOT NULL default 0, "\
"taxonomy varchar(32) NOT NULL default '', "\
"description longtext NOT NULL, "\
"parent bigint(20) unsigned NOT NULL default 0, "\
"count bigint(20) NOT NULL default 0, "\
"PRIMARY KEY (term_taxonomy_id), "\
"UNIQUE KEY term_id_taxonomy (term_id,taxonomy), "\
"KEY taxonomy (taxonomy)"\
") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci; "\
"SELECT 'wp_term_taxonomy', @@warning_count; "\
"CREATE TABLE wp_term_relationships ("\
"object_id bigint(20) unsigned NOT NULL default 0, "\
"term_taxonomy_id bigint(20) unsigned NOT NULL default 0, "\
"term_order int(11) NOT NULL default 0, "\
"PRIMARY KEY (object_id,term_taxonomy_id), "\
"KEY term_taxonomy_id (term_taxonomy_id)"\
") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci; "\
"SELECT 'wp_term_relationships', @@warning_count; "\
"CREATE TABLE wp_links ("\
"link_id bigint(20) unsigned NOT NULL auto_increment, "\
"link_url varchar(255) NOT NULL default '', "\
"link_name varchar(255) NOT NULL default '', "\
"link_image varchar(255) NOT NULL default '', "\
"link_target varchar(25) NOT NULL default '', "\
"link_description varchar(255) NOT NULL default '', "\
"link_visible varchar(20) NOT NULL default 'Y', "\
"link_owner bigint(20) unsigned NOT NULL default '1', "\
"link_rating int(11) NOT NULL default '0', "\
"link_updated datetime NOT NULL default '0000-00-00 00:00:00', "\
"link_rel varchar(255) NOT NULL default '', "\
"link_notes mediumtext NOT NULL, "\
"link_rss varchar(255) NOT NULL default '', "\
"PRIMARY KEY (link_id), "\
"KEY link_visible (link_visible)"\
") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci; "\
"SELECT 'wp_links', @@warning_count;"

remaining_create_warnings_expected=$(cat <<\EXPECTED
wp_commentmeta	2
wp_usermeta	2
wp_termmeta	2
wp_terms	2
wp_term_taxonomy	4
wp_term_relationships	3
wp_links	3
EXPECTED
)
expect_output \
    "remaining fixture create warning counts" \
    "$remaining_create_warnings_expected" \
    "$setup_sql" \
    "$DATABASE"

expect_output \
    "fixture display width warning count" \
    "2" \
    "CREATE TABLE display_width_warning_probe (a bigint(20), b int(11)); "\
"SELECT @@warning_count; "\
"DROP TABLE display_width_warning_probe;" \
    "$DATABASE"

quoted_integer_defaults_expected=$(cat <<\EXPECTED
0
i	int	YES	0
p	int	YES	7
n	int	YES	-3
bu	bigint unsigned	YES	9223372036854775807
EXPECTED
)
expect_output \
    "quoted integer default metadata" \
    "$quoted_integer_defaults_expected" \
    "CREATE TABLE quoted_defaults ("\
"i INT DEFAULT '0', "\
"p INT DEFAULT '+7', "\
"n INT DEFAULT '-3', "\
"bu BIGINT UNSIGNED DEFAULT '9223372036854775807'); "\
"SELECT @@warning_count; "\
"SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='${DATABASE}' "\
"AND TABLE_NAME='quoted_defaults' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

expect_error \
    "invalid quoted integer default" \
    "ERROR 1067 (42000)" \
    "CREATE TABLE bad_quoted_int (id INT DEFAULT 'abc');" \
    "$DATABASE"

expect_error \
    "negative quoted unsigned default" \
    "ERROR 1067 (42000)" \
    "CREATE TABLE bad_quoted_unsigned (id INT UNSIGNED DEFAULT '-1');" \
    "$DATABASE"

expect_error \
    "quoted signed bigint default out of range" \
    "ERROR 1067 (42000)" \
    "CREATE TABLE bad_quoted_big_signed (id BIGINT DEFAULT '9223372036854775808');" \
    "$DATABASE"

wp_users_create_expected=$(cat <<\EXPECTED
wp_users	CREATE TABLE `wp_users` (
  `ID` bigint unsigned NOT NULL AUTO_INCREMENT,
  `user_login` varchar(60) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `user_pass` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `user_nicename` varchar(50) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `user_email` varchar(100) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `user_url` varchar(100) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `user_registered` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `user_activation_key` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `user_status` int NOT NULL DEFAULT '0',
  `display_name` varchar(250) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`ID`),
  KEY `user_login_key` (`user_login`),
  KEY `user_nicename` (`user_nicename`),
  KEY `user_email` (`user_email`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci
EXPECTED
)
expect_output \
    "wp_users show create" \
    "$wp_users_create_expected" \
    "SHOW CREATE TABLE wp_users;" \
    "$DATABASE"

wp_users_columns_expected=$(cat <<\EXPECTED
ID	bigint unsigned	NO	PRI	NULL	auto_increment
user_login	varchar(60)	NO	MUL		
user_pass	varchar(255)	NO			
user_nicename	varchar(50)	NO	MUL		
user_email	varchar(100)	NO	MUL		
user_url	varchar(100)	NO			
user_registered	datetime	NO		0000-00-00 00:00:00	
user_activation_key	varchar(255)	NO			
user_status	int	NO		0	
display_name	varchar(250)	NO			
EXPECTED
)
expect_output \
    "wp_users columns" \
    "$wp_users_columns_expected" \
    "SHOW COLUMNS FROM wp_users;" \
    "$DATABASE"

wp_users_insert_expected=$(cat <<\EXPECTED
1	0	1		0000-00-00 00:00:00	0
EXPECTED
)
expect_output \
    "wp_users omitted defaults insert" \
    "$wp_users_insert_expected" \
    "INSERT INTO wp_users () VALUES (); "\
"SELECT ROW_COUNT(), @@warning_count, ID, user_login, user_registered, user_status FROM wp_users;" \
    "$DATABASE"

wp_options_create_expected=$(cat <<\EXPECTED
wp_options	CREATE TABLE `wp_options` (
  `option_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `option_name` varchar(191) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `option_value` longtext COLLATE utf8mb4_unicode_520_ci NOT NULL,
  `autoload` varchar(20) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT 'yes',
  PRIMARY KEY (`option_id`),
  UNIQUE KEY `option_name` (`option_name`),
  KEY `autoload` (`autoload`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci
EXPECTED
)
expect_output \
    "wp_options show create" \
    "$wp_options_create_expected" \
    "SHOW CREATE TABLE wp_options;" \
    "$DATABASE"

wp_posts_create_expected=$(cat <<\EXPECTED
wp_posts	CREATE TABLE `wp_posts` (
  `ID` bigint unsigned NOT NULL AUTO_INCREMENT,
  `post_author` bigint unsigned NOT NULL DEFAULT '0',
  `post_date` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `post_date_gmt` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `post_content` longtext COLLATE utf8mb4_unicode_520_ci NOT NULL,
  `post_title` text COLLATE utf8mb4_unicode_520_ci NOT NULL,
  `post_excerpt` text COLLATE utf8mb4_unicode_520_ci NOT NULL,
  `post_status` varchar(20) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT 'publish',
  `comment_status` varchar(20) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT 'open',
  `ping_status` varchar(20) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT 'open',
  `post_password` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `post_name` varchar(200) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `to_ping` text COLLATE utf8mb4_unicode_520_ci NOT NULL,
  `pinged` text COLLATE utf8mb4_unicode_520_ci NOT NULL,
  `post_modified` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `post_modified_gmt` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `post_content_filtered` longtext COLLATE utf8mb4_unicode_520_ci NOT NULL,
  `post_parent` bigint unsigned NOT NULL DEFAULT '0',
  `guid` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `menu_order` int NOT NULL DEFAULT '0',
  `post_type` varchar(20) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT 'post',
  `post_mime_type` varchar(100) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `comment_count` bigint NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`),
  KEY `post_name` (`post_name`(191)),
  KEY `type_status_date` (`post_type`,`post_status`,`post_date`,`ID`),
  KEY `post_parent` (`post_parent`),
  KEY `post_author` (`post_author`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci
EXPECTED
)
expect_output \
    "wp_posts show create" \
    "$wp_posts_create_expected" \
    "SHOW CREATE TABLE wp_posts;" \
    "$DATABASE"

wp_posts_show_index_expected=$(cat <<\EXPECTED
wp_posts	0	PRIMARY	1	ID	A	0	NULL	NULL		BTREE			YES	NULL
wp_posts	1	post_name	1	post_name	A	0	191	NULL		BTREE			YES	NULL
wp_posts	1	type_status_date	1	post_type	A	0	NULL	NULL		BTREE			YES	NULL
wp_posts	1	type_status_date	2	post_status	A	0	NULL	NULL		BTREE			YES	NULL
wp_posts	1	type_status_date	3	post_date	A	0	NULL	NULL		BTREE			YES	NULL
wp_posts	1	type_status_date	4	ID	A	0	NULL	NULL		BTREE			YES	NULL
wp_posts	1	post_parent	1	post_parent	A	0	NULL	NULL		BTREE			YES	NULL
wp_posts	1	post_author	1	post_author	A	0	NULL	NULL		BTREE			YES	NULL
EXPECTED
)
expect_output \
    "wp_posts show index" \
    "$wp_posts_show_index_expected" \
    "SHOW INDEX FROM wp_posts;" \
    "$DATABASE"

wp_posts_show_column_keys_expected=$(printf '%b' \
    'ID\tbigint unsigned\tNO\tPRI\tNULL\tauto_increment\n'\
    'post_date\tdatetime\tNO\t\t0000-00-00 00:00:00\n'\
    'post_status\tvarchar(20)\tNO\t\tpublish\n'\
    'post_type\tvarchar(20)\tNO\tMUL\tpost')
expect_output_rstrip \
    "wp_posts SHOW COLUMNS key fields" \
    "$wp_posts_show_column_keys_expected" \
    "SHOW COLUMNS FROM wp_posts "\
"WHERE Field IN ('ID','post_type','post_status','post_date');" \
    "$DATABASE"

wp_posts_column_keys_expected=$(cat <<\EXPECTED
ID	PRI
post_date
post_status
post_type	MUL
EXPECTED
)
expect_output_rstrip \
    "wp_posts information_schema column keys" \
    "$wp_posts_column_keys_expected" \
    "SELECT COLUMN_NAME, COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='wp_posts' "\
"AND COLUMN_NAME IN ('ID','post_type','post_status','post_date') "\
"ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

wp_posts_insert_expected=$(cat <<\EXPECTED
1	0	1	0	0000-00-00 00:00:00	publish	open	open		0	0	post	0	body	Title	filtered
EXPECTED
)
expect_output \
    "wp_posts inserted row" \
    "$wp_posts_insert_expected" \
    "INSERT INTO wp_posts (post_content, post_title, post_excerpt, to_ping, pinged, "\
"post_content_filtered) VALUES ('body', 'Title', '', '', '', 'filtered'); "\
"SELECT ROW_COUNT(), @@warning_count, ID, post_author, post_date, post_status, "\
"comment_status, ping_status, post_name, post_parent, menu_order, post_type, "\
"comment_count, post_content, post_title, post_content_filtered FROM wp_posts;" \
    "$DATABASE"

wp_comments_create_expected=$(cat <<\EXPECTED
wp_comments	CREATE TABLE `wp_comments` (
  `comment_ID` bigint unsigned NOT NULL AUTO_INCREMENT,
  `comment_post_ID` bigint unsigned NOT NULL DEFAULT '0',
  `comment_author` tinytext COLLATE utf8mb4_unicode_520_ci NOT NULL,
  `comment_author_email` varchar(100) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `comment_author_url` varchar(200) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `comment_author_IP` varchar(100) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `comment_date` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `comment_date_gmt` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `comment_content` text COLLATE utf8mb4_unicode_520_ci NOT NULL,
  `comment_karma` int NOT NULL DEFAULT '0',
  `comment_approved` varchar(20) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '1',
  `comment_agent` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `comment_type` varchar(20) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT 'comment',
  `comment_parent` bigint unsigned NOT NULL DEFAULT '0',
  `user_id` bigint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`comment_ID`),
  KEY `comment_post_ID` (`comment_post_ID`),
  KEY `comment_approved_date_gmt` (`comment_approved`,`comment_date_gmt`),
  KEY `comment_date_gmt` (`comment_date_gmt`),
  KEY `comment_parent` (`comment_parent`),
  KEY `comment_author_email` (`comment_author_email`(10))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci
EXPECTED
)
expect_output \
    "wp_comments show create" \
    "$wp_comments_create_expected" \
    "SHOW CREATE TABLE wp_comments;" \
    "$DATABASE"

wp_comments_show_index_expected=$(cat <<\EXPECTED
wp_comments	0	PRIMARY	1	comment_ID	A	0	NULL	NULL		BTREE			YES	NULL
wp_comments	1	comment_post_ID	1	comment_post_ID	A	0	NULL	NULL		BTREE			YES	NULL
wp_comments	1	comment_approved_date_gmt	1	comment_approved	A	0	NULL	NULL		BTREE			YES	NULL
wp_comments	1	comment_approved_date_gmt	2	comment_date_gmt	A	0	NULL	NULL		BTREE			YES	NULL
wp_comments	1	comment_date_gmt	1	comment_date_gmt	A	0	NULL	NULL		BTREE			YES	NULL
wp_comments	1	comment_parent	1	comment_parent	A	0	NULL	NULL		BTREE			YES	NULL
wp_comments	1	comment_author_email	1	comment_author_email	A	0	10	NULL		BTREE			YES	NULL
EXPECTED
)
expect_output \
    "wp_comments show index" \
    "$wp_comments_show_index_expected" \
    "SHOW INDEX FROM wp_comments;" \
    "$DATABASE"

wp_comments_show_column_keys_expected=$(printf '%b' \
    'comment_ID\tbigint unsigned\tNO\tPRI\tNULL\tauto_increment\n'\
    'comment_date_gmt\tdatetime\tNO\tMUL\t0000-00-00 00:00:00\n'\
    'comment_approved\tvarchar(20)\tNO\tMUL\t1\n'\
    'comment_parent\tbigint unsigned\tNO\tMUL\t0')
expect_output_rstrip \
    "wp_comments SHOW COLUMNS key fields" \
    "$wp_comments_show_column_keys_expected" \
    "SHOW COLUMNS FROM wp_comments "\
"WHERE Field IN ('comment_ID','comment_approved','comment_date_gmt','comment_parent');" \
    "$DATABASE"

wp_comments_column_keys_expected=$(cat <<\EXPECTED
comment_ID	PRI
comment_date_gmt	MUL
comment_approved	MUL
comment_parent	MUL
EXPECTED
)
expect_output_rstrip \
    "wp_comments information_schema column keys" \
    "$wp_comments_column_keys_expected" \
    "SELECT COLUMN_NAME, COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='wp_comments' "\
"AND COLUMN_NAME IN ('comment_ID','comment_approved','comment_date_gmt','comment_parent') "\
"ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

wp_comments_insert_expected=$(cat <<\EXPECTED
1	0	1	0	Jan		0000-00-00 00:00:00	hello	1	comment	0
EXPECTED
)
expect_output \
    "wp_comments inserted row" \
    "$wp_comments_insert_expected" \
    "INSERT INTO wp_comments (comment_author, comment_content) VALUES ('Jan', 'hello'); "\
"SELECT ROW_COUNT(), @@warning_count, comment_ID, comment_post_ID, comment_author, "\
"comment_author_email, comment_date, comment_content, comment_approved, comment_type, "\
"user_id FROM wp_comments;" \
    "$DATABASE"

wp_postmeta_create_expected=$(cat <<\EXPECTED
wp_postmeta	CREATE TABLE `wp_postmeta` (
  `meta_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `post_id` bigint unsigned NOT NULL DEFAULT '0',
  `meta_key` varchar(255) COLLATE utf8mb4_unicode_520_ci DEFAULT NULL,
  `meta_value` longtext COLLATE utf8mb4_unicode_520_ci,
  PRIMARY KEY (`meta_id`),
  KEY `post_id` (`post_id`),
  KEY `meta_key` (`meta_key`(191))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci
EXPECTED
)
expect_output \
    "wp_postmeta show create" \
    "$wp_postmeta_create_expected" \
    "SHOW CREATE TABLE wp_postmeta;" \
    "$DATABASE"

wp_postmeta_show_index_expected=$(cat <<\EXPECTED
wp_postmeta	0	PRIMARY	1	meta_id	A	0	NULL	NULL		BTREE			YES	NULL
wp_postmeta	1	post_id	1	post_id	A	0	NULL	NULL		BTREE			YES	NULL
wp_postmeta	1	meta_key	1	meta_key	A	0	191	NULL	YES	BTREE			YES	NULL
EXPECTED
)
expect_output \
    "wp_postmeta show index" \
    "$wp_postmeta_show_index_expected" \
    "SHOW INDEX FROM wp_postmeta;" \
    "$DATABASE"

wp_postmeta_columns_expected=$(cat <<\EXPECTED
meta_id	bigint unsigned	NULL	NULL	PRI	auto_increment	1
post_id	bigint unsigned	NULL	0	MUL		2
meta_key	varchar(255)	utf8mb4_unicode_520_ci	NULL	MUL		3
meta_value	longtext	utf8mb4_unicode_520_ci	NULL			4
EXPECTED
)
expect_output \
    "wp_postmeta information_schema columns" \
    "$wp_postmeta_columns_expected" \
    "SELECT COLUMN_NAME, COLUMN_TYPE, COLLATION_NAME, COLUMN_DEFAULT, COLUMN_KEY, EXTRA, "\
"ORDINAL_POSITION "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='${DATABASE}' "\
"AND TABLE_NAME='wp_postmeta' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

wp_row_values_expected=$(cat <<\EXPECTED
1	0
1	siteurl	https://example.test	yes
1	1	k	v
2	2	NULL	NULL
3	0	omitted	default
EXPECTED
)
expect_output \
    "wp_options and wp_postmeta row values" \
    "$wp_row_values_expected" \
    "INSERT INTO wp_options (option_name, option_value) "\
"VALUES ('siteurl', 'https://example.test'); "\
"INSERT INTO wp_postmeta (post_id, meta_key, meta_value) "\
"VALUES (1, 'k', 'v'), (2, NULL, NULL); "\
"INSERT INTO wp_postmeta (meta_key, meta_value) VALUES ('omitted', 'default'); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT option_id, option_name, option_value, autoload FROM wp_options; "\
"SELECT meta_id, post_id, meta_key, meta_value FROM wp_postmeta ORDER BY meta_id;" \
    "$DATABASE"

wp_postmeta_index_expected=$(cat <<\EXPECTED
PRIMARY	1	meta_id	NULL		YES
meta_key	1	meta_key	191	YES	YES
post_id	1	post_id	NULL		YES
EXPECTED
)
expect_output \
    "wp_postmeta index metadata" \
    "$wp_postmeta_index_expected" \
    "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART, NULLABLE, IS_VISIBLE "\
"FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA='${DATABASE}' "\
"AND TABLE_NAME='wp_postmeta' ORDER BY INDEX_NAME='PRIMARY' DESC, INDEX_NAME;" \
    "$DATABASE"

remaining_show_create_expected=$(cat <<\EXPECTED
wp_commentmeta	CREATE TABLE `wp_commentmeta` (
  `meta_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `comment_id` bigint unsigned NOT NULL DEFAULT '0',
  `meta_key` varchar(255) COLLATE utf8mb4_unicode_520_ci DEFAULT NULL,
  `meta_value` longtext COLLATE utf8mb4_unicode_520_ci,
  PRIMARY KEY (`meta_id`),
  KEY `comment_id` (`comment_id`),
  KEY `meta_key` (`meta_key`(191))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci
wp_terms	CREATE TABLE `wp_terms` (
  `term_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(200) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `slug` varchar(200) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `term_group` bigint NOT NULL DEFAULT '0',
  PRIMARY KEY (`term_id`),
  KEY `slug` (`slug`(191)),
  KEY `name` (`name`(191))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci
wp_term_taxonomy	CREATE TABLE `wp_term_taxonomy` (
  `term_taxonomy_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `term_id` bigint unsigned NOT NULL DEFAULT '0',
  `taxonomy` varchar(32) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `description` longtext COLLATE utf8mb4_unicode_520_ci NOT NULL,
  `parent` bigint unsigned NOT NULL DEFAULT '0',
  `count` bigint NOT NULL DEFAULT '0',
  PRIMARY KEY (`term_taxonomy_id`),
  UNIQUE KEY `term_id_taxonomy` (`term_id`,`taxonomy`),
  KEY `taxonomy` (`taxonomy`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci
wp_term_relationships	CREATE TABLE `wp_term_relationships` (
  `object_id` bigint unsigned NOT NULL DEFAULT '0',
  `term_taxonomy_id` bigint unsigned NOT NULL DEFAULT '0',
  `term_order` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`object_id`,`term_taxonomy_id`),
  KEY `term_taxonomy_id` (`term_taxonomy_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci
wp_links	CREATE TABLE `wp_links` (
  `link_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `link_url` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `link_name` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `link_image` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `link_target` varchar(25) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `link_description` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `link_visible` varchar(20) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT 'Y',
  `link_owner` bigint unsigned NOT NULL DEFAULT '1',
  `link_rating` int NOT NULL DEFAULT '0',
  `link_updated` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `link_rel` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  `link_notes` mediumtext COLLATE utf8mb4_unicode_520_ci NOT NULL,
  `link_rss` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`link_id`),
  KEY `link_visible` (`link_visible`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci
EXPECTED
)
expect_output \
    "remaining fixture SHOW CREATE TABLE" \
    "$remaining_show_create_expected" \
    "SHOW CREATE TABLE wp_commentmeta; "\
"SHOW CREATE TABLE wp_terms; "\
"SHOW CREATE TABLE wp_term_taxonomy; "\
"SHOW CREATE TABLE wp_term_relationships; "\
"SHOW CREATE TABLE wp_links;" \
    "$DATABASE"

remaining_show_index_expected=$(cat <<\EXPECTED
wp_commentmeta	0	PRIMARY	1	meta_id	A	0	NULL	NULL		BTREE			YES	NULL
wp_commentmeta	1	comment_id	1	comment_id	A	0	NULL	NULL		BTREE			YES	NULL
wp_commentmeta	1	meta_key	1	meta_key	A	0	191	NULL	YES	BTREE			YES	NULL
wp_usermeta	0	PRIMARY	1	umeta_id	A	0	NULL	NULL		BTREE			YES	NULL
wp_usermeta	1	user_id	1	user_id	A	0	NULL	NULL		BTREE			YES	NULL
wp_usermeta	1	meta_key	1	meta_key	A	0	191	NULL	YES	BTREE			YES	NULL
wp_termmeta	0	PRIMARY	1	meta_id	A	0	NULL	NULL		BTREE			YES	NULL
wp_termmeta	1	term_id	1	term_id	A	0	NULL	NULL		BTREE			YES	NULL
wp_termmeta	1	meta_key	1	meta_key	A	0	191	NULL	YES	BTREE			YES	NULL
wp_terms	0	PRIMARY	1	term_id	A	0	NULL	NULL		BTREE			YES	NULL
wp_terms	1	slug	1	slug	A	0	191	NULL		BTREE			YES	NULL
wp_terms	1	name	1	name	A	0	191	NULL		BTREE			YES	NULL
wp_term_taxonomy	0	PRIMARY	1	term_taxonomy_id	A	0	NULL	NULL		BTREE			YES	NULL
wp_term_taxonomy	0	term_id_taxonomy	1	term_id	A	0	NULL	NULL		BTREE			YES	NULL
wp_term_taxonomy	0	term_id_taxonomy	2	taxonomy	A	0	NULL	NULL		BTREE			YES	NULL
wp_term_taxonomy	1	taxonomy	1	taxonomy	A	0	NULL	NULL		BTREE			YES	NULL
wp_term_relationships	0	PRIMARY	1	object_id	A	0	NULL	NULL		BTREE			YES	NULL
wp_term_relationships	0	PRIMARY	2	term_taxonomy_id	A	0	NULL	NULL		BTREE			YES	NULL
wp_term_relationships	1	term_taxonomy_id	1	term_taxonomy_id	A	0	NULL	NULL		BTREE			YES	NULL
wp_links	0	PRIMARY	1	link_id	A	0	NULL	NULL		BTREE			YES	NULL
wp_links	1	link_visible	1	link_visible	A	0	NULL	NULL		BTREE			YES	NULL
EXPECTED
)
expect_output \
    "remaining fixture SHOW INDEX" \
    "$remaining_show_index_expected" \
    "SHOW INDEX FROM wp_commentmeta; "\
"SHOW INDEX FROM wp_usermeta; "\
"SHOW INDEX FROM wp_termmeta; "\
"SHOW INDEX FROM wp_terms; "\
"SHOW INDEX FROM wp_term_taxonomy; "\
"SHOW INDEX FROM wp_term_relationships; "\
"SHOW INDEX FROM wp_links;" \
    "$DATABASE"

remaining_row_values_expected=$(cat <<\EXPECTED
1	0
1	1	k	v
1	2	u	uv
1	3	t	tv
1			0
1	1	category	desc	0	0
10	1	0
1		Y	1	0	0000-00-00 00:00:00	notes
EXPECTED
)
expect_output \
    "remaining fixture row values" \
    "$remaining_row_values_expected" \
    "INSERT INTO wp_commentmeta (comment_id, meta_key, meta_value) "\
"VALUES (1, 'k', 'v'); "\
"INSERT INTO wp_usermeta (user_id, meta_key, meta_value) "\
"VALUES (2, 'u', 'uv'); "\
"INSERT INTO wp_termmeta (term_id, meta_key, meta_value) "\
"VALUES (3, 't', 'tv'); "\
"INSERT INTO wp_terms () VALUES (); "\
"INSERT INTO wp_term_taxonomy (term_id, taxonomy, description) "\
"VALUES (1, 'category', 'desc'); "\
"INSERT INTO wp_term_relationships (object_id, term_taxonomy_id) VALUES (10, 1); "\
"INSERT INTO wp_links (link_notes) VALUES ('notes'); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT meta_id, comment_id, meta_key, meta_value FROM wp_commentmeta; "\
"SELECT umeta_id, user_id, meta_key, meta_value FROM wp_usermeta; "\
"SELECT meta_id, term_id, meta_key, meta_value FROM wp_termmeta; "\
"SELECT term_id, name, slug, term_group FROM wp_terms; "\
"SELECT term_taxonomy_id, term_id, taxonomy, description, parent, count "\
"FROM wp_term_taxonomy; "\
"SELECT object_id, term_taxonomy_id, term_order FROM wp_term_relationships; "\
"SELECT link_id, link_url, link_visible, link_owner, link_rating, link_updated, "\
"link_notes FROM wp_links;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_wordpress_core_ddl_fixtures_expectations: ok"
