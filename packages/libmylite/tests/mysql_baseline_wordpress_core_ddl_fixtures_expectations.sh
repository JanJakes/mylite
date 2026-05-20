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
                --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names "$@"
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
"CREATE TABLE wp_postmeta ("\
"meta_id bigint(20) unsigned NOT NULL auto_increment, "\
"post_id bigint(20) unsigned NOT NULL default '0', "\
"meta_key varchar(255) default NULL, "\
"meta_value longtext, "\
"PRIMARY KEY (meta_id), "\
"KEY post_id (post_id), "\
"KEY meta_key (meta_key(191))"\
") DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci;"

run_mysql "$setup_sql" "$DATABASE" >/dev/null

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

wp_row_values_expected=$(cat <<\EXPECTED
2	0
1	siteurl	https://example.test	yes
1	1	k	v
2	2	NULL	NULL
EXPECTED
)
expect_output \
    "wp_options and wp_postmeta row values" \
    "$wp_row_values_expected" \
    "INSERT INTO wp_options (option_name, option_value) "\
"VALUES ('siteurl', 'https://example.test'); "\
"INSERT INTO wp_postmeta (post_id, meta_key, meta_value) "\
"VALUES (1, 'k', 'v'), (2, NULL, NULL); "\
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

printf '%s\n' "mysql_baseline_wordpress_core_ddl_fixtures_expectations: ok"
