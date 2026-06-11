#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_order_expr_residuals_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_order_expression_residuals: $1" >&2
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

    output=$(run_mysql "$sql")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_success() {
    label=$1
    sql=$2

    if ! run_mysql "$sql" >/dev/null 2>&1; then
        fail "$label: expected success"
    fi
}

expect_error() {
    label=$1
    sql=$2

    if run_mysql "$sql" >/dev/null 2>&1; then
        fail "$label: expected error"
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
"CREATE TABLE t(a INT, b INT, c INT, id INT); "\
"INSERT INTO t VALUES (1, 1, 1, 1), (2, 1, 1, 2), (3, 1, 1, 3);" >/dev/null

expect_output "logical not scalar values" "1	0	1	NULL	5" \
    "SELECT !0, !(1), NOT 0, !NULL, !0 * 5;"

expect_success "nested do unary not expression" \
    "USE ${DATABASE}; SET @c = '1'; SET @f = 0; "\
"DO ( (@c) RLIKE (COT((!(@f)))) );"

expect_output "window function top-level order" "3
2
1" \
    "USE ${DATABASE}; SELECT id FROM t ORDER BY RANK() OVER (ORDER BY a DESC,b,c), id;"

expect_output "delete expression order key" "1	2" \
    "USE ${DATABASE}; DELETE FROM t ORDER BY (@@GLOBAL.INIT_FILE) ASC LIMIT 1; "\
"SELECT ROW_COUNT(), COUNT(*) FROM t;"

expect_output "update expression order key" "0	2" \
    "USE ${DATABASE}; UPDATE t SET b = b ORDER BY (@@GLOBAL.INIT_FILE) ASC LIMIT 1; "\
"SELECT ROW_COUNT(), COUNT(*) FROM t;"

expect_error "incomplete logical not remains syntax" "SELECT !;"
expect_error "malformed order expression remains syntax" \
    "USE ${DATABASE}; DELETE FROM t ORDER BY (@@GLOBAL.INIT_FILE ASC LIMIT 1;"

cleanup

printf '%s\n' "mysql_parser_corpus_order_expression_residuals: ok"
