#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_cmp_per_index_expectations: $1" >&2
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

enabled=$(run_mysql "SELECT @@innodb_cmp_per_index_enabled;")
expect_value "default innodb_cmp_per_index_enabled" "0" "$enabled"

table_kinds=$(run_mysql \
    "SHOW FULL TABLES FROM INFORMATION_SCHEMA WHERE Tables_in_INFORMATION_SCHEMA "\
"IN ('INNODB_CMP_PER_INDEX', 'INNODB_CMP_PER_INDEX_RESET');")
expect_value "innodb cmp per index table kinds" \
    "INNODB_CMP_PER_INDEX	SYSTEM VIEW
INNODB_CMP_PER_INDEX_RESET	SYSTEM VIEW" \
    "$table_kinds"

cmp_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX;")
expect_value "default innodb cmp per index count" "0" "$cmp_count"

reset_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX_RESET;")
expect_value "default innodb cmp per index reset count" "0" "$reset_count"

repeat_reset_counts=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX_RESET; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX_RESET;")
expect_value "repeated innodb cmp per index reset counts" "0
0" "$repeat_reset_counts"

case_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_cmp_per_index;")
expect_value "case-insensitive cmp per index table name count" "0" "$case_count"

use_counts=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM INNODB_CMP_PER_INDEX; "\
"SELECT COUNT(*) FROM INNODB_CMP_PER_INDEX_RESET;")
expect_value "unqualified innodb cmp per index counts" "0
0" "$use_counts"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX_RESET; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb cmp per index status" "0	-1" "$status"

cmp_alias_rows=$(run_mysql \
    "SELECT c.database_name, c.table_name, c.index_name "\
"FROM INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX AS c WHERE c.compress_ops = 1;")
expect_value "innodb cmp per index alias observation" "" "$cmp_alias_rows"

reset_alias_rows=$(run_mysql \
    "SELECT r.database_name, r.table_name, r.index_name "\
"FROM INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX_RESET AS r WHERE r.uncompress_ops = 1;")
expect_value "innodb cmp per index reset alias observation" "" "$reset_alias_rows"

system_table_rows=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('INNODB_CMP_PER_INDEX', 'INNODB_CMP_PER_INDEX_RESET') "\
"ORDER BY TABLE_NAME;")
expect_value "innodb cmp per index system table rows" \
    "information_schema	INNODB_CMP_PER_INDEX	SYSTEM VIEW	NULL	10	NULL	0	0	NULL
information_schema	INNODB_CMP_PER_INDEX_RESET	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_rows"

expected_columns_metadata="INNODB_CMP_PER_INDEX	database_name	1		NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(192)	select
INNODB_CMP_PER_INDEX	table_name	2		NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(192)	select
INNODB_CMP_PER_INDEX	index_name	3		NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(192)	select
INNODB_CMP_PER_INDEX	compress_ops	4		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP_PER_INDEX	compress_ops_ok	5		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP_PER_INDEX	compress_time	6		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP_PER_INDEX	uncompress_ops	7		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP_PER_INDEX	uncompress_time	8		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP_PER_INDEX_RESET	database_name	1		NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(192)	select
INNODB_CMP_PER_INDEX_RESET	table_name	2		NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(192)	select
INNODB_CMP_PER_INDEX_RESET	index_name	3		NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(192)	select
INNODB_CMP_PER_INDEX_RESET	compress_ops	4		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP_PER_INDEX_RESET	compress_ops_ok	5		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP_PER_INDEX_RESET	compress_time	6		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP_PER_INDEX_RESET	uncompress_ops	7		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_CMP_PER_INDEX_RESET	uncompress_time	8		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('INNODB_CMP_PER_INDEX', 'INNODB_CMP_PER_INDEX_RESET') "\
"ORDER BY TABLE_NAME, ORDINAL_POSITION;")
expect_value "innodb cmp per index columns metadata" "$expected_columns_metadata" "$columns_metadata"

printf '%s\n' "mysql_baseline_information_schema_innodb_cmp_per_index_expectations: ok"
