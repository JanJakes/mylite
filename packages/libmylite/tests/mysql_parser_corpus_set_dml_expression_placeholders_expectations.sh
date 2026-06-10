#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_set_dml_expr_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_set_dml_expression_placeholders: $1" >&2
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
"CREATE TABLE t (id INT PRIMARY KEY, x INT, y DECIMAL(10,4)); "\
"INSERT INTO t VALUES (1, 2, 3.0000);" \
    >/dev/null

expect_output \
    "set expression assignments" \
    "7	2	2" \
    "USE ${DATABASE}; SET @a = 1 + 2 * 3, @b = 10 DIV 4, "\
"@c = 1 + (SELECT COUNT(*) FROM t); SELECT @a, @b, @c;"

expect_output \
    "insert values expressions" \
    "10	2.5000" \
    "USE ${DATABASE}; INSERT INTO t VALUES (2, 2 * 5, 10 / 4); "\
"SELECT x, y FROM t WHERE id = 2;"

expect_output \
    "insert unary boolean value expression" \
    "1	0.0000" \
    "USE ${DATABASE}; INSERT INTO t VALUES (3, +TRUE, -FALSE); "\
"SELECT x, y FROM t WHERE id = 3;"

expect_output \
    "duplicate update expressions" \
    "11	3.5000" \
    "USE ${DATABASE}; INSERT INTO t VALUES (2, 1, 1) "\
"ON DUPLICATE KEY UPDATE x = VALUES(x) + 10, y = y + 1.0000; "\
"SELECT x, y FROM t WHERE id = 2;"

expect_output \
    "update assignment expressions" \
    "22	1.7500" \
    "USE ${DATABASE}; UPDATE t SET x = x * 2, y = y / 2 WHERE id = 2; "\
"SELECT x, y FROM t WHERE id = 2;"

cleanup

printf '%s\n' "mysql_parser_corpus_set_dml_expression_placeholders: ok"
