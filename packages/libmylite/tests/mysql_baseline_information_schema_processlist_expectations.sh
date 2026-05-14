#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_information_schema_processlist_expectations_$$"
PROCESSLIST_WARNING="'INFORMATION_SCHEMA.PROCESSLIST' is deprecated and will be removed in a future release. Please use performance_schema.processlist instead"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_processlist_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5
    shift 5

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
        fail "$label: expected error $code/$state, command succeeded with [$output]"
    fi

    case "$output" in
        *"ERROR $code ($state)"*"$message"*) ;;
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

field_from_row() {
    row=$1
    field_index=$2

    printf '%s\n' "$row" | awk -F '\t' -v field="$field_index" '{ print $field; exit }'
}

expect_decimal_field() {
    label=$1
    value=$2

    case "$value" in
        ''|*[!0-9]*) fail "$label: expected decimal text, got [$value]" ;;
    esac
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_value \
    "default processlist implementation mode" \
    "0" \
    "$(run_mysql 'SELECT @@performance_schema_show_processlist;')"

cleanup
expect_value \
    "processlist no selected db" \
    "NULL" \
    "$(run_mysql 'SELECT DB FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = CONNECTION_ID();')"

run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

processlist_sql="SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = CONNECTION_ID()"
processlist_output=$(run_mysql_with_headers "USE ${DATABASE}; ${processlist_sql};")
expect_value \
    "processlist headers" \
    "ID	USER	HOST	DB	COMMAND	TIME	STATE	INFO" \
    "$(printf '%s\n' "$processlist_output" | sed -n '1p')"

processlist_row=$(printf '%s\n' "$processlist_output" | sed -n '2p')
expect_decimal_field "processlist id" "$(field_from_row "$processlist_row" 1)"
expect_value "processlist user" "root" "$(field_from_row "$processlist_row" 2)"
case "$(field_from_row "$processlist_row" 3)" in
    *:*) ;;
    *) fail "processlist host did not include a TCP client port: [$processlist_row]" ;;
esac
expect_value "processlist selected db" "$DATABASE" "$(field_from_row "$processlist_row" 4)"
expect_value "processlist command" "Query" "$(field_from_row "$processlist_row" 5)"
expect_value "processlist time" "0" "$(field_from_row "$processlist_row" 6)"
expect_value "processlist state" "executing" "$(field_from_row "$processlist_row" 7)"
expect_value "processlist info" "$processlist_sql" "$(field_from_row "$processlist_row" 8)"

status=$(run_mysql "USE ${DATABASE}; ${processlist_sql}; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "processlist warnings and row count" "1	-1" "$status"

warning=$(run_mysql "USE ${DATABASE}; ${processlist_sql}; SHOW WARNINGS;" | tail -n 1)
expect_value "processlist deprecation warning" "Warning	1287	${PROCESSLIST_WARNING}" "$warning"

no_match_status=$(run_mysql \
    "USE ${DATABASE}; SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = -1; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "processlist no-match warnings and row count" "0	-1" "$no_match_status"

limit_zero_status=$(run_mysql \
    "USE ${DATABASE}; SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST LIMIT 0; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "processlist limit zero warnings and row count" "0	-1" "$limit_zero_status"

count=$(run_mysql \
    "USE ${DATABASE}; SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = CONNECTION_ID();")
expect_value "processlist current row count" "1" "$count"

count_status=$(run_mysql \
    "USE ${DATABASE}; SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = CONNECTION_ID(); "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "processlist count warnings and row count" "1	-1" "$count_status"

no_match_count_status=$(run_mysql \
    "USE ${DATABASE}; SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = -1; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "processlist no-match count warnings and row count" "0	-1" "$no_match_count_status"

case_count=$(run_mysql \
    "USE ${DATABASE}; SELECT COUNT(*) FROM INFORMATION_SCHEMA.processlist WHERE ID = CONNECTION_ID();")
expect_value "case-insensitive processlist table count" "1" "$case_count"

alias_output=$(run_mysql_with_headers \
    "USE ${DATABASE}; SELECT p.ID FROM INFORMATION_SCHEMA.PROCESSLIST AS p "\
"WHERE p.ID = CONNECTION_ID() ORDER BY p.ID DESC LIMIT 1;")
expect_value "processlist alias header" "ID" "$(printf '%s\n' "$alias_output" | sed -n '1p')"
expect_decimal_field "processlist alias row" "$(printf '%s\n' "$alias_output" | sed -n '2p')"

long_comment=$(printf 'x%.0s' $(seq 1 160))
long_sql="SELECT /* ${long_comment} */ INFO FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = CONNECTION_ID()"
long_info=$(run_mysql "USE ${DATABASE}; ${long_sql};")
expect_value "processlist untruncated info" "$long_sql" "$long_info"

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'PROCESSLIST';")
expect_value "processlist system table row" \
    "information_schema	PROCESSLIST	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="PROCESSLIST	ID	1		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
PROCESSLIST	USER	2		NO	varchar	10	32	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(32)	select
PROCESSLIST	HOST	3		NO	varchar	87	261	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(261)	select
PROCESSLIST	DB	4		YES	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
PROCESSLIST	COMMAND	5		NO	varchar	5	16	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(16)	select
PROCESSLIST	TIME	6		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
PROCESSLIST	STATE	7		YES	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
PROCESSLIST	INFO	8		YES	varchar	21845	65535	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(65535)	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'PROCESSLIST' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "processlist columns metadata" "$expected_columns_metadata" "$columns_metadata"

expect_error \
    "unknown processlist projection column" \
    1054 \
    "42S22" \
    "Unknown column 'nope' in 'field list'" \
    "SELECT nope FROM INFORMATION_SCHEMA.PROCESSLIST;"

expect_error \
    "unknown processlist where column" \
    1054 \
    "42S22" \
    "Unknown column 'nope' in 'where clause'" \
    "SELECT ID FROM INFORMATION_SCHEMA.PROCESSLIST WHERE nope = 1;"

expect_error \
    "unknown processlist order column" \
    1054 \
    "42S22" \
    "Unknown column 'nope' in 'order clause'" \
    "SELECT ID FROM INFORMATION_SCHEMA.PROCESSLIST ORDER BY nope;"

printf '%s\n' "mysql_baseline_information_schema_processlist_expectations: ok"
