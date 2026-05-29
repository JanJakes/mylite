#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_cached_indexes_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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

table_kind=$(run_mysql \
    "SHOW FULL TABLES FROM INFORMATION_SCHEMA WHERE Tables_in_INFORMATION_SCHEMA = "\
"'INNODB_CACHED_INDEXES';")
expect_value "innodb cached indexes table kind" \
    "INNODB_CACHED_INDEXES	SYSTEM VIEW" \
    "$table_kind"

system_table_row=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'INNODB_CACHED_INDEXES';")
expect_value "innodb cached indexes system table row" \
    "INNODB_CACHED_INDEXES	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="INNODB_CACHED_INDEXES	SPACE_ID	1		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int unsigned	select
INNODB_CACHED_INDEXES	INDEX_ID	2		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_CACHED_INDEXES	N_CACHED_PAGES	3		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_CACHED_INDEXES' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "innodb cached indexes columns metadata" \
    "$expected_columns_metadata" \
    "$columns_metadata"

count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CACHED_INDEXES;")
case "$count" in
    ''|*[!0-9]*) fail "cached index count was not numeric: [$count]" ;;
    0) fail "expected MySQL runtime to expose dynamic cached-index rows" ;;
    *) ;;
esac

positive_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_cached_indexes "\
"WHERE N_CACHED_PAGES > 0;")
expect_value "case-insensitive cached indexes positive count" "$count" "$positive_count"

sample=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CACHED_INDEXES "\
"WHERE SPACE_ID IS NOT NULL AND INDEX_ID IS NOT NULL AND N_CACHED_PAGES IS NOT NULL;")
expect_value "cached indexes non-null sample count" "$count" "$sample"

use_count=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM INNODB_CACHED_INDEXES WHERE INDEX_ID = 0;")
expect_value "unqualified cached indexes zero index count" "0" "$use_count"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CACHED_INDEXES; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb cached indexes status" "0	-1" "$status"

printf '%s\n' "mysql_baseline_information_schema_innodb_cached_indexes_expectations: ok"
