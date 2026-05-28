#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_temp_table_info_expectations: $1" >&2
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

count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO;")
expect_value "default innodb temp table info count" "0" "$count"

case_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_temp_table_info;")
expect_value "case-insensitive table name count" "0" "$case_count"

status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb temp table info status" "0	-1" "$status"

alias_row=$(run_mysql \
    "SELECT t.NAME FROM INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO AS t "\
"WHERE t.NAME IS NOT NULL ORDER BY t.NAME LIMIT 1;")
expect_value "empty innodb temp table info alias observation" "" "$alias_row"

dynamic_count=$(run_mysql \
    "DROP DATABASE IF EXISTS mylite_temp_info_probe; "\
"CREATE DATABASE mylite_temp_info_probe; "\
"USE mylite_temp_info_probe; "\
"CREATE TEMPORARY TABLE t1 (c1 INT PRIMARY KEY) ENGINE=InnoDB; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO "\
"WHERE NAME LIKE '#sql%' AND N_COLS = 4; "\
"DROP DATABASE mylite_temp_info_probe;" | sed -n '1p')
expect_value "created temporary table dynamic count observation" "1" "$dynamic_count"

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'INNODB_TEMP_TABLE_INFO';")
expect_value "innodb temp table info system table row" \
    "information_schema	INNODB_TEMP_TABLE_INFO	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="INNODB_TEMP_TABLE_INFO	TABLE_ID	1		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TEMP_TABLE_INFO	NAME	2		YES	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
INNODB_TEMP_TABLE_INFO	N_COLS	3		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int unsigned	select
INNODB_TEMP_TABLE_INFO	SPACE	4		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int unsigned	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_TEMP_TABLE_INFO' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "innodb temp table info columns metadata" "$expected_columns_metadata" "$columns_metadata"

printf '%s\n' "mysql_baseline_information_schema_innodb_temp_table_info_expectations: ok"
