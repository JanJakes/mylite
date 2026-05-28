#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_ft_index_expectations: $1" >&2
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
"IN ('INNODB_FT_INDEX_CACHE', 'INNODB_FT_INDEX_TABLE');")
expect_value "innodb ft index table kinds" \
    "INNODB_FT_INDEX_CACHE	SYSTEM VIEW
INNODB_FT_INDEX_TABLE	SYSTEM VIEW" \
    "$table_kinds"

cache_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE;")
expect_value "default innodb ft index cache count" "0" "$cache_count"

table_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE;")
expect_value "default innodb ft index table count" "0" "$table_count"

case_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_ft_index_cache;")
expect_value "case-insensitive index cache table name count" "0" "$case_count"

use_counts=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM INNODB_FT_INDEX_CACHE; "\
"SELECT COUNT(*) FROM INNODB_FT_INDEX_TABLE;")
expect_value "unqualified innodb ft index counts" "0
0" "$use_counts"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb ft index status" "0	-1" "$status"

cache_alias_rows=$(run_mysql \
    "SELECT c.WORD, c.DOC_ID, c.POSITION FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE AS c "\
"WHERE c.WORD = '';")
expect_value "innodb ft index cache alias observation" "" "$cache_alias_rows"

table_alias_rows=$(run_mysql \
    "SELECT t.WORD, t.DOC_ID, t.POSITION FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE AS t "\
"WHERE t.DOC_ID = 1;")
expect_value "innodb ft index table alias observation" "" "$table_alias_rows"

system_table_rows=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('INNODB_FT_INDEX_CACHE', 'INNODB_FT_INDEX_TABLE') "\
"ORDER BY TABLE_NAME;")
expect_value "innodb ft index system table rows" \
    "information_schema	INNODB_FT_INDEX_CACHE	SYSTEM VIEW	NULL	10	NULL	0	0	NULL
information_schema	INNODB_FT_INDEX_TABLE	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_rows"

expected_columns_metadata="INNODB_FT_INDEX_CACHE	WORD	1		NO	varchar	112	337	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(337)	select
INNODB_FT_INDEX_CACHE	FIRST_DOC_ID	2		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_FT_INDEX_CACHE	LAST_DOC_ID	3		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_FT_INDEX_CACHE	DOC_COUNT	4		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_FT_INDEX_CACHE	DOC_ID	5		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_FT_INDEX_CACHE	POSITION	6		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_FT_INDEX_TABLE	WORD	1		NO	varchar	112	337	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(337)	select
INNODB_FT_INDEX_TABLE	FIRST_DOC_ID	2		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_FT_INDEX_TABLE	LAST_DOC_ID	3		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_FT_INDEX_TABLE	DOC_COUNT	4		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_FT_INDEX_TABLE	DOC_ID	5		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_FT_INDEX_TABLE	POSITION	6		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('INNODB_FT_INDEX_CACHE', 'INNODB_FT_INDEX_TABLE') "\
"ORDER BY TABLE_NAME, ORDINAL_POSITION;")
expect_value "innodb ft index columns metadata" "$expected_columns_metadata" "$columns_metadata"

printf '%s\n' "mysql_baseline_information_schema_innodb_ft_index_expectations: ok"
