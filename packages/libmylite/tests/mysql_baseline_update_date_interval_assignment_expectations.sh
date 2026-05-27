#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_update_date_interval_assignment_$$"

fail() {
    printf '%s\n' "mysql_baseline_update_date_interval_assignment_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; SET time_zone = '+00:00';" >/dev/null

expect_output \
    "successful same-column date interval updates" \
    "$(cat <<EXPECTED
1	0	2014-01-15 00:00:00
1	0	2024-02-29
1	0	2014-01-15 00:00:00
1	0	2016-01-16
0	0	1
1	0	1
0	0	2017-02-28 00:00:00
1	0	1:2014-01-15 00:00:00,2:2017-03-01 00:00:00,3:2018-03-01 00:00:00
0	0
EXPECTED
)" \
    "USE ${DATABASE}; "\
"CREATE TABLE events(id INT PRIMARY KEY, dt DATETIME NOT NULL, d DATE NULL, txt LONGTEXT NOT NULL, v VARCHAR(32) NOT NULL, nullable DATETIME NULL); "\
"INSERT INTO events VALUES "\
"(1,'2016-01-15 00:00:00','2024-01-31','2016-01-15 00:00:00','2016-01-15','2016-01-15 00:00:00'), "\
"(2,'2017-02-28 00:00:00','2023-01-31','2017-02-28','2017-02-28',NULL), "\
"(3,'2018-03-01 00:00:00',NULL,'2018-03-01','2018-03-01',NULL); "\
"UPDATE events SET dt = DATE_SUB(dt, INTERVAL '2' YEAR) WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, dt FROM events WHERE id = 1; "\
"UPDATE events SET d = DATE_ADD(d, INTERVAL +1 MONTH) WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, d FROM events WHERE id = 1; "\
"UPDATE events SET txt = SUBDATE(txt, INTERVAL '+2' YEAR) WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, txt FROM events WHERE id = 1; "\
"UPDATE events SET v = ADDDATE(v, INTERVAL 1 DAY) WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, v FROM events WHERE id = 1; "\
"UPDATE events SET nullable = DATE_ADD(nullable, INTERVAL 1 DAY) WHERE id = 2; "\
"SELECT ROW_COUNT(), @@warning_count, nullable IS NULL FROM events WHERE id = 2; "\
"UPDATE events SET nullable = DATE_ADD(nullable, INTERVAL NULL DAY) WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, nullable IS NULL FROM events WHERE id = 1; "\
"UPDATE events SET dt = DATE_ADD(dt, INTERVAL 0 DAY) WHERE id = 2; "\
"SELECT ROW_COUNT(), @@warning_count, dt FROM events WHERE id = 2; "\
"UPDATE events SET dt = DATE_ADD(dt, INTERVAL 1 DAY) WHERE id IN (1, 2) ORDER BY id DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', dt) ORDER BY id) FROM events; "\
"UPDATE events SET dt = DATE_ADD(dt, INTERVAL 5 DAY) ORDER BY id LIMIT 0; "\
"SELECT ROW_COUNT(), @@warning_count;"

expect_output \
    "no match skips invalid source conversion" \
    "$(cat <<EXPECTED
0	0	bad
0	0	bad
EXPECTED
)" \
    "USE ${DATABASE}; "\
"CREATE TABLE invalids(id INT PRIMARY KEY, v VARCHAR(32) NOT NULL); "\
"INSERT INTO invalids VALUES (1, 'bad'); "\
"UPDATE invalids SET v = DATE_ADD(v, INTERVAL 1 DAY) WHERE id = 2; "\
"SELECT ROW_COUNT(), @@warning_count, v FROM invalids; "\
"UPDATE invalids SET v = DATE_ADD(v, INTERVAL 1 DAY) ORDER BY id LIMIT 0; "\
"SELECT ROW_COUNT(), @@warning_count, v FROM invalids;"

expect_output \
    "no match and limit zero skip null interval into not null" \
    "$(cat <<EXPECTED
0	0
0	0
EXPECTED
)" \
    "USE ${DATABASE}; "\
"CREATE TABLE required(id INT PRIMARY KEY, dt DATETIME NOT NULL); "\
"INSERT INTO required VALUES (1, '2016-01-15 00:00:00'); "\
"UPDATE required SET dt = DATE_ADD(dt, INTERVAL NULL DAY) WHERE id = 2; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"UPDATE required SET dt = DATE_ADD(dt, INTERVAL NULL DAY) ORDER BY id LIMIT 0; "\
"SELECT ROW_COUNT(), @@warning_count;"

expect_error \
    "null interval into not null target" \
    1048 \
    "23000" \
    "Column 'dt' cannot be null" \
    "USE ${DATABASE}; "\
"UPDATE required SET dt = DATE_ADD(dt, INTERVAL NULL DAY);"

expect_error \
    "matched invalid source value" \
    1292 \
    "22007" \
    "Incorrect datetime value: 'bad'" \
    "USE ${DATABASE}; "\
"UPDATE invalids SET v = DATE_ADD(v, INTERVAL 1 DAY);"

expect_error \
    "matched datetime overflow" \
    1441 \
    "22008" \
    "Datetime function: datetime field overflow" \
    "USE ${DATABASE}; "\
"CREATE TABLE overflows(id INT PRIMARY KEY, dt DATETIME NULL); "\
"INSERT INTO overflows VALUES (1, '9999-12-31 23:59:59'); "\
"UPDATE overflows SET dt = DATE_ADD(dt, INTERVAL 1 SECOND);"

printf '%s\n' "mysql_baseline_update_date_interval_assignment_expectations: ok"
