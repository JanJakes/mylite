#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_function_expression_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_function_expression_placeholders: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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

expect_failure() {
    label=$1
    sql=$2
    shift 2

    if run_mysql "$sql" "$@" >/dev/null 2>&1; then
        fail "$label: command unexpectedly succeeded"
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_success \
    "function expression surfaces accepted" \
    "USE ${DATABASE}; "\
"CREATE TABLE t (a INT, b INT, word VARCHAR(20), grp INT, name VARCHAR(20), fld3 VARCHAR(20)); "\
"INSERT INTO t VALUES (1, 2, 'x', 1, 'alice', 'd%'), (2, 3, 'y', 1, 'bob', 'e%'); "\
"SELECT HEX(WEIGHT_STRING('a' AS CHAR(1))); "\
"SELECT COUNT(DISTINCT a) FROM t GROUP BY b HAVING COUNT(DISTINCT a) > 0; "\
"SELECT GROUP_CONCAT(name ORDER BY name SEPARATOR ',') FROM t GROUP BY grp; "\
"SELECT word FROM t ORDER BY word, HEX(word); "\
"SELECT * FROM t WHERE word = CAST(0x78 AS CHAR); "\
"INSERT INTO t(word) VALUES (DATE_FORMAT('2004-02-02','%M')); "\
"UPDATE t SET a = DATE_ADD(NULL, INTERVAL 1 DAY); "\
"DELETE FROM t WHERE fld3 = 'd%' ORDER BY RAND();" \
    "$DATABASE"

expect_failure \
    "malformed function arguments rejected" \
    "USE ${DATABASE}; SELECT f(1,,2);" \
    "$DATABASE"

expect_failure \
    "dangling operator rejected" \
    "USE ${DATABASE}; SELECT ABS(1) +;" \
    "$DATABASE"

cleanup

printf '%s\n' "mysql_parser_corpus_function_expression_placeholders: ok"
