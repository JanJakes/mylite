#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_optimizer_hints_noop_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_optimizer_hints_noop_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null
run_mysql \
    "CREATE TABLE t (id INT PRIMARY KEY, v INT, s VARCHAR(20)); "\
"INSERT INTO t VALUES (1, 10, 'a'), (2, 20, 'b'), (3, 30, 'c');" \
    "$DATABASE" >/dev/null

expect_output \
    "select valid hint result" \
    "1	10
-1	0	0" \
    "SELECT /*+ MAX_EXECUTION_TIME(1000) */ id, v FROM t WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count;" \
    "$DATABASE"

expect_output \
    "select set_var hint diagnostics" \
    "3
-1	0	0" \
    "SELECT /*+ SET_VAR(sort_buffer_size=262144) */ COUNT(*) FROM t; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count;" \
    "$DATABASE"

expect_output \
    "insert valid hint diagnostics" \
    "1	0	0" \
    "INSERT /*+ SET_VAR(sort_buffer_size=262144) */ INTO t VALUES (4, 40, 'd'); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count;" \
    "$DATABASE"

expect_output \
    "replace valid hint diagnostics" \
    "1	0	0" \
    "REPLACE /*+ SET_VAR(sort_buffer_size=262144) */ INTO t VALUES (5, 50, 'e'); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count;" \
    "$DATABASE"

expect_output \
    "update valid hint diagnostics" \
    "1	0	0" \
    "UPDATE /*+ SET_VAR(sort_buffer_size=262144) */ t SET v = 25 WHERE id = 2; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count;" \
    "$DATABASE"

expect_output \
    "delete valid hint diagnostics" \
    "1	0	0" \
    "DELETE /*+ SET_VAR(sort_buffer_size=262144) */ FROM t WHERE id = 4; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count;" \
    "$DATABASE"

expect_output \
    "trailing hint-shaped comment is ordinary comment" \
    "1
-1	0	0" \
    "SELECT 1 /*+ NO_SUCH_HINT() */; SELECT ROW_COUNT(), @@warning_count, @@error_count;" \
    "$DATABASE"

invalid_output=$(
    run_mysql \
        "SELECT /*+ NO_SUCH_HINT() */ 1; SHOW WARNINGS;" \
        "$DATABASE"
)
case "$invalid_output" in
    *"1
Warning	1064	Optimizer hint syntax error near 'NO_SUCH_HINT() */ 1' at line 1"*) ;;
    *) fail "invalid hint warning: got [$invalid_output]" ;;
esac

printf '%s\n' "baseline-optimizer-hints-noop MySQL 8.4.9 expectations verified"
