#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_cte_placeholder_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_cte_placeholder_surfaces_expectations: $1" >&2
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
"CREATE TABLE t1 (id INT PRIMARY KEY, v INT); "\
"INSERT INTO t1 VALUES (1, 10), (2, 20);" >/dev/null

expect_output \
    "simple cte select" \
    "1" \
    "USE ${DATABASE}; WITH qn AS (SELECT 1) SELECT * FROM qn;"

expect_output \
    "cte column list" \
    "2" \
    "USE ${DATABASE}; WITH qn(a) AS (SELECT 1), qn2 AS (SELECT a + 1 FROM qn) "\
"SELECT * FROM qn2;"

expect_output \
    "recursive cte select" \
    "1
2
3" \
    "USE ${DATABASE}; WITH RECURSIVE qn(n) AS "\
"(SELECT 1 UNION ALL SELECT n + 1 FROM qn WHERE n < 3) SELECT * FROM qn;"

expect_output \
    "cte parenthesized query expression" \
    "1" \
    "USE ${DATABASE}; WITH cte AS (SELECT 1 AS a) (SELECT * FROM cte) LIMIT 1;"

expect_output \
    "cte update" \
    "20" \
    "USE ${DATABASE}; WITH ids AS (SELECT 1 AS id) "\
"UPDATE t1 SET v = 20 WHERE id IN (SELECT id FROM ids); "\
"SELECT v FROM t1 WHERE id = 1;"

expect_output \
    "cte delete" \
    "1" \
    "USE ${DATABASE}; WITH doomed AS (SELECT 2 AS id) "\
"DELETE FROM t1 WHERE id IN (SELECT id FROM doomed); "\
"SELECT COUNT(*) FROM t1;"

cleanup

printf '%s\n' "mysql_parser_corpus_cte_placeholder_surfaces_expectations: ok"
