#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_buffer_pool_stats_expectations: $1" >&2
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
    "SHOW FULL TABLES FROM INFORMATION_SCHEMA WHERE Tables_in_INFORMATION_SCHEMA = "\
"'INNODB_BUFFER_POOL_STATS';")
expect_value "innodb buffer pool stats table kind" \
    "INNODB_BUFFER_POOL_STATS	SYSTEM VIEW" \
    "$table_kind"

system_table_row=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'INNODB_BUFFER_POOL_STATS';")
expect_value "innodb buffer pool stats system table row" \
    "INNODB_BUFFER_POOL_STATS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="INNODB_BUFFER_POOL_STATS	POOL_ID	1		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	POOL_SIZE	2		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	FREE_BUFFERS	3		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	DATABASE_PAGES	4		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	OLD_DATABASE_PAGES	5		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	MODIFIED_DATABASE_PAGES	6		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	PENDING_DECOMPRESS	7		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	PENDING_READS	8		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	PENDING_FLUSH_LRU	9		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	PENDING_FLUSH_LIST	10		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	PAGES_MADE_YOUNG	11		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	PAGES_NOT_MADE_YOUNG	12		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	PAGES_MADE_YOUNG_RATE	13		NO	float	NULL	NULL	NULL	NULL	NULL	NULL	NULL	float(12,0)	select
INNODB_BUFFER_POOL_STATS	PAGES_MADE_NOT_YOUNG_RATE	14		NO	float	NULL	NULL	NULL	NULL	NULL	NULL	NULL	float(12,0)	select
INNODB_BUFFER_POOL_STATS	NUMBER_PAGES_READ	15		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	NUMBER_PAGES_CREATED	16		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	NUMBER_PAGES_WRITTEN	17		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	PAGES_READ_RATE	18		NO	float	NULL	NULL	NULL	NULL	NULL	NULL	NULL	float(12,0)	select
INNODB_BUFFER_POOL_STATS	PAGES_CREATE_RATE	19		NO	float	NULL	NULL	NULL	NULL	NULL	NULL	NULL	float(12,0)	select
INNODB_BUFFER_POOL_STATS	PAGES_WRITTEN_RATE	20		NO	float	NULL	NULL	NULL	NULL	NULL	NULL	NULL	float(12,0)	select
INNODB_BUFFER_POOL_STATS	NUMBER_PAGES_GET	21		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	HIT_RATE	22		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	YOUNG_MAKE_PER_THOUSAND_GETS	23		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	NOT_YOUNG_MAKE_PER_THOUSAND_GETS	24		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	NUMBER_PAGES_READ_AHEAD	25		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	NUMBER_READ_AHEAD_EVICTED	26		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	READ_AHEAD_RATE	27		NO	float	NULL	NULL	NULL	NULL	NULL	NULL	NULL	float(12,0)	select
INNODB_BUFFER_POOL_STATS	READ_AHEAD_EVICTED_RATE	28		NO	float	NULL	NULL	NULL	NULL	NULL	NULL	NULL	float(12,0)	select
INNODB_BUFFER_POOL_STATS	LRU_IO_TOTAL	29		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	LRU_IO_CURRENT	30		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	UNCOMPRESS_TOTAL	31		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_BUFFER_POOL_STATS	UNCOMPRESS_CURRENT	32		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_BUFFER_POOL_STATS' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "innodb buffer pool stats columns metadata" \
    "$expected_columns_metadata" \
    "$columns_metadata"

count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS;")
expect_positive_integer "buffer pool stats row count" "$count"

pool_zero_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS WHERE POOL_ID = 0;")
expect_value "buffer pool stats pool zero row count" "1" "$pool_zero_count"

non_null_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS "\
"WHERE POOL_ID IS NOT NULL AND POOL_SIZE IS NOT NULL AND FREE_BUFFERS IS NOT NULL "\
"AND DATABASE_PAGES IS NOT NULL AND PAGES_READ_RATE IS NOT NULL "\
"AND READ_AHEAD_EVICTED_RATE IS NOT NULL AND UNCOMPRESS_CURRENT IS NOT NULL;")
expect_value "buffer pool stats non-null representative count" "$count" "$non_null_count"

use_count=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM INNODB_BUFFER_POOL_STATS WHERE POOL_ID = 0;")
expect_value "unqualified buffer pool stats pool zero count" "1" "$use_count"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb buffer pool stats status" "0	-1" "$status"

printf '%s\n' "mysql_baseline_information_schema_innodb_buffer_pool_stats_expectations: ok"
