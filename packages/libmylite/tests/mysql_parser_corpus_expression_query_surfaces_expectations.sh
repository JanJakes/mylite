#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_expression_query_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_expression_query_surfaces_expectations: $1" >&2
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

expect_success() {
    label=$1
    sql=$2
    shift 2

    if ! run_mysql "$sql" "$@" >/dev/null; then
        fail "$label: command failed"
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
"CREATE TABLE t (id INT PRIMARY KEY, c VARCHAR(16), n INT); "\
"INSERT INTO t VALUES (1, 'abc', 10), (2, 'def', 20);" >/dev/null

expect_output \
    "unary BINARY and scalar regexp" \
    "61	1	0	1" \
    "USE ${DATABASE}; "\
"SELECT HEX(BINARY 'a'), 'abc' REGEXP 'B', BINARY 'abc' REGEXP BINARY 'B', "\
"'abc' NOT REGEXP 'z';"

expect_output \
    "row constructor comparison" \
    "1" \
    "USE ${DATABASE}; SELECT ROW(1, 2) = ROW(1, 2);"

expect_success \
    "CHAR USING charset" \
    "USE ${DATABASE}; SELECT CHAR(0x41 USING ucs2);"

expect_success \
    "GROUPING with rollup" \
    "USE ${DATABASE}; SELECT c, GROUPING(c), COUNT(*) FROM t GROUP BY c WITH ROLLUP;"

expect_success \
    "expression group and order keys" \
    "USE ${DATABASE}; "\
"SELECT c, COUNT(*) FROM t GROUP BY 1; "\
"SELECT c, COUNT(*) FROM t GROUP BY c COLLATE utf8mb4_0900_ai_ci ORDER BY BINARY c; "\
"SELECT c, SUM(n) FROM t GROUP BY c ORDER BY SUM(n) DESC;"

expect_output \
    "distinct aggregate syntax" \
    "2	30" \
    "USE ${DATABASE}; SELECT COUNT(DISTINCT CONCAT(c, 'x')), SUM(DISTINCT n) FROM t;"

expect_output \
    "group concat distinct syntax" \
    "abc,def" \
    "USE ${DATABASE}; SELECT GROUP_CONCAT(DISTINCT c ORDER BY c SEPARATOR ',') FROM t;"

expect_output \
    "nested VALUES duplicate expression" \
    "30" \
    "USE ${DATABASE}; "\
"INSERT INTO t VALUES (1, 'abc', 30) ON DUPLICATE KEY UPDATE n = GREATEST(n, VALUES(n)); "\
"SELECT n FROM t WHERE id = 1;"

cleanup

printf '%s\n' "mysql_parser_corpus_expression_query_surfaces_expectations: ok"
