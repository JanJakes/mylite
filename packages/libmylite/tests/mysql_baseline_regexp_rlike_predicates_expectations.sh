#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_regexp_rlike_predicates_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_regexp_rlike_predicates_expectations: $1" >&2
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
    status=$?
    set -e

    if [ "$status" -eq 0 ]; then
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

run_mysql \
    "CREATE TABLE strings (id INT, c CHAR(8), v VARCHAR(16), t TEXT); "\
"INSERT INTO strings VALUES "\
"(1, 'abc', 'abc', 'abc'), "\
"(2, 'ABC', 'ABC', 'ABC'), "\
"(3, 'abcd', 'abcd', 'abcd'), "\
"(4, 'rss_a', 'rss_a', 'rss_a'), "\
"(5, 'rss_', 'rss_', 'rss_'), "\
"(6, 'rss_12', 'rss_12', 'rss_12'), "\
"(7, NULL, NULL, NULL), "\
"(8, 'xy', 'xy', 'xy'), "\
"(9, 'abc  ', 'abc  ', 'abc  ');" \
    "$DATABASE" >/dev/null

expect_output \
    "regexp prefix folds ASCII case" \
    "1,2,3,9" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v REGEXP '^ab';" \
    "$DATABASE"

expect_output \
    "rlike synonym folds ASCII case" \
    "1,2,3,9" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v RLIKE '^AB';" \
    "$DATABASE"

expect_output \
    "wordpress rss regexp shape" \
    "4,6" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v REGEXP '^rss_.+$';" \
    "$DATABASE"

expect_output \
    "char regexp observes char trimmed shape" \
    "1,2,9" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE c REGEXP '^abc$';" \
    "$DATABASE"

expect_output \
    "bracket class range" \
    "1,2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v REGEXP '^[a-d]+$';" \
    "$DATABASE"

expect_output \
    "negated bracket class" \
    "4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v REGEXP '^rss_[^0-9]+$';" \
    "$DATABASE"

expect_output \
    "not regexp excludes null rows" \
    "4,5,6,8" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v NOT REGEXP '^ab';" \
    "$DATABASE"

expect_output \
    "not wrapped rlike excludes null rows" \
    "4,5,6,8" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE NOT (v RLIKE '^ab');" \
    "$DATABASE"

expect_output \
    "regexp null operands" \
    "NULL	NULL" \
    "SELECT NULL REGEXP '^a$', 'abc' REGEXP NULL;" \
    "$DATABASE"

expect_output \
    "update regexp predicate affected rows" \
    "2	0	1:abc,2:ABC,3:abcd,4:hit,5:rss_,6:hit,7:NULL,8:xy,9:abc  " \
    "UPDATE strings SET v = 'hit' WHERE v REGEXP '^rss_.+$'; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(v, 'NULL')) ORDER BY id) FROM strings;" \
    "$DATABASE"

expect_output \
    "delete rlike predicate affected rows" \
    "2	0	1:abc,2:ABC,3:abcd,5:rss_,7:NULL,8:xy,9:abc  " \
    "DELETE FROM strings WHERE v RLIKE '^hit$'; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(v, 'NULL')) ORDER BY id) FROM strings;" \
    "$DATABASE"

run_mysql \
    "DROP TABLE strings; "\
"CREATE TABLE strings (id INT, v VARCHAR(16)); "\
"INSERT INTO strings VALUES (1, '1+2'), (2, '12'), (3, '1++2');" \
    "$DATABASE" >/dev/null

expect_output \
    "regex plus is quantifier" \
    "2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v REGEXP '1+2';" \
    "$DATABASE"

expect_output \
    "regex escaped plus requires SQL-escaped backslash" \
    "1" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v REGEXP '1\\\\+2';" \
    "$DATABASE"

run_mysql \
    "INSERT INTO strings VALUES (4, 'ac'), (5, 'abc'), (6, 'abbc');" \
    "$DATABASE" >/dev/null

expect_output \
    "regex star allows zero or more matches" \
    "4,5,6" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v REGEXP '^ab*c$';" \
    "$DATABASE"

expect_output \
    "regex question mark allows zero or one match" \
    "4,5" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v REGEXP '^ab?c$';" \
    "$DATABASE"

expect_output \
    "regex quantifiers backtrack" \
    "5,6" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v REGEXP '^ab*bc$';" \
    "$DATABASE"

expect_error \
    "invalid bracket expression" \
    3696 \
    HY000 \
    "unclosed bracket expression" \
    "SELECT v REGEXP '[' FROM strings LIMIT 1;" \
    "$DATABASE"

expect_error \
    "invalid character range" \
    3697 \
    HY000 \
    "character range where x comes after y" \
    "SELECT v REGEXP '[z-a]' FROM strings LIMIT 1;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_regexp_rlike_predicates_expectations: ok"
