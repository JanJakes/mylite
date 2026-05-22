#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_time_format_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_time_format_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "${MYSQL_BIN:-mysql}" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
        return
    fi
    if [ -n "$MYSQL_BIN" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot --default-character-set=utf8mb4 \
                --batch --raw --skip-column-names "$@"
        return
    fi
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "${MYSQL_BIN:-mysql}" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --default-character-set=utf8mb4 --batch --raw "$@"
        return
    fi
    if [ -n "$MYSQL_BIN" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot --default-character-set=utf8mb4 \
                --batch --raw "$@"
        return
    fi
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
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

core_expected=$(cat <<EXPECTED
100|100|04|04|4|00|00|00|100:00:00|04:00:00 AM|AM|000000|%|q|%	-01:02:03	13:29:17	NULL	NULL	NULL	0
-1	0
EXPECTED
)
expect_output \
    "core TIME_FORMAT values" \
    "$core_expected" \
    "USE ${DATABASE}; DO 0; "\
"SELECT TIME_FORMAT('100:00:00','%H|%k|%h|%I|%l|%i|%S|%s|%T|%r|%p|%f|%%|%q|%'), "\
"TIME_FORMAT('-01:02:03','%H:%i:%s'), "\
"TIME_FORMAT('2008-01-02 13:29:17','%H:%i:%s'), "\
"TIME_FORMAT(NULL,'%H'), TIME_FORMAT('01:02:03',NULL), "\
"TIME_FORMAT('01:02:03',''), @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;"

negative_expected=$(cat <<EXPECTED
-x02x03x	-02:03	-%	-q	-abc
EXPECTED
)
expect_output \
    "TIME_FORMAT negative sign placement" \
    "$negative_expected" \
    "USE ${DATABASE}; "\
"SELECT TIME_FORMAT('-01:02:03','x%ix%sx'), TIME_FORMAT('-01:02:03','%i:%s'), "\
"TIME_FORMAT('-01:02:03','%%'), TIME_FORMAT('-01:02:03','%q'), "\
"TIME_FORMAT('-01:02:03','abc');"

am_pm_expected=$(cat <<EXPECTED
00|12|AM	11|11|AM	12|12|PM	23|11|PM	24|12|AM	25|01|AM
EXPECTED
)
expect_output \
    "TIME_FORMAT AM PM and long-hour clock behavior" \
    "$am_pm_expected" \
    "USE ${DATABASE}; "\
"SELECT TIME_FORMAT('00:00:00','%H|%h|%p'), "\
"TIME_FORMAT('11:59:59','%H|%h|%p'), TIME_FORMAT('12:00:00','%H|%h|%p'), "\
"TIME_FORMAT('23:59:59','%H|%h|%p'), TIME_FORMAT('24:00:00','%H|%h|%p'), "\
"TIME_FORMAT('25:00:00','%H|%h|%p');"

label_expected=$(cat <<EXPECTED
TIME_FORMAT ('01:02:03','%H')	quoted
01	01:02
EXPECTED
)
expect_output_with_headers \
    "TIME_FORMAT labels and whitespace" \
    "$label_expected" \
    "USE ${DATABASE}; SET SESSION sql_mode = ''; "\
"SELECT TIME_FORMAT ('01:02:03','%H'), TIME_FORMAT(\"01:02:03\", \"%H:%i\") AS quoted "\
"FROM DUAL;"

identifier_expected=$(cat <<EXPECTED
time_format
time_format
EXPECTED
)
expect_output \
    "TIME_FORMAT identifier remains nonreserved" \
    "$identifier_expected" \
    "USE ${DATABASE}; SET SESSION sql_mode = ''; "\
"DROP TABLE IF EXISTS time_format; CREATE TABLE time_format(id INT); "\
"SHOW TABLES LIKE 'time_format'; DROP TABLE time_format; "\
"SET SESSION sql_mode = 'IGNORE_SPACE'; CREATE TABLE time_format(id INT); "\
"SHOW TABLES LIKE 'time_format'; DROP TABLE time_format;"

table_expected=$(cat <<EXPECTED
1	01:02:03	13.29	13:29:17	00:00:00	04:05:06
2	100:00:00	00.42	NULL	NULL	NULL
3	NULL	NULL	NULL	00:00:00	NULL
Warning	1292	Truncated incorrect time value: 'bad'
1
EXPECTED
)
expect_output \
    "TIME_FORMAT table-backed projection and warning" \
    "$table_expected" \
    "USE ${DATABASE}; "\
"CREATE TABLE t(id INT, tm TIME, dt DATETIME, ts TIMESTAMP NULL, d DATE, v VARCHAR(32)); "\
"INSERT INTO t VALUES "\
"(1,'01:02:03','2008-01-02 13:29:17','2008-01-02 13:29:17','2008-01-02','04:05:06'), "\
"(2,'100:00:00','2008-01-02 00:42:00',NULL,NULL,'bad'), "\
"(3,NULL,NULL,NULL,'2008-01-03',NULL); "\
"SELECT id, TIME_FORMAT(tm,'%H:%i:%s'), TIME_FORMAT(dt,'%H.%i'), TIME_FORMAT(ts,'%T'), "\
"TIME_FORMAT(d,'%H:%i:%s'), TIME_FORMAT(v,'%H:%i:%s') FROM t ORDER BY id; "\
"SHOW WARNINGS; SELECT @@warning_count;"

date_token_expected=$(cat <<EXPECTED
0000	00	NULL	NULL	0
EXPECTED
)
expect_output \
    "TIME_FORMAT date and week tokens accepted upstream and deferred by MyLite" \
    "$date_token_expected" \
    "USE ${DATABASE}; "\
"SELECT TIME_FORMAT('01:02:03','%Y'), TIME_FORMAT('01:02:03','%m'), "\
"TIME_FORMAT('01:02:03','%a'), TIME_FORMAT('01:02:03','%U'), @@warning_count;"

date_only_expected=$(cat <<EXPECTED
00:20:03
Warning	1292	Truncated incorrect time value: '2003-12-31'
EXPECTED
)
expect_output \
    "TIME_FORMAT date-only string accepted upstream and deferred by MyLite" \
    "$date_only_expected" \
    "USE ${DATABASE}; SELECT TIME_FORMAT('2003-12-31','%H:%i:%s'); SHOW WARNINGS;"

expect_output \
    "TIME_FORMAT invalid datetime warning" \
    "$(cat <<EXPECTED
NULL
Warning	1292	Truncated incorrect time value: '2003-12-31 24:00:00'
EXPECTED
)" \
    "USE ${DATABASE}; SELECT TIME_FORMAT('2003-12-31 24:00:00','%H:%i:%s'); SHOW WARNINGS;"

expect_error \
    "TIME_FORMAT no args parameter count" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "USE ${DATABASE}; SELECT TIME_FORMAT();"

expect_error \
    "TIME_FORMAT one arg parameter count" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "USE ${DATABASE}; SELECT TIME_FORMAT('01:02:03');"

expect_error \
    "TIME_FORMAT three args parameter count" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "USE ${DATABASE}; SELECT TIME_FORMAT('01:02:03','%H','extra');"

expect_upstream_accepts \
    "TIME_FORMAT numeric input deferred by MyLite" \
    "USE ${DATABASE}; SELECT TIME_FORMAT(123456, '%H:%i:%s');"

printf '%s\n' "mysql_baseline_time_format_function_expectations: ok"
