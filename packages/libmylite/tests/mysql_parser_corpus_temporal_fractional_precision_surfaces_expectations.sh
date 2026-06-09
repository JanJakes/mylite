#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_temporal_fsp_surfaces_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_temporal_fractional_precision_surfaces_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names "$@"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    output=$(printf '%s\n' "$output" | tr '\t' '|')
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; "\
"CREATE TABLE fsp_cols ("\
"tm0 TIME(0), tm6 TIME(6), "\
"dt0 DATETIME(0), dt6 DATETIME(6), "\
"ts0 TIMESTAMP(0) NULL DEFAULT NULL, ts6 TIMESTAMP(6) NULL DEFAULT NULL);" >/dev/null

expect_output \
    "SHOW COLUMNS temporal fsp metadata" \
    "tm0|time|YES||NULL|
tm6|time(6)|YES||NULL|
dt0|datetime|YES||NULL|
dt6|datetime(6)|YES||NULL|
ts0|timestamp|YES||NULL|
ts6|timestamp(6)|YES||NULL|" \
    "USE ${DATABASE}; SHOW COLUMNS FROM fsp_cols;"

expect_output \
    "INFORMATION_SCHEMA.COLUMNS temporal fsp metadata" \
    "tm0|time|0|time
tm6|time|6|time(6)
dt0|datetime|0|datetime
dt6|datetime|6|datetime(6)
ts0|timestamp|0|timestamp
ts6|timestamp|6|timestamp(6)" \
    "SELECT COLUMN_NAME, DATA_TYPE, DATETIME_PRECISION, COLUMN_TYPE "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'fsp_cols' "\
"ORDER BY ORDINAL_POSITION;"

expect_output \
    "temporal function fsp output" \
    "22:13:20|22:13:20.000000|22:13:20|22:13:20.000000|22:13:20|22:13:20.000000|2023-11-14 22:13:20|2023-11-14 22:13:20.000000" \
    "SET timestamp = 1700000000; "\
"SELECT CURTIME(0), CURTIME(6), CURRENT_TIME(0), CURRENT_TIME(6), "\
"UTC_TIME(0), UTC_TIME(6), UTC_TIMESTAMP(0), UTC_TIMESTAMP(6);"

expect_output \
    "cast datetime fsp output" \
    "2024-01-02 03:04:05|2024-01-02 03:04:05.123456" \
    "SELECT CAST('2024-01-02 03:04:05.123456' AS DATETIME(0)), "\
"CAST('2024-01-02 03:04:05.123456' AS DATETIME(6));"

expect_error \
    "TIME fsp too large" \
    1426 \
    "42000" \
    "Too-big precision 7 specified for 't'. Maximum is 6." \
    "USE ${DATABASE}; CREATE TABLE bad_time (t TIME(7));"

expect_error \
    "DATETIME fsp too large" \
    1426 \
    "42000" \
    "Too-big precision 7 specified for 'd'. Maximum is 6." \
    "USE ${DATABASE}; CREATE TABLE bad_datetime (d DATETIME(7));"

expect_error \
    "TIMESTAMP fsp too large" \
    1426 \
    "42000" \
    "Too-big precision 7 specified for 'ts'. Maximum is 6." \
    "USE ${DATABASE}; CREATE TABLE bad_timestamp (ts TIMESTAMP(7));"

expect_error \
    "CURTIME fsp too large" \
    1426 \
    "42000" \
    "Too-big precision 7 specified for 'curtime'. Maximum is 6." \
    "SELECT CURTIME(7);"

expect_error \
    "CURRENT_TIME fsp too large" \
    1426 \
    "42000" \
    "Too-big precision 7 specified for 'curtime'. Maximum is 6." \
    "SELECT CURRENT_TIME(7);"

expect_error \
    "UTC_TIME fsp too large" \
    1426 \
    "42000" \
    "Too-big precision 7 specified for 'utc_time'. Maximum is 6." \
    "SELECT UTC_TIME(7);"

expect_error \
    "UTC_TIMESTAMP fsp too large" \
    1426 \
    "42000" \
    "Too-big precision 7 specified for 'utc_timestamp'. Maximum is 6." \
    "SELECT UTC_TIMESTAMP(7);"

expect_error \
    "CAST DATETIME fsp too large" \
    1426 \
    "42000" \
    "Too-big precision 7 specified for 'CAST'. Maximum is 6." \
    "SELECT CAST('2024-01-02 03:04:05' AS DATETIME(7));"

printf '%s\n' "mysql_parser_corpus_temporal_fractional_precision_surfaces_expectations: ok"
