#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_date_interval_core_units_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_date_interval_core_units: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_type_info() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --column-type-info -vvv "$@"
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

expect_contains() {
    label=$1
    text=$2
    expected=$3

    case "$text" in
        *"$expected"*) ;;
        *) fail "$label: expected [$expected] in [$text]" ;;
    esac
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT HUP INT TERM

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9, got [$version]" ;;
esac

expect_output \
    "datetime input core units" \
    "2008-01-02 13:29:18	2008-01-02 13:30:17	2008-01-02 14:29:17	2008-01-03 13:29:17	2008-01-09 13:29:17	2008-02-02 13:29:17	2008-04-02 13:29:17	2009-01-02 13:29:17" \
    "SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND),
            DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 MINUTE),
            DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 HOUR),
            DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 DAY),
            DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 WEEK),
            DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 MONTH),
            DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 QUARTER),
            DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 YEAR);"

expect_output \
    "date input output shape" \
    "2008-01-02 00:00:01	2008-01-02 00:01:00	2008-01-02 01:00:00	2008-01-03	2008-01-09	2008-02-02	2008-04-02	2009-01-02" \
    "SELECT DATE_ADD('2008-01-02', INTERVAL 1 SECOND),
            DATE_ADD('2008-01-02', INTERVAL 1 MINUTE),
            DATE_ADD('2008-01-02', INTERVAL 1 HOUR),
            DATE_ADD('2008-01-02', INTERVAL 1 DAY),
            DATE_ADD('2008-01-02', INTERVAL 1 WEEK),
            DATE_ADD('2008-01-02', INTERVAL 1 MONTH),
            DATE_ADD('2008-01-02', INTERVAL 1 QUARTER),
            DATE_ADD('2008-01-02', INTERVAL 1 YEAR);"

expect_output \
    "subtraction and aliases" \
    "2008-01-02 13:28:17	2008-02-02	2009-01-02" \
    "SELECT DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 MINUTE),
            ADDDATE('2008-01-02', INTERVAL 1 MONTH),
            SUBDATE('2008-01-02', INTERVAL -1 YEAR);"

expect_output \
    "calendar clamp behavior" \
    "2024-02-29	2023-02-28	2025-02-28	2024-02-29	2024-04-30" \
    "SELECT DATE_ADD('2024-01-31', INTERVAL 1 MONTH),
            DATE_ADD('2023-01-31', INTERVAL 1 MONTH),
            DATE_ADD('2024-02-29', INTERVAL 1 YEAR),
            DATE_ADD('2024-03-31', INTERVAL -1 MONTH),
            DATE_ADD('2024-01-31', INTERVAL 1 QUARTER);"

expect_output \
    "exact quoted interval integers" \
    "2010-01-02	2008-03-02	2007-12-31	2008-01-16" \
    "SELECT DATE_ADD('2008-01-02', INTERVAL '2' YEAR),
            DATE_ADD('2008-01-02', INTERVAL '+2' MONTH),
            DATE_ADD('2008-01-02', INTERVAL '-2' DAY),
            DATE_ADD('2008-01-02', INTERVAL '02' WEEK);
     SHOW WARNINGS;"

expect_output \
    "null propagation" \
    "NULL	NULL" \
    "SELECT DATE_ADD('2008-01-02', INTERVAL NULL MINUTE),
            DATE_ADD(NULL, INTERVAL 1 YEAR);"

expect_output \
    "mysql warning-producing prefix intervals remain known behavior" \
    "2008-01-04	2008-01-02	2008-01-02
Warning	1292	Truncated incorrect INTEGER value: '2x'
Warning	1292	Truncated incorrect INTEGER value: 'x'
Warning	1292	Truncated incorrect INTEGER value: ''" \
    "SELECT DATE_ADD('2008-01-02', INTERVAL '2x' DAY),
            DATE_ADD('2008-01-02', INTERVAL 'x' DAY),
            DATE_ADD('2008-01-02', INTERVAL '' DAY);
     SHOW WARNINGS;"

run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; CREATE DATABASE ${DATABASE}; USE ${DATABASE};
CREATE TABLE t (d DATE, dt DATETIME, ts TIMESTAMP NULL, s VARCHAR(20), txt TEXT);
INSERT INTO t VALUES ('2008-01-02', '2008-01-02 13:29:17',
                      '2008-01-02 13:29:17', '2008-01-02', '2008-01-02');" >/dev/null

expect_output \
    "row-backed values" \
    "2008-01-03	2008-01-02 01:00:00	2008-01-03 13:29:17	2008-02-02 13:29:17	2008-01-03	2008-01-03" \
    "SELECT DATE_ADD(d, INTERVAL 1 DAY),
            DATE_ADD(d, INTERVAL 1 HOUR),
            DATE_ADD(dt, INTERVAL 1 DAY),
            DATE_ADD(ts, INTERVAL 1 MONTH),
            DATE_ADD(s, INTERVAL 1 DAY),
            DATE_ADD(txt, INTERVAL 1 DAY)
       FROM ${DATABASE}.t;"

metadata=$(run_mysql_type_info \
    "SELECT DATE_ADD(d, INTERVAL 1 DAY) AS d_day,
            DATE_ADD(d, INTERVAL 1 HOUR) AS d_hour,
            DATE_ADD(dt, INTERVAL 1 DAY) AS dt_day,
            DATE_ADD(s, INTERVAL 1 DAY) AS s_day
       FROM ${DATABASE}.t;")
d_day_metadata=$(printf '%s\n' "$metadata" | sed -n '/Field   1:/,/Field   2:/p')
d_hour_metadata=$(printf '%s\n' "$metadata" | sed -n '/Field   2:/,/Field   3:/p')
dt_day_metadata=$(printf '%s\n' "$metadata" | sed -n '/Field   3:/,/Field   4:/p')
s_day_metadata=$(printf '%s\n' "$metadata" | sed -n '/Field   4:/,/+---/p')

expect_contains "DATE plus DAY metadata" "$d_day_metadata" "Type:       DATE"
expect_contains "DATE plus HOUR metadata" "$d_hour_metadata" "Type:       DATETIME"
expect_contains "DATETIME plus DAY metadata" "$dt_day_metadata" "Type:       DATETIME"
expect_contains "VARCHAR plus DAY metadata" "$s_day_metadata" "Type:       STRING"

