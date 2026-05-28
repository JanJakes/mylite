#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_optimizer_trace_expectations: $1" >&2
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

trace_variable=$(run_mysql 'SELECT @@optimizer_trace;')
expect_value "default optimizer trace variable" "enabled=off,one_line=off" "$trace_variable"

count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;")
expect_value "default optimizer trace count" "0" "$count"

case_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.optimizer_trace;")
expect_value "case-insensitive table name count" "0" "$case_count"

status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "optimizer trace status" "0	-1" "$status"

dynamic_count=$(run_mysql \
    "SET optimizer_trace='enabled=on'; "\
"SELECT 1; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE WHERE QUERY = 'SELECT 1';" | tail -n 1)
expect_value "enabled optimizer trace dynamic count" "1" "$dynamic_count"

dynamic_row=$(run_mysql \
    "SET optimizer_trace='enabled=on'; "\
"SELECT 1; "\
"SELECT QUERY,MISSING_BYTES_BEYOND_MAX_MEM_SIZE,INSUFFICIENT_PRIVILEGES,LEFT(TRACE,1) "\
"FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE WHERE QUERY = 'SELECT 1';" | tail -n 1)
expect_value "enabled optimizer trace dynamic row observation" "SELECT 1	0	0	{" "$dynamic_row"

alias_row=$(run_mysql \
    "SET optimizer_trace='enabled=on'; "\
"SELECT 1; "\
"SELECT t.QUERY FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE AS t "\
"WHERE t.QUERY = 'SELECT 1' LIMIT 1;" | tail -n 1)
expect_value "optimizer trace alias observation" "SELECT 1" "$alias_row"

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'OPTIMIZER_TRACE';")
expect_value "optimizer trace system table row" \
    "information_schema	OPTIMIZER_TRACE	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="OPTIMIZER_TRACE	QUERY	1		NO	varchar	21845	65535	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(65535)	select
OPTIMIZER_TRACE	TRACE	2		NO	varchar	21845	65535	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(65535)	select
OPTIMIZER_TRACE	MISSING_BYTES_BEYOND_MAX_MEM_SIZE	3		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
OPTIMIZER_TRACE	INSUFFICIENT_PRIVILEGES	4		NO	tinyint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	tinyint(1)	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'OPTIMIZER_TRACE' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "optimizer trace columns metadata" "$expected_columns_metadata" "$columns_metadata"

printf '%s\n' "mysql_baseline_information_schema_optimizer_trace_expectations: ok"
