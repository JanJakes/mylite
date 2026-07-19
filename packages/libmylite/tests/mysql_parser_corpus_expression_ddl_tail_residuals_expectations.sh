#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_expr_ddl_tail_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_expression_ddl_tail_residuals: $1" >&2
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
"CREATE TABLE t1(id INT PRIMARY KEY, sex CHAR(1)); "\
"INSERT INTO t1 VALUES (1, 'f'), (2, 'm');" \
    >/dev/null

expect_output \
    "nth value from first" \
    "1	1
2	1" \
    "USE ${DATABASE}; SELECT id, NTH_VALUE(id, 1) FROM FIRST OVER (ORDER BY id) "\
"FROM t1 ORDER BY id;"

expect_error \
    "nth value from last unsupported" \
    "USE ${DATABASE}; SELECT NTH_VALUE(id, 1) FROM LAST OVER (ORDER BY id) FROM t1;"

expect_success \
    "repeated nullability" \
    "USE ${DATABASE}; CREATE TABLE repeated_nullability (a INT NOT NULL NULL, "\
"b INT NULL NOT NULL);"
expect_output \
    "repeated nullability metadata" \
    "a	YES
b	NO" \
    "USE ${DATABASE}; SELECT COLUMN_NAME, IS_NULLABLE FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'repeated_nullability' "\
"ORDER BY ORDINAL_POSITION;"

expect_success \
    "temporal repeated nullability corpus ddl" \
    "USE ${DATABASE}; SET sql_mode = ''; CREATE TABLE temporal_residual ("\
"col_time_not_null_key time not null, "\
"col_timestamp_6_not_null_key timestamp(6) not null NULL DEFAULT 0, "\
"col_datetime_6_not_null_key datetime(6) not null, "\
"col_datetime_6_key datetime(6), "\
"col_time_3_not_null_key time(3) not null, "\
"col_datetime_3_key datetime(3), "\
"key (col_datetime_6_not_null_key), key (col_datetime_3_key));"
expect_output \
    "temporal repeated nullability metadata" \
    "YES" \
    "USE ${DATABASE}; SELECT IS_NULLABLE FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'temporal_residual' "\
"AND COLUMN_NAME = 'col_timestamp_6_not_null_key';"

expect_output \
    "interval bitwise expressions" \
    "NULL
NULL" \
    "SELECT 1^1 + INTERVAL 1+1 SECOND & 1 + INTERVAL 1+1 SECOND; "\
"SELECT 1%2 - INTERVAL 1^1 SECOND | 1%2 - INTERVAL 1^1 SECOND;"

expect_success \
    "create table start transaction" \
    "USE ${DATABASE}; CREATE TABLE binlog_tail (f1 INT) START TRANSACTION;"

expect_error "show master status removed" "SHOW MASTER STATUS;"
expect_error "show slave status removed" "SHOW SLAVE STATUS;"
expect_error "show slave hosts removed" "SHOW SLAVE HOSTS;"
expect_error \
    "lock low priority write rejected" \
    "USE ${DATABASE}; LOCK TABLES t1 LOW_PRIORITY WRITE;"

cleanup

printf '%s\n' "mysql_parser_corpus_expression_ddl_tail_residuals: ok"
