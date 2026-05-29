#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_metrics_expectations: $1" >&2
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
"'INNODB_METRICS';")
expect_value "innodb metrics table kind" "INNODB_METRICS	SYSTEM VIEW" "$table_kind"

system_table_row=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH, "\
"AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_METRICS';")
expect_value "innodb metrics system table row" \
    "INNODB_METRICS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_METRICS' "\
"ORDER BY ORDINAL_POSITION;")
expected_columns_metadata="INNODB_METRICS	NAME	1		NO	varchar	64	193	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(193)	select
INNODB_METRICS	SUBSYSTEM	2		NO	varchar	64	193	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(193)	select
INNODB_METRICS	COUNT	3		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint	select
INNODB_METRICS	MAX_COUNT	4		YES	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint	select
INNODB_METRICS	MIN_COUNT	5		YES	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint	select
INNODB_METRICS	AVG_COUNT	6		YES	float	NULL	NULL	NULL	NULL	NULL	NULL	NULL	float(12,0)	select
INNODB_METRICS	COUNT_RESET	7		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint	select
INNODB_METRICS	MAX_COUNT_RESET	8		YES	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint	select
INNODB_METRICS	MIN_COUNT_RESET	9		YES	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint	select
INNODB_METRICS	AVG_COUNT_RESET	10		YES	float	NULL	NULL	NULL	NULL	NULL	NULL	NULL	float(12,0)	select
INNODB_METRICS	TIME_ENABLED	11		YES	datetime	NULL	NULL	NULL	NULL	NULL	NULL	NULL	datetime	select
INNODB_METRICS	TIME_DISABLED	12		YES	datetime	NULL	NULL	NULL	NULL	NULL	NULL	NULL	datetime	select
INNODB_METRICS	TIME_ELAPSED	13		YES	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint	select
INNODB_METRICS	TIME_RESET	14		YES	datetime	NULL	NULL	NULL	NULL	NULL	NULL	NULL	datetime	select
INNODB_METRICS	STATUS	15		NO	varchar	64	193	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(193)	select
INNODB_METRICS	TYPE	16		NO	varchar	64	193	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(193)	select
INNODB_METRICS	COMMENT	17		NO	varchar	64	193	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(193)	select"
expect_value "innodb metrics columns metadata" "$expected_columns_metadata" "$columns_metadata"

metrics_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_METRICS;")
expect_value "innodb metrics row count" "314" "$metrics_count"

enabled_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_METRICS WHERE STATUS = 'enabled';")
expect_positive_integer "innodb metrics enabled row count" "$enabled_count"

sample_rows=$(run_mysql \
    "SELECT NAME,SUBSYSTEM,STATUS,TYPE FROM INFORMATION_SCHEMA.INNODB_METRICS "\
"ORDER BY NAME LIMIT 6;")
expect_value "innodb metrics sample rows" \
    "adaptive_hash_pages_added	adaptive_hash_index	disabled	counter
adaptive_hash_pages_removed	adaptive_hash_index	disabled	counter
adaptive_hash_rows_added	adaptive_hash_index	disabled	counter
adaptive_hash_rows_deleted_no_hash_entry	adaptive_hash_index	disabled	counter
adaptive_hash_rows_removed	adaptive_hash_index	disabled	counter
adaptive_hash_rows_updated	adaptive_hash_index	disabled	counter" \
    "$sample_rows"

use_count=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM INNODB_METRICS WHERE STATUS = 'enabled';")
expect_positive_integer "unqualified innodb metrics enabled count" "$use_count"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_METRICS; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb metrics warning and row count status" "0	-1" "$status"

printf '%s\n' "mysql_baseline_information_schema_innodb_metrics_expectations: ok"
