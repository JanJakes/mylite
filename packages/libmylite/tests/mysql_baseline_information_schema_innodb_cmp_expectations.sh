#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_cmp_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

table_kinds=$(run_mysql \
    "SHOW FULL TABLES FROM INFORMATION_SCHEMA WHERE Tables_in_INFORMATION_SCHEMA "\
"IN ('INNODB_CMP', 'INNODB_CMP_RESET');")
expect_value "innodb cmp table kinds" \
    "INNODB_CMP	SYSTEM VIEW
INNODB_CMP_RESET	SYSTEM VIEW" \
    "$table_kinds"

cmp_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CMP;")
expect_value "default innodb cmp count" "5" "$cmp_count"

reset_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CMP_RESET;")
expect_value "default innodb cmp reset count" "5" "$reset_count"

expected_cmp_rows="1024	0	0	0	0	0
2048	0	0	0	0	0
4096	0	0	0	0	0
8192	0	0	0	0	0
16384	0	0	0	0	0"

cmp_rows=$(run_mysql \
    "SELECT PAGE_SIZE, COMPRESS_OPS, COMPRESS_OPS_OK, COMPRESS_TIME, "\
"UNCOMPRESS_OPS, UNCOMPRESS_TIME FROM INFORMATION_SCHEMA.INNODB_CMP "\
"ORDER BY PAGE_SIZE;")
expect_value "innodb cmp baseline rows" "$expected_cmp_rows" "$cmp_rows"

reset_rows=$(run_mysql \
    "SELECT PAGE_SIZE, COMPRESS_OPS, COMPRESS_OPS_OK, COMPRESS_TIME, "\
"UNCOMPRESS_OPS, UNCOMPRESS_TIME FROM INFORMATION_SCHEMA.INNODB_CMP_RESET "\
"ORDER BY PAGE_SIZE;")
expect_value "innodb cmp reset baseline rows" "$expected_cmp_rows" "$reset_rows"

repeat_reset_counts=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CMP_RESET; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CMP_RESET;")
expect_value "repeated innodb cmp reset counts" "5
5" "$repeat_reset_counts"

case_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_cmp;")
expect_value "case-insensitive cmp table name count" "5" "$case_count"

use_counts=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM INNODB_CMP; "\
"SELECT COUNT(*) FROM INNODB_CMP_RESET;")
expect_value "unqualified innodb cmp counts" "5
5" "$use_counts"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CMP_RESET; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb cmp status" "0	-1" "$status"

cmp_alias_rows=$(run_mysql \
    "SELECT c.page_size, c.compress_ops FROM INFORMATION_SCHEMA.INNODB_CMP AS c "\
"WHERE c.page_size IN (1024, 16384) ORDER BY c.page_size;")
expect_value "innodb cmp alias rows" "1024	0
16384	0" "$cmp_alias_rows"

reset_alias_rows=$(run_mysql \
    "SELECT r.page_size, r.uncompress_ops FROM INFORMATION_SCHEMA.INNODB_CMP_RESET AS r "\
"WHERE r.page_size BETWEEN 2048 AND 8192 ORDER BY r.page_size;")
expect_value "innodb cmp reset alias rows" "2048	0
4096	0
8192	0" "$reset_alias_rows"

system_table_rows=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('INNODB_CMP', 'INNODB_CMP_RESET') "\
"ORDER BY TABLE_NAME;")
expect_value "innodb cmp system table rows" \
    "information_schema	INNODB_CMP	SYSTEM VIEW	NULL	10	NULL	0	0	NULL
information_schema	INNODB_CMP_RESET	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_rows"

expected_columns_metadata="INNODB_CMP	page_size	1		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP	compress_ops	2		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP	compress_ops_ok	3		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP	compress_time	4		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP	uncompress_ops	5		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP	uncompress_time	6		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP_RESET	page_size	1		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP_RESET	compress_ops	2		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP_RESET	compress_ops_ok	3		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP_RESET	compress_time	4		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP_RESET	uncompress_ops	5		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP_RESET	uncompress_time	6		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('INNODB_CMP', 'INNODB_CMP_RESET') "\
"ORDER BY TABLE_NAME, ORDINAL_POSITION;")
expect_value "innodb cmp columns metadata" "$expected_columns_metadata" "$columns_metadata"

printf '%s\n' "mysql_baseline_information_schema_innodb_cmp_expectations: ok"
