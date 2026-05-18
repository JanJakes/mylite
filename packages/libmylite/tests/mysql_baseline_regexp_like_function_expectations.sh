#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_regexp_like_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_regexp_like_function_expectations: $1" >&2
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
    "scalar match and null behavior" \
    "1	0	1	1	0	NULL	NULL	NULL" \
    "SELECT REGEXP_LIKE('abc', 'ABC'), REGEXP_LIKE('abc', 'ABC', 'c'), "\
"REGEXP_LIKE('abc', 'ABC', 'i'), REGEXP_LIKE('abc', 'ABC', 'ci'), "\
"REGEXP_LIKE('abc', 'ABC', 'ic'), REGEXP_LIKE(NULL, 'a'), "\
"REGEXP_LIKE('a', NULL), REGEXP_LIKE('a', 'a', NULL);" \
    "$DATABASE"

expect_output \
    "numeric and boolean scalar coercion" \
    "1	1	1	1" \
    "SELECT REGEXP_LIKE(123, '23'), REGEXP_LIKE('123', 23), "\
"REGEXP_LIKE(TRUE, '^1$'), REGEXP_LIKE(FALSE, '^0$');" \
    "$DATABASE"

expect_output \
    "deferred upstream multiline match type" \
    "1	1	1" \
    "SELECT REGEXP_LIKE('a\nb', '^b', 'm'), REGEXP_LIKE('a\nb', 'a.b', 'n'), "\
"REGEXP_LIKE('a', 'a', 'u');" \
    "$DATABASE"

expect_output \
    "default line terminator behavior" \
    "0	0" \
    "SELECT REGEXP_LIKE('a\nb', 'a.b'), REGEXP_LIKE('a\nb', '^b');" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE strings (id INT, v VARCHAR(16), note VARCHAR(16)); "\
"INSERT INTO strings VALUES "\
"(1, 'abc', 'keep'), (2, 'ABC', 'keep'), (3, 'rss_a', 'keep'), "\
"(4, 'rss_', 'keep'), (5, NULL, 'keep'), (6, '1+2', 'keep'), "\
"(7, '12', 'keep');" \
    "$DATABASE" >/dev/null

expect_output \
    "table predicate folds ASCII case" \
    "1,2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE REGEXP_LIKE(v, '^ab');" \
    "$DATABASE"

expect_output \
    "not regexp_like excludes null rows" \
    "3,4,6,7" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE NOT REGEXP_LIKE(v, '^ab');" \
    "$DATABASE"

expect_output \
    "case-sensitive match type in predicate" \
    "2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE REGEXP_LIKE(v, '^AB', 'c');" \
    "$DATABASE"

expect_output \
    "regex result projection over nullable rows" \
    "1	0
2	0
3	1
4	0
5	NULL
6	0
7	0" \
    "SELECT id, REGEXP_LIKE(v, '^rss_.+$') FROM strings ORDER BY id;" \
    "$DATABASE"

expect_output \
    "update regexp_like predicate affected rows" \
    "1	0	1:keep,2:keep,3:hit,4:keep,5:keep,6:keep,7:keep" \
    "UPDATE strings SET note = 'hit' WHERE REGEXP_LIKE(v, '^rss_.+$'); "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', note) ORDER BY id) "\
"FROM strings;" \
    "$DATABASE"

expect_output \
    "delete regexp_like predicate affected rows" \
    "1	0	1:keep,2:keep,3:hit,4:keep,5:keep,7:keep" \
    "DELETE FROM strings WHERE REGEXP_LIKE(v, '1\\\\+2'); "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', note) ORDER BY id) "\
"FROM strings;" \
    "$DATABASE"

expect_output \
    "select row count and warning count" \
    "1
-1	0" \
    "SELECT REGEXP_LIKE('abc', 'abc'); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "do row count and warning count" \
    "0	0" \
    "DO REGEXP_LIKE('abc', 'abc'); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'REGEXP_LIKE'" \
    "SELECT REGEXP_LIKE();" \
    "$DATABASE"

expect_error \
    "one argument" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'REGEXP_LIKE'" \
    "SELECT REGEXP_LIKE('a');" \
    "$DATABASE"

expect_error \
    "four arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'REGEXP_LIKE'" \
    "SELECT REGEXP_LIKE('a', 'a', 'i', 'extra');" \
    "$DATABASE"

expect_error \
    "unsupported match type" \
    1210 \
    HY000 \
    "Incorrect arguments to regexp_like" \
    "SELECT REGEXP_LIKE('a', 'a', 'z');" \
    "$DATABASE"

expect_error \
    "uppercase match type" \
    1210 \
    HY000 \
    "Incorrect arguments to regexp_like" \
    "SELECT REGEXP_LIKE('a', 'a', 'I');" \
    "$DATABASE"

expect_error \
    "invalid match type with null value" \
    1210 \
    HY000 \
    "Incorrect arguments to regexp_like" \
    "SELECT REGEXP_LIKE(NULL, 'a', 'z');" \
    "$DATABASE"

expect_error \
    "binary value rejected" \
    3995 \
    HY000 \
    "Character set 'binary' cannot be used" \
    "SELECT REGEXP_LIKE(CAST('a' AS BINARY), 'A', 'i');" \
    "$DATABASE"

expect_error \
    "invalid bracket expression" \
    3696 \
    HY000 \
    "unclosed bracket expression" \
    "SELECT REGEXP_LIKE('a', '[');" \
    "$DATABASE"

expect_error \
    "invalid bracket expression with null value" \
    3696 \
    HY000 \
    "unclosed bracket expression" \
    "SELECT REGEXP_LIKE(NULL, '[');" \
    "$DATABASE"

expect_error \
    "invalid character range" \
    3697 \
    HY000 \
    "character range where x comes after y" \
    "SELECT REGEXP_LIKE('a', '[z-a]');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_regexp_like_function_expectations: ok"
