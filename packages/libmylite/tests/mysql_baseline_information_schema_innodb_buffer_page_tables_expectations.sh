#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_buffer_page_tables_expectations: $1" >&2
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

expect_positive_integer() {
    label=$1
    actual=$2

    case "$actual" in
        ''|*[!0-9]*) fail "$label was not numeric: [$actual]" ;;
        0) fail "$label was zero" ;;
        *) ;;
    esac
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

table_kind=$(run_mysql \
    "SHOW FULL TABLES FROM INFORMATION_SCHEMA WHERE Tables_in_INFORMATION_SCHEMA IN "\
"('INNODB_BUFFER_PAGE','INNODB_BUFFER_PAGE_LRU');")
expect_value "innodb buffer page table kinds" \
    "INNODB_BUFFER_PAGE	SYSTEM VIEW
INNODB_BUFFER_PAGE_LRU	SYSTEM VIEW" \
    "$table_kind"

system_table_rows=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('INNODB_BUFFER_PAGE','INNODB_BUFFER_PAGE_LRU') ORDER BY TABLE_NAME;")
expect_value "innodb buffer page system table rows" \
    "INNODB_BUFFER_PAGE	SYSTEM VIEW	NULL	10	NULL	0	0	NULL
INNODB_BUFFER_PAGE_LRU	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_rows"

expected_columns_metadata="INNODB_BUFFER_PAGE	POOL_ID	1		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE	BLOCK_ID	2		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE	SPACE	3		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE	PAGE_NUMBER	4		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE	PAGE_TYPE	5		YES	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
INNODB_BUFFER_PAGE	FLUSH_TYPE	6		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE	FIX_COUNT	7		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE	IS_HASHED	8		YES	varchar	1	3	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
INNODB_BUFFER_PAGE	NEWEST_MODIFICATION	9		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE	OLDEST_MODIFICATION	10		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE	ACCESS_TIME	11		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE	TABLE_NAME	12		YES	varchar	341	1024	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(1024)	select
INNODB_BUFFER_PAGE	INDEX_NAME	13		YES	varchar	341	1024	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(1024)	select
INNODB_BUFFER_PAGE	NUMBER_RECORDS	14		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE	DATA_SIZE	15		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE	COMPRESSED_SIZE	16		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE	PAGE_STATE	17		YES	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
INNODB_BUFFER_PAGE	IO_FIX	18		YES	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
INNODB_BUFFER_PAGE	IS_OLD	19		YES	varchar	1	3	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
INNODB_BUFFER_PAGE	FREE_PAGE_CLOCK	20		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE	IS_STALE	21		YES	varchar	1	3	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
INNODB_BUFFER_PAGE_LRU	POOL_ID	1		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE_LRU	LRU_POSITION	2		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE_LRU	SPACE	3		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE_LRU	PAGE_NUMBER	4		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE_LRU	PAGE_TYPE	5		YES	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
INNODB_BUFFER_PAGE_LRU	FLUSH_TYPE	6		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE_LRU	FIX_COUNT	7		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE_LRU	IS_HASHED	8		YES	varchar	1	3	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
INNODB_BUFFER_PAGE_LRU	NEWEST_MODIFICATION	9		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE_LRU	OLDEST_MODIFICATION	10		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE_LRU	ACCESS_TIME	11		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE_LRU	TABLE_NAME	12		YES	varchar	341	1024	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(1024)	select
INNODB_BUFFER_PAGE_LRU	INDEX_NAME	13		YES	varchar	341	1024	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(1024)	select
INNODB_BUFFER_PAGE_LRU	NUMBER_RECORDS	14		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE_LRU	DATA_SIZE	15		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE_LRU	COMPRESSED_SIZE	16		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_PAGE_LRU	COMPRESSED	17		YES	varchar	1	3	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
INNODB_BUFFER_PAGE_LRU	IO_FIX	18		YES	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
INNODB_BUFFER_PAGE_LRU	IS_OLD	19		YES	varchar	1	3	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
INNODB_BUFFER_PAGE_LRU	FREE_PAGE_CLOCK	20		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('INNODB_BUFFER_PAGE','INNODB_BUFFER_PAGE_LRU') "\
"ORDER BY TABLE_NAME,ORDINAL_POSITION;")
expect_value "innodb buffer page columns metadata" \
    "$expected_columns_metadata" \
    "$columns_metadata"

buffer_page_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE;")
expect_positive_integer "buffer page row count" "$buffer_page_count"

buffer_page_lru_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU;")
expect_positive_integer "buffer page lru row count" "$buffer_page_lru_count"

buffer_page_pool_zero=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE WHERE POOL_ID = 0;")
expect_positive_integer "buffer page pool zero row count" "$buffer_page_pool_zero"

buffer_page_lru_pool_zero=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU WHERE POOL_ID = 0;")
expect_positive_integer "buffer page lru pool zero row count" "$buffer_page_lru_pool_zero"

use_count=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM INNODB_BUFFER_PAGE WHERE PAGE_TYPE = 'INDEX';")
case "$use_count" in
    ''|*[!0-9]*) fail "unqualified buffer page index count was not numeric: [$use_count]" ;;
    *) ;;
esac

use_lru_count=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM INNODB_BUFFER_PAGE_LRU WHERE COMPRESSED = 'NO';")
case "$use_lru_count" in
    ''|*[!0-9]*) fail "unqualified buffer page lru compressed count was not numeric: [$use_lru_count]" ;;
    *) ;;
esac

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb buffer page table status" "0	-1" "$status"

printf '%s\n' "mysql_baseline_information_schema_innodb_buffer_page_tables_expectations: ok"
