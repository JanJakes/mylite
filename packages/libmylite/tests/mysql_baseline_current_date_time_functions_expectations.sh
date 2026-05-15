#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_current_date_time_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_current_date_time_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "current date and time scalar synonyms use statement timestamp" \
    "functions	2023-11-14	2023-11-14	2023-11-14	22:13:20	22:13:20	22:13:20	2023-11-14 22:13:20	1700000000.000000	0" \
    "SET time_zone = '+00:00'; SET timestamp = 1700000000; "\
"SELECT 'functions', CURDATE(), CURRENT_DATE, CURRENT_DATE(), "\
"CURTIME(), CURRENT_TIME, CURRENT_TIME(), NOW(), @@timestamp, @@warning_count;" \
    "$DATABASE"

expect_output \
    "current date and time DO statement" \
    "0	0" \
    "SET time_zone = '+00:00'; SET timestamp = 1700000000; "\
"DO CURDATE(), CURRENT_DATE, CURTIME(), CURRENT_TIME; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "current date and time DML assignment" \
    "1	0
1	2023-11-14	22:14:20
2	2023-11-14	22:14:20
0	0" \
    "SET time_zone = '+00:00'; "\
"CREATE TABLE current_values (id INT PRIMARY KEY, d DATE, tm TIME); "\
"SET timestamp = 1700000000; "\
"INSERT INTO current_values VALUES (1, CURDATE(), CURTIME()); "\
"SET timestamp = 1700000060; "\
"INSERT INTO current_values VALUES (2, CURRENT_DATE, CURRENT_TIME); "\
"UPDATE current_values SET d = CURDATE(), tm = CURTIME() WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, tm FROM current_values ORDER BY id; "\
"UPDATE current_values SET tm = CURTIME() WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "current date and time wider mysql behavior deferred by mylite" \
    "22:13:20.000000	22:13:20.000000
2023-12-14" \
    "SET time_zone = '+00:00'; SET timestamp = 1700000000; "\
"SELECT CURTIME(6), CURRENT_TIME(6); "\
"SELECT CURRENT_DATE + INTERVAL 30 DAY;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
default_current_temporals	CREATE TABLE `default_current_temporals` (
  `d` date DEFAULT (curdate()),
  `tm` time DEFAULT (curtime())
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "mysql accepts current date and time expression defaults deferred by mylite" \
    "$show_create_expected" \
    "CREATE TABLE default_current_temporals ("\
"d DATE DEFAULT (CURDATE()), tm TIME DEFAULT (CURTIME())); "\
"SHOW CREATE TABLE default_current_temporals;" \
    "$DATABASE"

expect_error \
    "current date rejects arguments" \
    1064 \
    "42000" \
    "right syntax" \
    "SELECT CURDATE(1);" \
    "$DATABASE"
