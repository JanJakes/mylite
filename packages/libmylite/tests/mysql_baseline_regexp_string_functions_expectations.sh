#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_regexp_string_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_regexp_string_functions_expectations: $1" >&2
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

expect_output \
    "scalar values" \
    "2	bc	aXaX	0	NULL	abc	1		abcX	XXbXcX	[][][][]" \
    "SELECT REGEXP_INSTR('abcabc', 'b'), REGEXP_SUBSTR('abcabc', 'b.'), "\
"REGEXP_REPLACE('abcabc', 'b.', 'X'), REGEXP_INSTR('abc', 'z'), "\
"REGEXP_SUBSTR('abc', 'z'), REGEXP_REPLACE('abc', 'z', 'X'), "\
"REGEXP_INSTR('AbC', 'a'), REGEXP_SUBSTR('abc', '$'), "\
"REGEXP_REPLACE('abc', '$', 'X'), REGEXP_REPLACE('abc', 'a*', 'X'), "\
"CONCAT('[', REGEXP_REPLACE('', 'a*', 'X'), ']', "\
"'[', REGEXP_REPLACE('', '.*', 'X'), ']', "\
"'[', REGEXP_REPLACE('', '^', 'X'), ']', "\
"'[', REGEXP_REPLACE('', '$', 'X'), ']');" \
    "$DATABASE"

expect_output \
    "null propagation" \
    "NULL	NULL	NULL	NULL	NULL	NULL	NULL" \
    "SELECT REGEXP_INSTR(NULL, 'a'), REGEXP_INSTR('a', NULL), "\
"REGEXP_SUBSTR(NULL, 'a'), REGEXP_SUBSTR('a', NULL), "\
"REGEXP_REPLACE(NULL, 'a', 'x'), REGEXP_REPLACE('a', NULL, 'x'), "\
"REGEXP_REPLACE('a', 'a', NULL);" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE strings (id INT, v VARCHAR(16)); "\
"INSERT INTO strings VALUES (1, 'abc'), (2, 'ABC'), (3, 'rss_a'), "\
"(4, 'rss_'), (5, '1+2'), (6, NULL);" \
    "$DATABASE" >/dev/null

expect_output \
    "table projection" \
    "1	2	bc	aX	0
2	2	BC	AX	1
3	0	NULL	rss_a	0
4	0	NULL	rss_	0
5	0	NULL	1+2	0
6	NULL	NULL	NULL	0" \
    "SELECT id, REGEXP_INSTR(v, 'b.'), REGEXP_SUBSTR(v, 'b.'), "\
"REGEXP_REPLACE(v, 'b.', 'X'), REGEXP_INSTR(id, '2') FROM strings ORDER BY id;" \
    "$DATABASE"

expect_output \
    "select row count and warning count" \
    "aXc
-1	0" \
    "SELECT REGEXP_REPLACE('abc', 'b', 'X'); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "do row count and warning count" \
    "0	0" \
    "DO REGEXP_INSTR('abc', 'b'), REGEXP_SUBSTR('abc', 'b'), "\
"REGEXP_REPLACE('abc', 'b', 'X'); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "empty pattern" \
    3685 \
    HY000 \
    "Illegal argument to a regular expression" \
    "SELECT REGEXP_INSTR('abc', '');" \
    "$DATABASE"

expect_error \
    "unclosed bracket" \
    3696 \
    HY000 \
    "unclosed bracket" \
    "SELECT REGEXP_SUBSTR('abc', '[');" \
    "$DATABASE"
