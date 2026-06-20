#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_select_clause_residuals_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_select_clause_residuals_expectations: $1" >&2
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
"CREATE TABLE t1(id INT PRIMARY KEY, a INT, b VARCHAR(20)) ENGINE=InnoDB; "\
"CREATE TABLE t2(id INT PRIMARY KEY, a INT, b VARCHAR(20)) ENGINE=InnoDB; "\
"INSERT INTO t1 VALUES (1,10,'hello'),(2,20,'world'); "\
"INSERT INTO t2 VALUES (1,10,'x'),(2,30,'y');" >/dev/null

expect_output \
    "tableless limit zero" \
    "" \
    "SELECT 0 LIMIT 0;"

expect_output \
    "tableless limit offset" \
    "" \
    "SELECT 1 AS a LIMIT 1,10;"

expect_output \
    "tableless limit one" \
    "1" \
    "SELECT 1 AS a LIMIT 1;"

expect_output \
    "constant order null" \
    "1
2" \
    "USE ${DATABASE}; SELECT id FROM t1 ORDER BY NULL;"

expect_output \
    "constant order string" \
    "1
2" \
    "USE ${DATABASE}; SELECT id FROM t1 ORDER BY 'a' DESC;"

expect_output \
    "user variable order key" \
    "1
2" \
    "USE ${DATABASE}; SELECT id FROM t1 ORDER BY @rank;"

expect_output \
    "assigned user variable order key" \
    "1
2" \
    "USE ${DATABASE}; SET @rank = 7; SELECT id FROM t1 ORDER BY @rank DESC;"

expect_output \
    "having string rhs" \
    "10	hello" \
    "USE ${DATABASE}; SELECT a,b FROM t1 GROUP BY a,b HAVING b='hello';"

expect_output \
    "having alias equality" \
    "10	10" \
    "USE ${DATABASE}; "\
"SELECT t1.a AS t1c1, t2.a AS t2c1 FROM t1 JOIN t2 ON t1.id=t2.id "\
"HAVING t1c1 = t2c1;"

expect_output \
    "having alias comparison" \
    "20	30" \
    "USE ${DATABASE}; "\
"SELECT t1.a AS t1c1, t2.a AS t2c1 FROM t1 JOIN t2 ON t1.id=t2.id "\
"HAVING t1c1 != t2c1;"

expect_output \
    "having in" \
    "10
20" \
    "USE ${DATABASE}; SELECT a FROM t1 GROUP BY a HAVING a IN (10,20) ORDER BY a;"

expect_output \
    "multi locking clauses" \
    "1	1
2	2" \
    "USE ${DATABASE}; "\
"SELECT t1.id,t2.id FROM t1 JOIN t2 ON t1.id=t2.id "\
"FOR SHARE OF t1 FOR UPDATE OF t2;"

expect_output \
    "multi locking clauses wait" \
    "1	1
2	2" \
    "USE ${DATABASE}; "\
"SELECT t1.id,t2.id FROM t1 JOIN t2 ON t1.id=t2.id "\
"FOR SHARE OF t1 NOWAIT FOR UPDATE OF t2 SKIP LOCKED;"

cleanup

printf '%s\n' "mysql_parser_corpus_select_clause_residuals_expectations: ok"
