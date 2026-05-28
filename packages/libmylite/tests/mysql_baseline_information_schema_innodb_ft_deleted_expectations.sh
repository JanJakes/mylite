#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_ft_deleted_expectations: $1" >&2
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
"IN ('INNODB_FT_BEING_DELETED', 'INNODB_FT_DELETED');")
expect_value "innodb ft deleted table kinds" \
    "INNODB_FT_BEING_DELETED	SYSTEM VIEW
INNODB_FT_DELETED	SYSTEM VIEW" \
    "$table_kinds"

being_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_BEING_DELETED;")
expect_value "default innodb ft being deleted count" "0" "$being_count"

deleted_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_DELETED;")
expect_value "default innodb ft deleted count" "0" "$deleted_count"

case_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_ft_deleted;")
expect_value "case-insensitive deleted table name count" "0" "$case_count"

use_counts=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM INNODB_FT_BEING_DELETED; "\
"SELECT COUNT(*) FROM INNODB_FT_DELETED;")
expect_value "unqualified innodb ft deleted counts" "0
0" "$use_counts"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_DELETED; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb ft deleted status" "0	-1" "$status"

being_alias_rows=$(run_mysql \
    "SELECT b.DOC_ID FROM INFORMATION_SCHEMA.INNODB_FT_BEING_DELETED AS b "\
"WHERE b.DOC_ID = 1;")
expect_value "innodb ft being deleted alias observation" "" "$being_alias_rows"

deleted_alias_rows=$(run_mysql \
    "SELECT d.DOC_ID FROM INFORMATION_SCHEMA.INNODB_FT_DELETED AS d "\
"WHERE d.DOC_ID = 1;")
expect_value "innodb ft deleted alias observation" "" "$deleted_alias_rows"

system_table_rows=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('INNODB_FT_BEING_DELETED', 'INNODB_FT_DELETED') "\
"ORDER BY TABLE_NAME;")
expect_value "innodb ft deleted system table rows" \
    "information_schema	INNODB_FT_BEING_DELETED	SYSTEM VIEW	NULL	10	NULL	0	0	NULL
information_schema	INNODB_FT_DELETED	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_rows"

expected_columns_metadata="INNODB_FT_BEING_DELETED	DOC_ID	1		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_FT_DELETED	DOC_ID	1		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('INNODB_FT_BEING_DELETED', 'INNODB_FT_DELETED') "\
"ORDER BY TABLE_NAME, ORDINAL_POSITION;")
expect_value "innodb ft deleted columns metadata" "$expected_columns_metadata" "$columns_metadata"

printf '%s\n' "mysql_baseline_information_schema_innodb_ft_deleted_expectations: ok"
