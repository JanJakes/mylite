#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_unix_timestamp_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_unix_timestamp_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw "$@"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
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

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
    fi
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

current_expected=$(cat <<EXPECTED
1704067200	2024-01-01 00:00:00
-1	0
1704067200	2024-01-01 01:00:00
EXPECTED
)
expect_output \
    "no-argument statement timestamp and time zone" \
    "$current_expected" \
    "SET time_zone = '+00:00'; SET timestamp = 1704067200; "\
"SELECT UNIX_TIMESTAMP(), NOW(); SELECT ROW_COUNT(), @@warning_count; "\
"SET time_zone = '+01:00'; SELECT UNIX_TIMESTAMP(), NOW();" \
    "$DATABASE"

timezone_expected=$(cat <<EXPECTED
1	0	1704063600	1704063600
9001	18001
EXPECTED
)
expect_output \
    "literal time zone conversion" \
    "$timezone_expected" \
    "SET time_zone = '+01:00'; "\
"SELECT UNIX_TIMESTAMP('1970-01-01 01:00:01'), "\
"UNIX_TIMESTAMP('1970-01-01 00:00:01'), UNIX_TIMESTAMP('2024-01-01'), "\
"UNIX_TIMESTAMP('2024-01-01 00:00:00'); SET time_zone = '-02:30'; "\
"SELECT UNIX_TIMESTAMP('1970-01-01 00:00:01'), "\
"UNIX_TIMESTAMP('1970-01-01 02:30:01');" \
    "$DATABASE"

range_expected=$(cat <<EXPECTED
NULL	0	1	0	2147483648	32536771199	0
0
EXPECTED
)
expect_output \
    "literal null and range values" \
    "$range_expected" \
    "SET time_zone = '+00:00'; "\
"SELECT UNIX_TIMESTAMP(NULL), UNIX_TIMESTAMP('1970-01-01 00:00:00'), "\
"UNIX_TIMESTAMP('1970-01-01 00:00:01'), UNIX_TIMESTAMP('1969-12-31 23:59:59'), "\
"UNIX_TIMESTAMP('2038-01-19 03:14:08'), "\
"UNIX_TIMESTAMP('3001-01-18 23:59:59'), "\
"UNIX_TIMESTAMP('3001-01-19 03:14:07'); SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

invalid_expected=$(cat <<EXPECTED
0.000000	0	0
Warning	1292	Incorrect datetime value: 'bad'
Warning	1292	Incorrect datetime value: '0000-00-00'
2
EXPECTED
)
expect_output \
    "invalid string warnings" \
    "$invalid_expected" \
    "SET time_zone = '+00:00'; "\
"SELECT UNIX_TIMESTAMP('bad'), UNIX_TIMESTAMP('0000-00-00'), "\
"UNIX_TIMESTAMP('2024-00-01'); SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

headers_expected=$(cat <<EXPECTED
UNIX_TIMESTAMP()	ts
1704067200	1
EXPECTED
)
expect_output_with_headers \
    "labels and whitespace" \
    "$headers_expected" \
    "SET time_zone = '+00:00'; SET timestamp = 1704067200; "\
"SELECT UNIX_TIMESTAMP(), UNIX_TIMESTAMP ('1970-01-01 00:00:01') AS ts FROM DUAL;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	86400	1	1
2	NULL	NULL	NULL
1	82800	0	1
2	NULL	NULL	NULL
0
EXPECTED
)
expect_output \
    "descriptor date datetime timestamp values" \
    "$table_expected" \
    "SET time_zone = '+00:00'; "\
"CREATE TABLE t(id INT, d DATE, dt DATETIME, ts TIMESTAMP NULL); "\
"INSERT INTO t VALUES "\
"(1, '1970-01-02', '1970-01-01 00:00:01', '1970-01-01 00:00:01'), "\
"(2, NULL, NULL, NULL); "\
"SELECT id, UNIX_TIMESTAMP(d), UNIX_TIMESTAMP(dt), UNIX_TIMESTAMP(ts) FROM t ORDER BY id; "\
"SET time_zone = '+01:00'; "\
"SELECT id, UNIX_TIMESTAMP(d), UNIX_TIMESTAMP(dt), UNIX_TIMESTAMP(ts) FROM t ORDER BY id; "\
"SELECT @@warning_count;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "do status" \
    "$do_expected" \
    "DO UNIX_TIMESTAMP(), UNIX_TIMESTAMP(NULL), UNIX_TIMESTAMP('1970-01-01 00:00:01'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'UNIX_TIMESTAMP'" \
    "SELECT UNIX_TIMESTAMP('2008-01-02', 'extra');" \
    "$DATABASE"

expect_upstream_accepts \
    "deferred compact numeric temporal values" \
    "SELECT UNIX_TIMESTAMP(20151113102019), UNIX_TIMESTAMP(151113102019);" \
    "$DATABASE"

expect_upstream_accepts \
    "deferred fractional temporal values" \
    "SELECT UNIX_TIMESTAMP('2015-11-13 10:20:19.012');" \
    "$DATABASE"
