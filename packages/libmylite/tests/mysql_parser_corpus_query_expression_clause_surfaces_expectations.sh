#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_query_expression_clause_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_query_expression_clause_surfaces_expectations: $1" >&2
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
"INSERT INTO t1 VALUES (1,2,'x'),(3,4,'y'); "\
"INSERT INTO t2 VALUES (1,20),(3,40);" >/dev/null

expect_output \
    "arithmetic predicate" \
    "2" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 WHERE a + 1 > 1;"

expect_output \
    "function order key" \
    "3
1" \
    "USE ${DATABASE}; SELECT a FROM t1 ORDER BY ABS(b - 5);"

expect_output \
    "expression group having order" \
    "1	1
3	1" \
    "USE ${DATABASE}; "\
"SELECT a, COUNT(*) FROM t1 GROUP BY a + 0 "\
"HAVING COUNT(*) >= 1 AND a > 0 ORDER BY a + 0;"

expect_output \
    "row tuple comparison" \
    "1" \
    "USE ${DATABASE}; SELECT a FROM t1 WHERE (a,b) = (1,2);"

expect_output \
    "subquery expression predicate" \
    "1
3" \
    "USE ${DATABASE}; "\
"SELECT a FROM t1 WHERE a IN (SELECT a FROM t2 WHERE b + 1 > 20) ORDER BY a;"

expect_output \
    "multi-table update expression assignment" \
    "1	3
3	5" \
    "USE ${DATABASE}; "\
"UPDATE t1, t2 SET t1.b = t1.b + 1 WHERE t1.a = t2.a; "\
"SELECT a,b FROM t1 ORDER BY a;"

expect_output \
    "ordered delete expression predicate" \
    "1" \
    "USE ${DATABASE}; "\
"DELETE FROM t1 WHERE a = a + sleep(0) ORDER BY a LIMIT 1; "\
"SELECT COUNT(*) FROM t1;"

cleanup

printf '%s\n' "mysql_parser_corpus_query_expression_clause_surfaces_expectations: ok"
