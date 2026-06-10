#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_variable_values_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_variable_value_surfaces_expectations: $1" >&2
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
"CREATE TABLE t (id INT PRIMARY KEY, i INT, s VARCHAR(32));" >/dev/null

expect_output \
    "SET variable arithmetic" \
    "9	1021	1017" \
    "USE ${DATABASE}; SET @step = 3, @now = 1000; "\
"SET @step3 = @step * 3; "\
"SET @unix_time = @now + 7 * @step; "\
"SET @mod = @unix_time - @unix_time % @step3; "\
"SELECT @step3, @unix_time, @mod;"

expect_output \
    "DML variable values" \
    "1	7	abc
2	9	abc-x" \
    "USE ${DATABASE}; "\
"SET @id = 1, @i = 7, @s = 'abc', @step = 3; "\
"INSERT INTO t VALUES (@id, @i, @s), (@id + 1, @step * 3, CONCAT(@s, '-x')); "\
"SELECT id, i, s FROM t ORDER BY id;"

expect_output \
    "system variable DML value" \
    "3	0	SYSTEM" \
    "USE ${DATABASE}; "\
"INSERT INTO t VALUES (3, @@warning_count, @@time_zone); "\
"SELECT id, i, s FROM t WHERE id = 3;"

expect_output \
    "direct variable update value" \
    "11" \
    "USE ${DATABASE}; SET @next = 11; UPDATE t SET i = @next WHERE id = 1; "\
"SELECT i FROM t WHERE id = 1;"

expect_output \
    "predicate variable values" \
    "1
2
1" \
    "USE ${DATABASE}; SET @i = 11, @step3 = 9, @pattern = 'abc'; "\
"SELECT id FROM t WHERE i IN (@i, @step3) ORDER BY id; "\
"SELECT id FROM t WHERE s LIKE @pattern ORDER BY id;"

expect_output \
    "system variable arithmetic SET" \
    "1" \
    "USE ${DATABASE}; SET TIMESTAMP = @@TIMESTAMP + 1; SELECT @@TIMESTAMP IS NOT NULL;"

printf '%s\n' "mysql_parser_corpus_variable_value_surfaces_expectations: ok"
