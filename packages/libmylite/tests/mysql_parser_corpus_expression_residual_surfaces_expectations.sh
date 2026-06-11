#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_expression_residuals_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_expression_residual_surfaces_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names "$@"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3

    output=$(run_mysql "$sql")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_success() {
    label=$1
    sql=$2

    if ! run_mysql "$sql" >/dev/null 2>&1; then
        fail "$label: expected success"
    fi
}

expect_error() {
    label=$1
    sql=$2

    if run_mysql "$sql" >/dev/null 2>&1; then
        fail "$label: expected error"
    fi
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; "\
"CREATE TABLE t0(c0 DECIMAL(10,2)); "\
"CREATE TABLE t1(a INT, b INT, c INT, f1 DATE, f2 DATE, f3 DATE); "\
"CREATE TABLE t(a TIME, b INT); "\
"CREATE TABLE r(c INT); "\
"CREATE TABLE t2(f2 DATE); "\
"CREATE TABLE t3(f3 DATE); "\
"CREATE TABLE log_rows(argument TEXT);" >/dev/null

expect_success "decimal left predicate" "USE ${DATABASE}; SELECT * FROM t0 WHERE 0.9 > t0.c0;"
expect_success "predicate result comparison" \
    "USE ${DATABASE}; SELECT a, b, c FROM t1 WHERE (a > b) <> c;"
expect_success "do count distinct expression" \
    "USE ${DATABASE}; DO COUNT(DISTINCT ROUND(CAST(SLEEP(0) AS DECIMAL), NULL));"
expect_success "do unary not expression" "USE ${DATABASE}; DO (!(SECOND(0xb16beeb7)));"
expect_output \
    "interval shift expression" \
    "2019-08-15 16:00:00" \
    "USE ${DATABASE}; SELECT \"1900-01-01 00:00:00\" + INTERVAL 1<<20 HOUR;"
expect_success "bare current time predicate" "USE ${DATABASE}; SELECT * FROM t WHERE a = CURRENT_TIME;"
expect_success "bare current date predicate" "USE ${DATABASE}; SELECT * FROM t WHERE a = CURRENT_DATE;"
expect_output "unary not arithmetic projection" "5" "SELECT !0 * 5 AS x FROM DUAL;"
expect_output \
    "charset introducer predicate" \
    "1" \
    "SELECT _latin1'B' BETWEEN _latin1'a' AND _latin1'c';"
expect_success \
    "order function desc" \
    "USE ${DATABASE}; SELECT * FROM t1 ORDER BY ADDTIME(a, '00:00:00') DESC;"
expect_success \
    "insert keyword function in group key" \
    "USE ${DATABASE}; SELECT 1 FROM t1 GROUP BY INSERT(a,'1','11','1');"
expect_success \
    "function group key with rollup" \
    "USE ${DATABASE}; SELECT 1 FROM r GROUP BY MAKE_SET(1,c) WITH ROLLUP;"
expect_success "nested not predicate" "USE ${DATABASE}; SELECT * FROM t1 WHERE NOT(NOT(a));"
expect_success \
    "identifier between bounds" \
    "USE ${DATABASE}; SELECT f1, f2, f3 FROM t1 WHERE f1 BETWEEN f2 AND f3;"
expect_success \
    "identifier in list" \
    "USE ${DATABASE}; SELECT * FROM t2,t3 WHERE f2 IN (f3,'2003-04-05');"
expect_success \
    "parenthesized like pattern" \
    "USE ${DATABASE}; SELECT argument FROM log_rows WHERE argument LIKE ('SET%');"
expect_success \
    "parenthesized interval value" \
    "USE ${DATABASE}; SELECT a - INTERVAL(b) MICROSECOND FROM t;"

expect_error "missing function argument remains syntax" "SELECT f(1,,2);"
expect_error \
    "malformed parenthesized interval remains syntax" \
    "USE ${DATABASE}; SELECT a - INTERVAL(,) MICROSECOND FROM t;"
expect_error "incomplete operator remains syntax" "SELECT !;"

cleanup

printf '%s\n' "mysql_parser_corpus_expression_residual_surfaces_expectations: ok"
