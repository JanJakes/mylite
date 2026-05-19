#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_utc_date_time_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_utc_date_time_functions_expectations: $1" >&2
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
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected upstream output [$expected], got [$output]"
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
    "UTC functions use statement UTC time under UTC session zone" \
    "2023-11-14	22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20	1700000000.000000	0	0" \
    "SET time_zone = '+00:00'; SET timestamp = 1700000000; "\
"SELECT UTC_DATE(), UTC_TIME(), UTC_TIMESTAMP(), NOW(), @@timestamp, @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "UTC functions ignore nonzero session zone while current functions observe it" \
    "2023-11-14	22:13:20	2023-11-14 22:13:20	2023-11-15 00:13:20	2023-11-15	00:13:20	0" \
    "SET time_zone = '+02:00'; SET timestamp = 1700000000; "\
"SELECT UTC_DATE(), UTC_TIME(), UTC_TIMESTAMP(), NOW(), CURRENT_DATE, CURRENT_TIME, "\
"@@warning_count;" \
    "$DATABASE"

expect_output \
    "bare keyword and whitespace forms" \
    "2023-11-14	22:13:20	2023-11-14 22:13:20
2023-11-14	22:13:20	2023-11-14 22:13:20" \
    "SET time_zone = '+02:00'; SET timestamp = 1700000000; "\
"SELECT UTC_DATE, UTC_TIME, UTC_TIMESTAMP; "\
"SELECT UTC_DATE (), UTC_TIME (), UTC_TIMESTAMP ();" \
    "$DATABASE"

expect_output \
    "UTC DO statement" \
    "0	0" \
    "SET time_zone = '+02:00'; SET timestamp = 1700000000; "\
"DO UTC_DATE(), UTC_TIME(), UTC_TIMESTAMP(); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

dml_expected=$(cat <<\EXPECTED
1	0
1	2023-11-14	22:14:20	2023-11-14 22:14:20	2023-11-14 22:14:20
2	2023-11-14	22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20
3	2023-11-14	22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20
4	2023-11-14	22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20
0	0
1	2023-11-14	22:14:20	2023-11-14 22:14:20	2023-11-14 22:14:20
2	2023-11-14	22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20
3	2023-11-14	22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20
4	2023-11-14	22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20
EXPECTED
)
expect_output \
    "UTC DML assignment" \
    "$dml_expected" \
    "SET time_zone = '+02:00'; "\
"CREATE TABLE utc_values (id INT, d DATE, tm TIME, dt DATETIME, ts TIMESTAMP NULL); "\
"SET timestamp = 1700000000; "\
"INSERT INTO utc_values VALUES (1, UTC_DATE(), UTC_TIME(), UTC_TIMESTAMP(), UTC_TIMESTAMP()); "\
"INSERT INTO utc_values SET id = 2, d = UTC_DATE, tm = UTC_TIME, dt = UTC_TIMESTAMP, "\
"ts = UTC_TIMESTAMP; "\
"REPLACE INTO utc_values VALUES (3, UTC_DATE(), UTC_TIME(), UTC_TIMESTAMP(), UTC_TIMESTAMP()); "\
"REPLACE INTO utc_values SET id = 4, d = UTC_DATE, tm = UTC_TIME, dt = UTC_TIMESTAMP, "\
"ts = UTC_TIMESTAMP; "\
"SET timestamp = 1700000060; "\
"UPDATE utc_values SET d = UTC_DATE(), tm = UTC_TIME(), dt = UTC_TIMESTAMP(), "\
"ts = UTC_TIMESTAMP() WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, tm, dt, ts FROM utc_values ORDER BY id; "\
"UPDATE utc_values SET d = UTC_DATE, tm = UTC_TIME, dt = UTC_TIMESTAMP, ts = UTC_TIMESTAMP "\
"WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, tm, dt, ts FROM utc_values ORDER BY id;" \
    "$DATABASE"

expect_upstream_accepts \
    "fractional UTC time and timestamp forms are deferred by MyLite" \
    "22:13:20.000000	2023-11-14 22:13:20.000000" \
    "SET time_zone = '+02:00'; SET timestamp = 1700000000; "\
"SELECT UTC_TIME(6), UTC_TIMESTAMP(6);" \
    "$DATABASE"

expect_error \
    "UTC_DATE rejects arguments" \
    1064 \
    "42000" \
    "right syntax" \
    "SELECT UTC_DATE(1);" \
    "$DATABASE"

expect_error \
    "UTC_TIME rejects too-large precision" \
    1426 \
    "42000" \
    "Too-big precision 7 specified for 'utc_time'" \
    "SELECT UTC_TIME(7);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_utc_date_time_functions_expectations: ok"
