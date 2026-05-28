#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_ft_config_expectations: $1" >&2
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

table_kind=$(run_mysql "SHOW FULL TABLES FROM INFORMATION_SCHEMA LIKE 'INNODB_FT_CONFIG';")
expect_value "innodb ft config table kind" "INNODB_FT_CONFIG	SYSTEM VIEW" "$table_kind"

count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_CONFIG;")
expect_value "default innodb ft config count" "0" "$count"

case_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_ft_config;")
expect_value "case-insensitive table name count" "0" "$case_count"

use_count=$(run_mysql "USE information_schema; SELECT COUNT(*) FROM INNODB_FT_CONFIG;")
expect_value "unqualified innodb ft config count" "0" "$use_count"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_CONFIG; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb ft config status" "0	-1" "$status"

quoted_projection=$(run_mysql \
    "SELECT \`KEY\`, VALUE FROM INFORMATION_SCHEMA.INNODB_FT_CONFIG "\
"WHERE VALUE = '';")
expect_value "innodb ft config quoted key projection" "" "$quoted_projection"

alias_rows=$(run_mysql \
    "SELECT c.VALUE FROM INFORMATION_SCHEMA.INNODB_FT_CONFIG AS c "\
"WHERE c.VALUE = '';")
expect_value "innodb ft config alias observation" "" "$alias_rows"

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'INNODB_FT_CONFIG';")
expect_value "innodb ft config system table row" \
    "information_schema	INNODB_FT_CONFIG	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="INNODB_FT_CONFIG	KEY	1		NO	varchar	64	193	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(193)	select
INNODB_FT_CONFIG	VALUE	2		NO	varchar	64	193	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(193)	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_FT_CONFIG' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "innodb ft config columns metadata" "$expected_columns_metadata" "$columns_metadata"

printf '%s\n' "mysql_baseline_information_schema_innodb_ft_config_expectations: ok"
