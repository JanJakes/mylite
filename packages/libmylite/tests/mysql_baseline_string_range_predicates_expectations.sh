#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_string_range_predicates_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_string_range_predicates_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;" \
    >/dev/null

run_mysql \
    "CREATE TABLE strings (id INT, c CHAR(5), v VARCHAR(8), t TEXT, flag VARCHAR(8)); "\
"INSERT INTO strings VALUES "\
"(1, 'abc', 'abc', 'abc', 'old'), "\
"(2, 'ABC', 'ABC', 'ABC', 'old'), "\
"(3, 'abd', 'abd', 'abd', 'old'), "\
"(4, 'ab', 'ab', 'ab', 'old'), "\
"(5, 'abc  ', 'abc  ', 'abc  ', 'old'), "\
"(6, NULL, NULL, NULL, 'old'), "\
"(7, 'b', 'b', 'b', 'old'), "\
"(8, 'aa', 'aa', 'aa', 'old');" \
    "$DATABASE" >/dev/null

expect_output \
    "varchar greater-than uses default collation and trailing spaces" \
    "3,5,7" \
    "SELECT IFNULL(GROUP_CONCAT(id ORDER BY id), 'NULL') FROM strings WHERE v > 'abc';" \
    "$DATABASE"

expect_output \
    "varchar greater-equal folds ASCII case" \
    "1,2,3,5,7" \
    "SELECT IFNULL(GROUP_CONCAT(id ORDER BY id), 'NULL') FROM strings WHERE v >= 'abc';" \
    "$DATABASE"

expect_output \
    "varchar less-than uses default collation" \
    "4,8" \
    "SELECT IFNULL(GROUP_CONCAT(id ORDER BY id), 'NULL') FROM strings WHERE v < 'abc';" \
    "$DATABASE"

expect_output \
    "varchar less-equal folds ASCII case" \
    "1,2,4,8" \
    "SELECT IFNULL(GROUP_CONCAT(id ORDER BY id), 'NULL') FROM strings WHERE v <= 'abc';" \
    "$DATABASE"

expect_output \
    "varchar between excludes trailing-space value above upper bound" \
    "1,2,4" \
    "SELECT IFNULL(GROUP_CONCAT(id ORDER BY id), 'NULL') FROM strings "\
"WHERE v BETWEEN 'ab' AND 'abc';" \
    "$DATABASE"

expect_output \
    "varchar not between excludes nulls" \
    "3,5,7,8" \
    "SELECT IFNULL(GROUP_CONCAT(id ORDER BY id), 'NULL') FROM strings "\
"WHERE v NOT BETWEEN 'ab' AND 'abc';" \
    "$DATABASE"

expect_output \
    "varchar in folds ASCII case" \
    "1,2,3" \
    "SELECT IFNULL(GROUP_CONCAT(id ORDER BY id), 'NULL') FROM strings "\
"WHERE v IN ('abc', 'abd');" \
    "$DATABASE"

expect_output \
    "varchar not in excludes nulls" \
    "4,5,7,8" \
    "SELECT IFNULL(GROUP_CONCAT(id ORDER BY id), 'NULL') FROM strings "\
"WHERE v NOT IN ('abc', 'abd');" \
    "$DATABASE"

expect_output \
    "varchar in with null list value still matches equal values" \
    "1,2" \
    "SELECT IFNULL(GROUP_CONCAT(id ORDER BY id), 'NULL') FROM strings "\
"WHERE v IN ('abc', NULL);" \
    "$DATABASE"

expect_output \
    "varchar not in with null list value has no true nonmatching rows" \
    "NULL" \
    "SELECT IFNULL(GROUP_CONCAT(id ORDER BY id), 'NULL') FROM strings "\
"WHERE v NOT IN ('abc', NULL);" \
    "$DATABASE"

expect_output \
    "char between uses char comparison shape" \
    "1,2,4,5" \
    "SELECT IFNULL(GROUP_CONCAT(id ORDER BY id), 'NULL') FROM strings "\
"WHERE c BETWEEN 'ab' AND 'abc';" \
    "$DATABASE"

expect_output \
    "text greater-than mirrors varchar range behavior" \
    "3,5,7" \
    "SELECT IFNULL(GROUP_CONCAT(id ORDER BY id), 'NULL') FROM strings WHERE t > 'abc';" \
    "$DATABASE"

expect_output \
    "update range predicate affected rows" \
    "3	0	1:old,2:old,3:range,4:old,5:range,6:old,7:range,8:old" \
    "UPDATE strings SET flag = 'range' WHERE v > 'abc'; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', flag) ORDER BY id) FROM strings;" \
    "$DATABASE"

expect_output \
    "delete between predicate affected rows" \
    "3	0	3,5,6,7,8" \
    "DELETE FROM strings WHERE v BETWEEN 'ab' AND 'abc'; "\
"SELECT ROW_COUNT(), @@warning_count, IFNULL(GROUP_CONCAT(id ORDER BY id), 'NULL') FROM strings;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE dates (id INT, option_value VARCHAR(32)); "\
"INSERT INTO dates VALUES "\
"(1, '2016-01-14T23:59:59Z'), "\
"(2, '2016-01-15T00:00:00Z'), "\
"(3, '2016-01-15T12:00:00Z'), "\
"(4, '2016-01-16T00:00:00Z'), "\
"(5, NULL);" \
    "$DATABASE" >/dev/null

expect_output \
    "iso-like varchar between is string comparison" \
    "2,3" \
    "SELECT IFNULL(GROUP_CONCAT(id ORDER BY id), 'NULL') FROM dates "\
"WHERE option_value BETWEEN '2016-01-15T00:00:00Z' AND '2016-01-15T23:59:59Z';" \
    "$DATABASE"

expect_output \
    "iso-like varchar greater-than is string comparison" \
    "3,4" \
    "SELECT IFNULL(GROUP_CONCAT(id ORDER BY id), 'NULL') FROM dates "\
"WHERE option_value > '2016-01-15T00:00:00Z';" \
    "$DATABASE"

expect_output \
    "iso-like varchar order keeps null first in ascending order" \
    "5,1,2,3,4" \
    "SELECT IFNULL(GROUP_CONCAT(id ORDER BY option_value, id), 'NULL') FROM dates;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_string_range_predicates_expectations: ok"
