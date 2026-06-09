#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_scalar_predicate_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_scalar_predicate_surfaces_expectations: $1" >&2
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
"CREATE TABLE t (id INT PRIMARY KEY, name VARCHAR(16)); "\
"INSERT INTO t VALUES (1, 'alpha'), (2, 'beta'), (3, 'gamma');" >/dev/null

expect_output \
    "scalar BETWEEN expressions" \
    "1	0" \
    "USE ${DATABASE}; SELECT 2 BETWEEN 1 AND 3, 2 NOT BETWEEN 1 AND 3;"

expect_output \
    "string BETWEEN expression" \
    "1" \
    "USE ${DATABASE}; SELECT 'b' BETWEEN 'a' AND 'c';"

expect_output \
    "EXISTS expressions" \
    "1	1" \
    "USE ${DATABASE}; SELECT EXISTS (SELECT 1 FROM t WHERE id = 1), "\
"NOT EXISTS (SELECT 1 FROM t WHERE id = 7);"

expect_output \
    "WHERE predicate baseline" \
    "1	alpha
2	beta" \
    "USE ${DATABASE}; SELECT id, name FROM t WHERE id BETWEEN 1 AND 2 ORDER BY id;"

cleanup

printf '%s\n' "mysql_parser_corpus_scalar_predicate_surfaces_expectations: ok"
