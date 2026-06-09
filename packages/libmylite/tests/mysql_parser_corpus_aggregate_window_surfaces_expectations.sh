#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_aggregate_window_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_aggregate_window_surfaces_expectations: $1" >&2
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
"CREATE TABLE t (id INT, a INT, b INT, c INT, d INT, k INT, j INT, "\
"name VARCHAR(16), sex CHAR(1), dt DATE); "\
"INSERT INTO t VALUES "\
"(1,1,2,3,4,1,10,'ann','F','2024-01-01'), "\
"(2,2,4,6,8,1,20,'bob','M','2024-01-02');" >/dev/null

expect_output \
    "multi argument group concat" \
    "12|24" \
    "USE ${DATABASE}; SELECT GROUP_CONCAT(a,b ORDER BY c,d SEPARATOR '|') FROM t;"

expect_output \
    "literal aggregate arguments" \
    "0	7	7	2.0000" \
    "USE ${DATABASE}; SELECT COUNT(DISTINCT 1,NULL), MIN(7), MAX(7), AVG(2) FROM t;"

expect_output \
    "arithmetic aggregate and window arguments" \
    "1.0000	4	1" \
    "USE ${DATABASE}; SELECT SUM(a/b), SUM(k+1), SUM(1) OVER () FROM t GROUP BY (k);"

expect_output \
    "interval range window frame" \
    "1
2" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) OVER (ORDER BY dt RANGE INTERVAL 1 DAY PRECEDING) FROM t ORDER BY id;"

expect_output \
    "string grouping with rollup" \
    "1foo,2foo
1foo,2foo" \
    "USE ${DATABASE}; SELECT GROUP_CONCAT(a,'foo') AS f1 FROM t GROUP BY 'x' WITH ROLLUP;"

cleanup

printf '%s\n' "mysql_parser_corpus_aggregate_window_surfaces_expectations: ok"
