#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_profiling_expectations: $1" >&2
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

profiling_variable=$(run_mysql 'SELECT @@profiling;')
expect_value "default profiling variable" "0" "$profiling_variable"

profiling_history_size=$(run_mysql 'SELECT @@profiling_history_size;')
expect_value "default profiling history size" "15" "$profiling_history_size"

count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROFILING;")
expect_value "default profiling count" "0" "$count"

case_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.profiling;")
expect_value "case-insensitive table name count" "0" "$case_count"

warning_status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROFILING; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "profiling warning status" "1	-1" "$warning_status"

warning_row=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.PROFILING; "\
"SHOW WARNINGS;" | tail -n 1)
expect_value "profiling deprecation warning" \
    "Warning	1287	'INFORMATION_SCHEMA.PROFILING' is deprecated and will be removed in a future release. Please use Performance Schema instead" \
    "$warning_row"

limit_zero_status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.PROFILING LIMIT 0; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "profiling LIMIT 0 warning status" "0	-1" "$limit_zero_status"

alias_warning_status=$(run_mysql \
    "SELECT p.QUERY_ID FROM INFORMATION_SCHEMA.PROFILING AS p "\
"WHERE p.QUERY_ID = -1; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "profiling alias empty warning status" "1	-1" "$alias_warning_status"

dynamic_count=$(run_mysql \
    "SET profiling=1; "\
"SELECT 1; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROFILING;" | tail -n 1)
case "$dynamic_count" in
    ''|*[!0-9]*) fail "enabled profiling dynamic count was not numeric: [$dynamic_count]" ;;
    *) ;;
esac
if [ "$dynamic_count" -le 0 ]; then
    fail "enabled profiling dynamic count should be positive, got [$dynamic_count]"
fi

dynamic_starting_count=$(run_mysql \
    "SET profiling=1; "\
"SELECT 1; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROFILING "\
"WHERE QUERY_ID = 1 AND STATE = 'starting';" | tail -n 1)
expect_value "enabled profiling starting row observation" "1" "$dynamic_starting_count"

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'PROFILING';")
expect_value "profiling system table row" \
    "information_schema	PROFILING	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="PROFILING	QUERY_ID	1		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
PROFILING	SEQ	2		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
PROFILING	STATE	3		NO	varchar	10	30	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(30)	select
PROFILING	DURATION	4		NO	decimal	NULL	NULL	NULL	NULL	NULL	NULL	NULL	decimal(905,0)	select
PROFILING	CPU_USER	5		YES	decimal	NULL	NULL	NULL	NULL	NULL	NULL	NULL	decimal(905,0)	select
PROFILING	CPU_SYSTEM	6		YES	decimal	NULL	NULL	NULL	NULL	NULL	NULL	NULL	decimal(905,0)	select
PROFILING	CONTEXT_VOLUNTARY	7		YES	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
PROFILING	CONTEXT_INVOLUNTARY	8		YES	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
PROFILING	BLOCK_OPS_IN	9		YES	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
PROFILING	BLOCK_OPS_OUT	10		YES	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
PROFILING	MESSAGES_SENT	11		YES	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
PROFILING	MESSAGES_RECEIVED	12		YES	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
PROFILING	PAGE_FAULTS_MAJOR	13		YES	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
PROFILING	PAGE_FAULTS_MINOR	14		YES	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
PROFILING	SWAPS	15		YES	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
PROFILING	SOURCE_FUNCTION	16		YES	varchar	10	30	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(30)	select
PROFILING	SOURCE_FILE	17		YES	varchar	6	20	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(20)	select
PROFILING	SOURCE_LINE	18		YES	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'PROFILING' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "profiling columns metadata" "$expected_columns_metadata" "$columns_metadata"

printf '%s\n' "mysql_baseline_information_schema_profiling_expectations: ok"
