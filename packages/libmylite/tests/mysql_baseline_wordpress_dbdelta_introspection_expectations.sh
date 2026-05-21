#!/usr/bin/env sh

set -eu

MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_wordpress_dbdelta_introspection_$$"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_wordpress_dbdelta_introspection_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" \
                -uroot --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" \
                mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw \
                --skip-column-names "$@"
    fi
}

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3

    output=$(run_mysql "$sql" | normalize_tsv)
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
run_mysql "USE ${DATABASE}; ${setup_sql}" >/dev/null

describe_expected=$(cat <<\EXPECTED
option_id|bigint unsigned|NO|PRI|NULL|auto_increment
option_name|varchar(191)|NO|UNI||
option_value|longtext|NO||NULL|
autoload|varchar(20)|NO|MUL|yes|
EXPECTED
)

expect_output \
    "DESCRIBE wp_options" \
    "$describe_expected" \
    "USE ${DATABASE}; DESCRIBE wp_options;"
expect_output \
    "DESC wp_options" \
    "$describe_expected" \
    "USE ${DATABASE}; DESC wp_options;"
expect_output \
    "EXPLAIN wp_options" \
    "$describe_expected" \
    "USE ${DATABASE}; EXPLAIN wp_options;"

full_columns_expected=$(cat <<\EXPECTED
option_id|bigint unsigned|NULL|NO|PRI|NULL|auto_increment|select,insert,update,references|
option_name|varchar(191)|utf8mb4_unicode_520_ci|NO|UNI|||select,insert,update,references|
option_value|longtext|utf8mb4_unicode_520_ci|NO||NULL||select,insert,update,references|
autoload|varchar(20)|utf8mb4_unicode_520_ci|NO|MUL|yes||select,insert,update,references|
EXPECTED
)
expect_output \
    "SHOW FULL COLUMNS wp_options" \
    "$full_columns_expected" \
    "USE ${DATABASE}; SHOW FULL COLUMNS FROM wp_options;"

full_columns_where_expected=$(cat <<\EXPECTED
option_id|bigint unsigned|NULL|NO|PRI|NULL|auto_increment|select,insert,update,references|
option_name|varchar(191)|utf8mb4_unicode_520_ci|NO|UNI|||select,insert,update,references|
autoload|varchar(20)|utf8mb4_unicode_520_ci|NO|MUL|yes||select,insert,update,references|
EXPECTED
)
expect_output \
    "SHOW FULL COLUMNS wp_options WHERE" \
    "$full_columns_where_expected" \
    "USE ${DATABASE}; SHOW FULL COLUMNS FROM wp_options "\
"WHERE Field IN ('option_id','option_name','autoload');"

show_index_postmeta_expected=$(cat <<\EXPECTED
wp_postmeta|1|post_id|1|post_id|A|0|NULL|NULL||BTREE|||YES|NULL
wp_postmeta|1|meta_key|1|meta_key|A|0|191|NULL|YES|BTREE|||YES|NULL
EXPECTED
)
expect_output \
    "SHOW INDEX wp_postmeta WHERE" \
    "$show_index_postmeta_expected" \
    "USE ${DATABASE}; SHOW INDEX FROM wp_postmeta "\
"WHERE Key_name IN ('post_id','meta_key');"

show_index_unique_expected=$(cat <<\EXPECTED
wp_options|0|option_name|1|option_name|A|0|NULL|NULL||BTREE|||YES|NULL
EXPECTED
)
expect_output \
    "SHOW INDEX unique WHERE" \
    "$show_index_unique_expected" \
    "USE ${DATABASE}; SHOW INDEX FROM wp_options "\
"WHERE Non_unique = '0' AND Column_name = 'option_name';"

columns_expected=$(cat <<\EXPECTED
wp_options|option_id|bigint unsigned|NULL|NO|PRI|NULL|auto_increment
wp_options|option_name|varchar(191)||NO|UNI|utf8mb4_unicode_520_ci|
wp_options|option_value|longtext|NULL|NO||utf8mb4_unicode_520_ci|
wp_options|autoload|varchar(20)|yes|NO|MUL|utf8mb4_unicode_520_ci|
EXPECTED
)
expect_output \
    "INFORMATION_SCHEMA.COLUMNS wp_options" \
    "$columns_expected" \
    "USE ${DATABASE}; "\
"SELECT TABLE_NAME,COLUMN_NAME,COLUMN_TYPE,COLUMN_DEFAULT,IS_NULLABLE,COLUMN_KEY,"\
"COLLATION_NAME,EXTRA FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='wp_options' ORDER BY ORDINAL_POSITION;"

statistics_expected=$(cat <<\EXPECTED
wp_postmeta|meta_key|1|meta_key|1|191|BTREE|YES
EXPECTED
)
expect_output \
    "INFORMATION_SCHEMA.STATISTICS wp_postmeta" \
    "$statistics_expected" \
    "USE ${DATABASE}; "\
"SELECT TABLE_NAME,INDEX_NAME,SEQ_IN_INDEX,COLUMN_NAME,NON_UNIQUE,SUB_PART,INDEX_TYPE,"\
"IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA=DATABASE() "\
"AND TABLE_NAME='wp_postmeta' AND INDEX_NAME='meta_key';"

show_create_expected=$(cat <<\EXPECTED
wp_options|CREATE TABLE `wp_options` (
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
    "SHOW CREATE TABLE wp_options" \
    "$show_create_expected" \
    "USE ${DATABASE}; SHOW CREATE TABLE wp_options;"

status=$(run_mysql \
    "USE ${DATABASE}; SHOW FULL COLUMNS FROM wp_options; SELECT @@warning_count, ROW_COUNT();" \
    | tail -n 1)
if [ "$status" != "0${TAB}-1" ]; then
    fail "metadata status: expected [0${TAB}-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_wordpress_dbdelta_introspection_expectations: ok"
