#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_sysdate_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_sysdate_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    fi
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

trap cleanup EXIT HUP INT TERM

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

expect_output \
    "SYSDATE ignores SET timestamp" \
    "1	1	1	0" \
    "SET time_zone = '+00:00'; SET timestamp = 1700000000; "\
"SELECT NOW() = '2023-11-14 22:13:20', SYSDATE() <> NOW(), "\
"SYSDATE() REGEXP '^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}$', "\
"@@warning_count;" \
    "$DATABASE"

expect_output \
    "SYSDATE observes session time zone" \
    "1	0" \
    "SET time_zone = '+02:00'; "\
"SELECT TIMESTAMPDIFF(MINUTE, UTC_TIMESTAMP(), SYSDATE()) BETWEEN 119 AND 121, "\
"@@warning_count;" \
    "$DATABASE"

expect_output \
    "SYSDATE from DUAL and DO" \
    "1
0	0" \
    "SELECT SYSDATE() REGEXP '^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}$' "\
"FROM DUAL; DO SYSDATE(); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "SYSDATE IGNORE_SPACE whitespace call" \
    "1" \
    "SET SESSION sql_mode = 'IGNORE_SPACE'; "\
"SELECT SYSDATE () REGEXP '^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}$';" \
    "$DATABASE"

expect_output \
    "SYSDATE DML accepted for temporal targets" \
    "1	0
1	1	1
1	0
1	1	0" \
    "CREATE TABLE t(id INT, dt DATETIME, ts TIMESTAMP NULL); "\
"SET time_zone = '+00:00'; SET timestamp = 1700000000; "\
"INSERT INTO t VALUES (1, SYSDATE(), SYSDATE()); SELECT ROW_COUNT(), @@warning_count; "\
"SELECT dt REGEXP '^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}$', "\
"ts IS NOT NULL, dt <> '2023-11-14 22:13:20' FROM t; "\
"UPDATE t SET dt = '2001-01-01 00:00:00', ts = '2001-01-01 00:00:00' WHERE id = 1; "\
"UPDATE t SET dt = SYSDATE(), ts = SYSDATE() WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT COUNT(*), MIN(ts IS NOT NULL), @@warning_count "\
"FROM t;" \
    "$DATABASE"

expect_output \
    "SYSDATE fsp forms accepted by upstream" \
    "1	1	1" \
    "SELECT CHAR_LENGTH(SYSDATE(0)) = 19, CHAR_LENGTH(SYSDATE(6)) = 26, @@warning_count;" \
    "$DATABASE"

expect_upstream_accepts \
    "SYSDATE broader assignment coercions deferred by MyLite" \
    "CREATE TABLE wider(d DATE, tm TIME, v VARCHAR(32)); "\
"INSERT INTO wider VALUES (SYSDATE(), SYSDATE(), SYSDATE());" \
    "$DATABASE"

expect_error \
    "SYSDATE whitespace call without IGNORE_SPACE is a stored function lookup" \
    1630 \
    "42000" \
    "FUNCTION" \
    "SET SESSION sql_mode = ''; SELECT SYSDATE ();" \
    "$DATABASE"

expect_error \
    "SYSDATE whitespace argument call without IGNORE_SPACE is a stored function lookup" \
    1630 \
    "42000" \
    "FUNCTION" \
    "SET SESSION sql_mode = ''; SELECT SYSDATE (1);" \
    "$DATABASE"

expect_error \
    "bare SYSDATE is an identifier" \
    1054 \
    "42S22" \
    "Unknown column 'SYSDATE'" \
    "SELECT SYSDATE;" \
    "$DATABASE"

expect_error \
    "SYSDATE fsp too large" \
    1426 \
    "42000" \
    "Too-big precision 7 specified for 'sysdate'" \
    "SELECT SYSDATE(7);" \
    "$DATABASE"

expect_error \
    "SYSDATE rejects multiple arguments" \
    1064 \
    "42000" \
    "syntax" \
    "SELECT SYSDATE(1, 2);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_sysdate_function_expectations: ok"
