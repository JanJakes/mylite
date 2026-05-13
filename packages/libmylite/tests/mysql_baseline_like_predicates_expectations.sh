#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_like_predicates_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_like_predicates_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

run_mysql \
    "CREATE TABLE strings (id INT, c CHAR(5), v VARCHAR(8), t TEXT); "\
"INSERT INTO strings VALUES "\
"(1, 'abc', 'abc', 'abc'), "\
"(2, 'ABC', 'ABC', 'ABC'), "\
"(3, 'abcd', 'abcd', 'abcd'), "\
"(4, 'ab_1', 'ab_1', 'ab_1'), "\
"(5, 'ab%1', 'ab%1', 'ab%1'), "\
"(6, 'abc  ', 'abc  ', 'abc  '), "\
"(7, NULL, NULL, NULL), "\
"(8, 'xy', 'xy', 'xy');" \
    "$DATABASE" >/dev/null

expect_output \
    "varchar percent wildcard folds ASCII case" \
    "1,2,3,4,5,6" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v LIKE 'ab%';" \
    "$DATABASE"

expect_output \
    "varchar underscore wildcard matches one character" \
    "1,2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v LIKE 'ab_';" \
    "$DATABASE"

expect_output \
    "varchar exact like keeps trailing spaces significant" \
    "1,2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v LIKE 'abc';" \
    "$DATABASE"

expect_output \
    "char like observes CHAR trimmed comparison shape" \
    "1,2,6" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE c LIKE 'abc';" \
    "$DATABASE"

expect_output \
    "text like folds ASCII case" \
    "1,2,3,4,5,6" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE t LIKE 'ab%';" \
    "$DATABASE"

expect_output \
    "default backslash escapes literal underscore" \
    "4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v LIKE 'ab\\_%';" \
    "$DATABASE"

expect_output \
    "default backslash escapes literal percent" \
    "5" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v LIKE 'ab\\%%';" \
    "$DATABASE"

expect_output \
    "not like excludes null rows" \
    "8" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v NOT LIKE 'ab%';" \
    "$DATABASE"

expect_output \
    "not wrapped like excludes null rows" \
    "8" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE NOT (v LIKE 'ab%');" \
    "$DATABASE"

expect_output \
    "aggregate like predicate source filter" \
    "6" \
    "SELECT COUNT(*) FROM strings WHERE v LIKE 'ab%';" \
    "$DATABASE"

expect_output \
    "update like predicate affected rows" \
    "2	0	1:abc,2:ABC,3:prefix,4:prefix,5:ab%1,6:abc  ,7:NULL,8:xy" \
    "UPDATE strings SET v = 'prefix' WHERE v LIKE 'abcd' OR v LIKE 'ab\\_1'; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(v, 'NULL')) ORDER BY id) FROM strings;" \
    "$DATABASE"

expect_output \
    "delete not like predicate affected rows" \
    "3	0	1:abc,2:ABC,5:ab%1,6:abc  ,7:NULL" \
    "DELETE FROM strings WHERE v NOT LIKE 'ab%'; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(v, 'NULL')) ORDER BY id) FROM strings;" \
    "$DATABASE"

expect_output \
    "no backslash escapes disables default pattern escaping" \
    "" \
    "SET sql_mode = 'NO_BACKSLASH_ESCAPES'; "\
"SELECT IFNULL(GROUP_CONCAT(id ORDER BY id), '') FROM strings WHERE v LIKE 'ab\\_%';" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_like_predicates_expectations: ok"
