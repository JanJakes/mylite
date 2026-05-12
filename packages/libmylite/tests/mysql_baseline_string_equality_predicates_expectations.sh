#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_string_equality_predicates_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_string_equality_predicates_expectations: $1" >&2
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
    "CREATE TABLE strings (id INT, c CHAR(5), v VARCHAR(5), t TEXT); "\
"INSERT INTO strings VALUES "\
"(1, 'abc', 'abc', 'abc'), "\
"(2, 'ABC', 'ABC', 'ABC'), "\
"(3, 'abc  ', 'abc  ', 'abc  '), "\
"(4, NULL, NULL, NULL), "\
"(5, 'ab', 'ab', 'ab');" \
    "$DATABASE" >/dev/null

expect_output \
    "varchar equality folds ASCII case" \
    "1,2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v = 'abc';" \
    "$DATABASE"

expect_output \
    "char equality uses canonical char storage" \
    "1,2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE c = 'abc';" \
    "$DATABASE"

expect_output \
    "text equality folds ASCII case" \
    "1,2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE t = 'abc';" \
    "$DATABASE"

expect_output \
    "varchar inequality excludes nulls" \
    "3,5" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v <> 'abc';" \
    "$DATABASE"

expect_output \
    "varchar bang inequality excludes nulls" \
    "3,5" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v != 'abc';" \
    "$DATABASE"

expect_output \
    "varchar null-safe equality with non-null literal" \
    "1,2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v <=> 'abc';" \
    "$DATABASE"

expect_output \
    "not null-safe equality includes nulls" \
    "3,4,5" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE NOT (v <=> 'abc');" \
    "$DATABASE"

expect_output \
    "aggregate string predicate source filter" \
    "2" \
    "SELECT COUNT(*) FROM strings WHERE v = 'abc';" \
    "$DATABASE"

expect_output \
    "update string predicate affected rows" \
    "2	0	1:hit,2:hit,3:abc  ,4:NULL,5:ab" \
    "UPDATE strings SET v = 'hit' WHERE v = 'abc'; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(v, 'NULL')) ORDER BY id) FROM strings;" \
    "$DATABASE"

expect_output \
    "delete string predicate affected rows" \
    "2	0	3:abc  ,4:NULL,5:ab" \
    "DELETE FROM strings WHERE t = 'ABC'; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(t, 'NULL')) ORDER BY id) FROM strings;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_string_equality_predicates_expectations: ok"
