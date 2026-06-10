#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_query_function_subquery_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_query_function_subquery_surfaces_expectations: $1" >&2
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; "\
"CREATE TABLE t1 (a INT, b INT, c VARCHAR(20)); "\
"CREATE TABLE t2 (a INT, b INT); "\
"INSERT INTO t1 VALUES (1,2,'x'),(3,4,'yy'); "\
"INSERT INTO t2 VALUES (1,20),(3,40);" >/dev/null

expect_output \
    "nested function projection and ordering" \
    "78	78	58
7979	7979	5959" \
    "USE ${DATABASE}; "\
"SELECT HEX(c), HEX(LOWER(c)), HEX(UPPER(c)) FROM t1 ORDER BY BINARY(c);"

expect_output \
    "scalar subquery projection" \
    "1	20
3	40" \
    "USE ${DATABASE}; "\
"SELECT a, (SELECT MAX(b) FROM t2 WHERE t2.a = t1.a) AS max_b "\
"FROM t1 ORDER BY a;"

expect_output \
    "named aggregate window expression" \
    "1
3" \
    "USE ${DATABASE}; "\
"SELECT SUM(a) OVER w FROM t1 WINDOW w AS (ORDER BY b ROWS CURRENT ROW) "\
"ORDER BY SUM(b) OVER w;"

expect_output \
    "values row subquery" \
    "1	10" \
    "USE ${DATABASE}; VALUES ROW((SELECT 1), 10);"

expect_output \
    "insert subquery value" \
    "10	40	subq" \
    "USE ${DATABASE}; "\
"INSERT INTO t1 (a, b, c) VALUES (10, (SELECT MAX(b) FROM t2), 'subq'); "\
"SELECT a,b,c FROM t1 WHERE a=10;"

cleanup

printf '%s\n' "mysql_parser_corpus_query_function_subquery_surfaces_expectations: ok"
