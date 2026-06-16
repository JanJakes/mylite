#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --binary-as-hex=1 --skip-column-names"
DATABASE="mylite_pipes_as_concat_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_pipes_as_concat_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    expect_value "$label" "$expected" "$output"
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

expect_output \
    "pipes mode scalar behavior" \
    "PIPES_AS_CONCAT	ab	1	12	24	15	abc	0" \
    "SET SESSION sql_mode = 'PIPES_AS_CONCAT';
     SELECT @@sql_mode, 'a'||'b', 'a'||NULL IS NULL, 1||2, 1+2||3, 1||2+3,
            ('a'||'b')||'c', @@warning_count;
     SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "ansi mode includes pipes" \
    "REAL_AS_FLOAT,PIPES_AS_CONCAT,ANSI_QUOTES,IGNORE_SPACE,ONLY_FULL_GROUP_BY,ANSI	xy	0" \
    "SET SESSION sql_mode = 'ANSI';
     SELECT @@sql_mode, 'x'||'y', @@warning_count;" \
    "$DATABASE"

expect_output \
    "no source dual and scalar subquery" \
    "${DATABASE}:x	test-${DATABASE}	ab	19223372036854775808	0
dual-ab" \
    "SET SESSION sql_mode = 'PIPES_AS_CONCAT';
     SELECT DATABASE()||':x', 'test-'||(SELECT DATABASE()), ('a'||'b'),
            1||9223372036854775808, @@warning_count;
     SELECT 'dual-'||'ab' FROM DUAL;" \
    "$DATABASE"

expect_output \
    "nested concat operands" \
    "ab	ab	0" \
    "SET SESSION sql_mode = 'PIPES_AS_CONCAT';
     SELECT CONCAT('a')||'b', 'a'||CONCAT_WS('-', 'b'), @@warning_count;" \
    "$DATABASE"

expect_output \
    "table backed concatenation" \
    "1	a:12	a:2024-01-02	ax
2	b:-3	NULL	NULL
3	NULL	NULL	NULL" \
    "SET SESSION sql_mode = 'PIPES_AS_CONCAT';
     CREATE TABLE t(id INT, s VARCHAR(10), n INT, d DATE, nullable VARCHAR(10));
     INSERT INTO t VALUES
        (1, 'a', 12, '2024-01-02', 'x'),
        (2, 'b', -3, NULL, NULL),
        (3, NULL, NULL, '2024-12-31', 'z');
     SELECT id, s||':'||n, s||':'||d, s||nullable FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "do statement conventions" \
    "0	0" \
    "SET SESSION sql_mode = 'PIPES_AS_CONCAT';
     DO 'a'||'b', 1||2;
     SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "inactive mode remains deprecated logical or" \
    "1	0	4
Warning	1287	'|| as a synonym for OR' is deprecated and will be removed in a future release. Please use OR instead
Warning	1287	'|| as a synonym for OR' is deprecated and will be removed in a future release. Please use OR instead
Warning	1292	Truncated incorrect DOUBLE value: 'a'
Warning	1292	Truncated incorrect DOUBLE value: 'b'" \
    "SET SESSION sql_mode = '';
     SELECT 1||0, 'a'||'b', @@warning_count;
     SHOW WARNINGS;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_pipes_as_concat_expectations: ok"
