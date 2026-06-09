#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_table_ddl_options_$$"

fail() {
    printf '%s\n' "mysql_baseline_table_ddl_option_composition_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "drop tables alias" \
    "0" \
    "CREATE TABLE drop_alias (id INT PRIMARY KEY); "\
"DROP TABLES drop_alias; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'drop_alias';" \
    "$DATABASE"

expect_output \
    "drop restrict cascade tails" \
    "0" \
    "CREATE TABLE drop_restrict (id INT PRIMARY KEY); "\
"CREATE TABLE drop_cascade (id INT PRIMARY KEY); "\
"DROP TABLE drop_restrict RESTRICT; "\
"DROP TABLE drop_cascade CASCADE; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME IN ('drop_restrict', 'drop_cascade');" \
    "$DATABASE"

show_extended_expected=$(cat <<\EXPECTED
show_extended	BASE TABLE
EXPECTED
)
expect_output \
    "show extended full tables" \
    "$show_extended_expected" \
    "CREATE TABLE show_extended (id INT PRIMARY KEY); "\
"SHOW EXTENDED FULL TABLES LIKE 'show_extended';" \
    "$DATABASE"

check_tables_expected=$(cat <<EXPECTED
${DATABASE}.check_alias	check	status	OK
EXPECTED
)
expect_output \
    "check tables alias" \
    "$check_tables_expected" \
    "CREATE TABLE check_alias (id INT PRIMARY KEY); CHECK TABLES check_alias;" \
    "$DATABASE"

parenthesized_expected=$(cat <<\EXPECTED
0	0
id	int	NO	PRI	NULL
a	int	YES		NULL
b	int	YES		7
1:NULL:7
EXPECTED
)
expect_output \
    "parenthesized add column list" \
    "$parenthesized_expected" \
    "CREATE TABLE parenthesized_add (id INT PRIMARY KEY); "\
"INSERT INTO parenthesized_add VALUES (1); "\
"ALTER TABLE parenthesized_add ADD COLUMN (a INT, b INT DEFAULT 7); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_KEY, COALESCE(COLUMN_DEFAULT, 'NULL') "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'parenthesized_add' "\
"ORDER BY ORDINAL_POSITION; "\
"SELECT CONCAT(id, ':', COALESCE(a, 'NULL'), ':', b) FROM parenthesized_add;" \
    "$DATABASE"

online_option_expected=$(cat <<\EXPECTED
c,d,e
EXPECTED
)
expect_output \
    "multi-action online option placement" \
    "$online_option_expected" \
    "CREATE TABLE online_options (id INT PRIMARY KEY); "\
"ALTER TABLE online_options ALGORITHM=INSTANT, ADD COLUMN c INT; "\
"ALTER TABLE online_options ADD COLUMN d INT, ADD COLUMN e INT, ALGORITHM=INSTANT, LOCK=DEFAULT; "\
"SELECT GROUP_CONCAT(COLUMN_NAME ORDER BY ORDINAL_POSITION) "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'online_options' "\
"AND COLUMN_NAME IN ('c', 'd', 'e');" \
    "$DATABASE"

storage_options_expected=$(cat <<\EXPECTED
Compressed	1	1	max_rows=100 min_rows=1 avg_row_length=10 row_format=COMPRESSED stats_sample_pages=16 stats_auto_recalc=1 KEY_BLOCK_SIZE=4 stats_persistent=1 pack_keys=1 checksum=1 delay_key_write=1
table_options	CREATE TABLE `table_options` (
  `id` int NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci MIN_ROWS=1 MAX_ROWS=100 AVG_ROW_LENGTH=10 PACK_KEYS=1 STATS_PERSISTENT=1 STATS_AUTO_RECALC=1 STATS_SAMPLE_PAGES=16 CHECKSUM=1 DELAY_KEY_WRITE=1 ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=4
EXPECTED
)
expect_output \
    "storage statistics table options" \
    "$storage_options_expected" \
    "CREATE TABLE table_options (id INT PRIMARY KEY); "\
"ALTER TABLE table_options ENGINE=InnoDB, ROW_FORMAT=COMPRESSED, KEY_BLOCK_SIZE=4, "\
"PACK_KEYS=1, CHECKSUM=1, STATS_PERSISTENT=1, STATS_AUTO_RECALC=1, "\
"STATS_SAMPLE_PAGES=16, MIN_ROWS=1, MAX_ROWS=100, AVG_ROW_LENGTH=10, DELAY_KEY_WRITE=1; "\
"SELECT ROW_FORMAT, CREATE_OPTIONS LIKE '%pack_keys=1%', "\
"CREATE_OPTIONS LIKE '%checksum=1%', CREATE_OPTIONS "\
"FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'table_options'; "\
"SHOW CREATE TABLE table_options;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_table_ddl_option_composition_expectations: ok"
