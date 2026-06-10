#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_expr_operator_temporal_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_expression_operator_temporal_surfaces: $1" >&2
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
"CREATE TABLE t (id INT PRIMARY KEY, c1 VARCHAR(16), c2 VARCHAR(16), d DATE, tm TIME, dt DATETIME); "\
"INSERT INTO t VALUES "\
"(1, 'ab', 'a%', DATE'2024-01-01', TIME'01:02:03', TIMESTAMP'2024-01-01 01:02:03'), "\
"(2, 'xy', 'x_', DATE'2024-01-02', TIME'02:03:04', TIMESTAMP'2024-01-02 02:03:04');" \
    >/dev/null

expect_output \
    "deprecated logical operators" \
    "1	1	0	1" \
    "SELECT 1 && 1, 0 || 1, 0 && 1, 1 || NULL;"

expect_output \
    "not like and escape expressions" \
    "1	1	1	0" \
    "SELECT 'ab' NOT LIKE 'ac', 'a%' LIKE 'a!%' ESCAPE '!', "\
"'a%' NOT LIKE 'a!_' ESCAPE '!', 'ab' LIKE 'a' || '%';"

expect_output \
    "sounds like expression" \
    "1	0" \
    "SELECT 'mood' SOUNDS LIKE 'mud', 'mood' SOUNDS LIKE 'xyz';"

expect_output \
    "column like column predicate" \
    "1
2" \
    "USE ${DATABASE}; SELECT id FROM t WHERE c1 LIKE c2 ORDER BY id;"

expect_output \
    "typed temporal predicate literals" \
    "1
1
1" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM t WHERE d IN (DATE'2024-01-01', DATE'2024-01-03'); "\
"SELECT COUNT(*) FROM t WHERE tm BETWEEN TIME'01:00:00' AND TIME'01:59:59'; "\
"SELECT COUNT(*) FROM t WHERE dt = TIMESTAMP'2024-01-02 02:03:04';"

expect_output \
    "interval arithmetic expressions" \
    "1997-12-31 23:59:59	1998-01-01	10:10:10.6" \
    "SELECT '1998-01-01 00:00:00' - INTERVAL 1 SECOND, "\
"INTERVAL 1 DAY + '1997-12-31', TIME'10:10:10' + INTERVAL .6 SECOND;"

cleanup

printf '%s\n' "mysql_parser_corpus_expression_operator_temporal_surfaces: ok"
