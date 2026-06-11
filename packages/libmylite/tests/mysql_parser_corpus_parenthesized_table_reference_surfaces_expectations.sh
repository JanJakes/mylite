#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_parenthesized_table_reference_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_parenthesized_table_reference_surfaces: $1" >&2
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
"CREATE TABLE t2 (id INT PRIMARY KEY, v INT); "\
"CREATE TABLE t3 (id INT PRIMARY KEY, v INT); "\
"INSERT INTO t1 VALUES (1,10),(2,20); "\
"INSERT INTO t2 VALUES (1,100),(3,300); "\
"INSERT INTO t3 VALUES (1,1000),(3,3000);" >/dev/null

expect_output \
    "left-deep parenthesized inner join" \
    "1" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM (t1 JOIN t2 ON t1.id=t2.id) JOIN t3 ON t2.id=t3.id;"

expect_output \
    "right-nested outer join" \
    "2" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t1 LEFT JOIN (t2 LEFT JOIN t3 ON t2.id=t3.id) ON t1.id=t2.id;"

expect_output \
    "parenthesized comma table-reference group" \
    "3" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM t1 LEFT JOIN (t2, t3) ON t1.id=t2.id;"

expect_output \
    "parenthesized base table reference" \
    "2" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM (t1);"

expect_output \
    "doubly parenthesized base table reference" \
    "2" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM ((t1));"

expect_output \
    "doubly parenthesized inner join" \
    "4" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM ((t1 JOIN t2 ON TRUE));"

expect_output \
    "doubly parenthesized comma group" \
    "4" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM ((t1, t2));"

expect_output \
    "ODBC left outer join escape" \
    "2" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM { OJ t1 LEFT OUTER JOIN t2 ON t1.id=t2.id };"

expect_output \
    "mixed comma before explicit join" \
    "4" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t1, t2 LEFT JOIN t3 ON t2.id=t3.id;"

expect_output \
    "mixed comma after explicit join" \
    "4" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t2 LEFT JOIN t3 ON t2.id=t3.id, t1;"

expect_output \
    "delayed outer join condition" \
    "2" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t1 LEFT JOIN t2 LEFT JOIN t3 "\
"ON t2.id=t3.id ON t1.id=t3.id;"

cleanup

printf '%s\n' "mysql_parser_corpus_parenthesized_table_reference_surfaces: ok"
