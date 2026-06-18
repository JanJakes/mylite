#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_string_search_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_string_search_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --default-character-set=utf8mb4 "$@"
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

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; USE ${DATABASE}; SET NAMES utf8mb4;" >/dev/null

scalar_expected=$(cat <<\EXPECTED
4	4	4	0	7	1	2	4	0	0	0	NULL	NULL	NULL	NULL	NULL
-1	0
EXPECTED
)
expect_output \
    "scalar string search values" \
    "$scalar_expected" \
    "DO 0; SELECT LOCATE('bar','foobarbar'), INSTR('foobarbar','bar'), "\
"POSITION('bar' IN 'foobarbar'), LOCATE('xbar','foobar'), "\
"LOCATE('bar','foobarbar',5), LOCATE('', 'abc'), LOCATE('', 'abc', 2), "\
"LOCATE('', 'abc', 4), LOCATE('', 'abc', 5), LOCATE('a','abc',0), "\
"LOCATE('a','abc',-1), LOCATE(NULL,'abc'), LOCATE('a',NULL), "\
"LOCATE('a','abc',NULL), INSTR(NULL,'a'), POSITION('a' IN NULL); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "converted argument values" \
    "2	1	2	2	2	0" \
    "SELECT LOCATE(23, 12345), LOCATE(TRUE, 12345), INSTR(12345, 23), "\
"POSITION(23 IN 12345), LOCATE('2', 12345, TRUE), LOCATE('2', 12345, FALSE);" \
    "$DATABASE"

expect_output \
    "whitespace accepted for locate instr" \
    "1	1" \
    "SELECT LOCATE ('a','abc'), INSTR ('abc','a');" \
    "$DATABASE"

expect_output \
    "scalar integer function locate positions" \
    "2	4" \
    "SELECT LOCATE('b', 'abc', ABS(-2)), LOCATE('', 'abc', LENGTH('abcd'));" \
    "$DATABASE"

header_expected=$(cat <<\EXPECTED
loc	INSTR('foobarbar','bar')	POSITION('bar' IN 'foobarbar')
4	4	4
EXPECTED
)
expect_output_with_headers \
    "string search labels" \
    "$header_expected" \
    "SELECT LOCATE('bar','foobarbar') AS loc, INSTR('foobarbar','bar'), "\
"POSITION('bar' IN 'foobarbar');" \
    "$DATABASE"

table_expected=$(cat <<\EXPECTED
1	4	4	4	1	2	5
2	3	3	3	0	NULL	NULL
3	NULL	NULL	NULL	NULL	0	5
-1	0
EXPECTED
)
expect_output \
    "table-backed row scalar string search values" \
    "$table_expected" \
    "CREATE TABLE t (id INT, s VARCHAR(20), c CHAR(5), txt TEXT, n INT, d DATE); "\
"INSERT INTO t VALUES "\
"(1,'foobarbar','a  ','hello',12345,'2024-01-02'),"\
"(2,'zzbarzz','B','x',NULL,NULL),"\
"(3,NULL,NULL,NULL,-77,'2024-03-04'); "\
"SELECT id, LOCATE('bar', s), INSTR(s, 'bar'), POSITION('bar' IN s), "\
"LOCATE('a', c), LOCATE('2', n), LOCATE('-', d) FROM t ORDER BY id; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "table integer expression locate positions" \
    "1	4	2
2	0	NULL
3	NULL	0" \
    "SELECT id, LOCATE('bar', s, id + 3), LOCATE('2', n, ABS(id)) FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table string search predicate truth" \
    "1
2" \
    "SELECT id FROM t WHERE LOCATE('bar', s) ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table string search predicate comparison and order" \
    "2
1" \
    "SELECT id FROM t WHERE LOCATE('bar', s) > 0 ORDER BY INSTR(s, 'bar'), id;" \
    "$DATABASE"

expect_output \
    "table string search predicate null test" \
    "3" \
    "SELECT id FROM t WHERE INSTR(s, 'bar') IS NULL ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table string search predicate not null test" \
    "1
2" \
    "SELECT id FROM t WHERE INSTR(s, 'bar') IS NOT NULL ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table string search predicate between" \
    "1
2" \
    "SELECT id FROM t WHERE POSITION('bar' IN s) BETWEEN 3 AND 4 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table string search predicate not between" \
    "2" \
    "SELECT id FROM t WHERE POSITION('bar' IN s) NOT BETWEEN 4 AND 4 ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table string search order expression" \
    "1	4
2	3
3	NULL" \
    "SELECT id, POSITION('bar' IN s) FROM t ORDER BY POSITION('bar' IN s) DESC, id;" \
    "$DATABASE"

expect_error \
    "locate too few arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'LOCATE'" \
    "SELECT LOCATE('a');" \
    "$DATABASE"

expect_error \
    "locate too many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'LOCATE'" \
    "SELECT LOCATE('a','abc',1,2);" \
    "$DATABASE"

expect_error \
    "instr too few arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'INSTR'" \
    "SELECT INSTR('abc');" \
    "$DATABASE"

expect_error \
    "instr too many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'INSTR'" \
    "SELECT INSTR('abc','a','x');" \
    "$DATABASE"

expect_error \
    "position whitespace is syntax error" \
    1064 \
    "42000" \
    "You have an error in your SQL syntax" \
    "SELECT POSITION ('a' IN 'abc');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_string_search_functions_expectations: ok"
