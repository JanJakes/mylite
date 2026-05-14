#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_information_schema_events_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_events_expectations: $1" >&2
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; CREATE TABLE ${DATABASE}.t(id INT);" >/dev/null

expected_events_columns="EVENT_CATALOG	EVENT_SCHEMA	EVENT_NAME	DEFINER	TIME_ZONE	EVENT_BODY	EVENT_DEFINITION	EVENT_TYPE	EXECUTE_AT	INTERVAL_VALUE	INTERVAL_FIELD	SQL_MODE	STARTS	ENDS	STATUS	ON_COMPLETION	CREATED	LAST_ALTERED	LAST_EXECUTED	EVENT_COMMENT	ORIGINATOR	CHARACTER_SET_CLIENT	COLLATION_CONNECTION	DATABASE_COLLATION"
count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.EVENTS WHERE EVENT_SCHEMA = '${DATABASE}';")
expect_value "empty events count" "0" "$count"

case_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.events WHERE EVENT_SCHEMA = '${DATABASE}';")
expect_value "case-insensitive table name count" "0" "$case_count"

status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.EVENTS WHERE EVENT_SCHEMA = '${DATABASE}'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "events status" "0	-1" "$status"

expect_error \
    "unknown events where column" \
    1054 \
    "42S22" \
    "Unknown column 'nope' in 'where clause'" \
    "SELECT EVENT_NAME FROM INFORMATION_SCHEMA.EVENTS WHERE nope = 'x';"

run_mysql \
    "CREATE EVENT ${DATABASE}.daily_event ON SCHEDULE EVERY 1 DAY DO SET @event_probe = 1;" \
    >/dev/null

events_output=$(run_mysql_with_headers \
    "SELECT * FROM INFORMATION_SCHEMA.EVENTS WHERE EVENT_SCHEMA = '${DATABASE}';")
expect_value "events headers" "$expected_events_columns" "$(printf '%s\n' "$events_output" | sed -n '1p')"
alias_output=$(run_mysql_with_headers \
    "SELECT ev.EVENT_NAME FROM INFORMATION_SCHEMA.EVENTS AS ev "\
"WHERE ev.EVENT_SCHEMA = '${DATABASE}' ORDER BY ev.EVENT_NAME LIMIT 1;")
expect_value "alias headers" "EVENT_NAME" "$(printf '%s\n' "$alias_output" | sed -n '1p')"

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'EVENTS';")
expect_value "events system table row" \
    "information_schema	EVENTS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="EVENTS	EVENT_CATALOG	1	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
EVENTS	EVENT_SCHEMA	2	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
EVENTS	EVENT_NAME	3	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
EVENTS	DEFINER	4	NULL	NO	varchar	288	864	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(288)	select
EVENTS	TIME_ZONE	5	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
EVENTS	EVENT_BODY	6		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
EVENTS	EVENT_DEFINITION	7	NULL	NO	longtext	4294967295	4294967295	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	longtext	select
EVENTS	EVENT_TYPE	8		NO	varchar	9	27	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(9)	select
EVENTS	EXECUTE_AT	9	NULL	YES	datetime	NULL	NULL	NULL	NULL	0	NULL	NULL	datetime	select
EVENTS	INTERVAL_VALUE	10	NULL	YES	varchar	256	768	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(256)	select
EVENTS	INTERVAL_FIELD	11	NULL	YES	enum	18	54	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	enum('YEAR','QUARTER','MONTH','DAY','HOUR','MINUTE','WEEK','SECOND','MICROSECOND','YEAR_MONTH','DAY_HOUR','DAY_MINUTE','DAY_SECOND','HOUR_MINUTE','HOUR_SECOND','MINUTE_SECOND','DAY_MICROSECOND','HOUR_MICROSECOND','MINUTE_MICROSECOND','SECOND_MICROSECOND')	select
EVENTS	SQL_MODE	12	NULL	NO	set	520	1560	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES','IGNORE_SPACE','NOT_USED','ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION','NO_DIR_IN_CREATE','NOT_USED_9','NOT_USED_10','NOT_USED_11','NOT_USED_12','NOT_USED_13','NOT_USED_14','NOT_USED_15','NOT_USED_16','NOT_USED_17','NOT_USED_18','ANSI','NO_AUTO_VALUE_ON_ZERO','NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES','STRICT_ALL_TABLES','NO_ZERO_IN_DATE','NO_ZERO_DATE','ALLOW_INVALID_DATES','ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL','NOT_USED_29','HIGH_NOT_PRECEDENCE','NO_ENGINE_SUBSTITUTION','PAD_CHAR_TO_FULL_LENGTH','TIME_TRUNCATE_FRACTIONAL')	select
EVENTS	STARTS	13	NULL	YES	datetime	NULL	NULL	NULL	NULL	0	NULL	NULL	datetime	select
EVENTS	ENDS	14	NULL	YES	datetime	NULL	NULL	NULL	NULL	0	NULL	NULL	datetime	select
EVENTS	STATUS	15		NO	varchar	21	63	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(21)	select
EVENTS	ON_COMPLETION	16		NO	varchar	12	36	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(12)	select
EVENTS	CREATED	17	NULL	NO	timestamp	NULL	NULL	NULL	NULL	0	NULL	NULL	timestamp	select
EVENTS	LAST_ALTERED	18	NULL	NO	timestamp	NULL	NULL	NULL	NULL	0	NULL	NULL	timestamp	select
EVENTS	LAST_EXECUTED	19	NULL	YES	datetime	NULL	NULL	NULL	NULL	0	NULL	NULL	datetime	select
EVENTS	EVENT_COMMENT	20	NULL	NO	varchar	2048	6144	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(2048)	select
EVENTS	ORIGINATOR	21	NULL	NO	int	NULL	NULL	10	0	NULL	NULL	NULL	int unsigned	select
EVENTS	CHARACTER_SET_CLIENT	22	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
EVENTS	COLLATION_CONNECTION	23	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
EVENTS	DATABASE_COLLATION	24	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'EVENTS' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "events columns metadata" "$expected_columns_metadata" "$columns_metadata"

real_event_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.EVENTS WHERE EVENT_SCHEMA = '${DATABASE}';")
expect_value "mysql real event observation" "1" "$real_event_count"

printf '%s\n' "mysql_baseline_information_schema_events_expectations: ok"
