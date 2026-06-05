#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_select_literal_projection_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_select_literal_projection_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 "$@"
}

repeat_char() {
    char=$1
    count=$2
    awk -v char="$char" -v count="$count" 'BEGIN { for (i = 0; i < count; ++i) printf "%s", char }'
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

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
    expect_value "$label" "$expected" "$output"
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t(id INT); INSERT INTO t VALUES (1), (2);" >/dev/null

digits81=$(repeat_char 9 81)
digits82=$(repeat_char 9 82)
digits65=$(repeat_char 9 65)

expect_output_with_headers \
    "no-source literal values and labels" \
    "0001	0001	-0001	NULL	TRUE	false
1	1	-1	NULL	1	0" \
    "SELECT 0001, +0001, -0001, NULL, TRUE, false;" \
    "$DATABASE"

expect_output_with_headers \
    "dual all literal values and labels" \
    "1	1	-1	NULL	TRUE	FALSE
1	1	-1	NULL	1	0" \
    "SELECT ALL 1, +1, -1, NULL, TRUE, FALSE FROM DUAL;" \
    "$DATABASE"

expect_output_with_headers \
    "literal aliases" \
    "one	plus_one	neg	n	t	f
1	1	-1	NULL	1	0" \
    "SELECT 1 AS one, +1 plus_one, -1 AS neg, NULL n, TRUE t, false f;" \
    "$DATABASE"

expect_output \
    "following row count" \
    "1	NULL	1	0
0	-1" \
    "DO 0; SELECT 1, NULL, TRUE, FALSE; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "integer normalization" \
    "1	1	-1	0	0	0" \
    "SELECT 0001, +0001, -0001, 000000, +000000, -000000;" \
    "$DATABASE"

expect_output \
    "81 significant digits warning-free" \
    "${digits81}
0" \
    "SELECT ${digits81}; SELECT @@warning_count;" \
    "$DATABASE"

expect_output \
    "82 significant digits truncates with warning in mysql" \
    "${digits65}
1" \
    "SELECT ${digits82}; SELECT @@warning_count;" \
    "$DATABASE"

expect_output_with_headers \
    "table-backed literal projection" \
    "test
1" \
    "SELECT 1 AS test FROM t WHERE id = 1 LIMIT 1;" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT (1), (+1), (-1), (NULL), (TRUE), (FALSE);
     SELECT 'x', 1.0, 1e0, 0x10, b'10';
     SELECT 1 ORDER BY 1;
     SELECT 1 LIMIT 0;
     SELECT 1 LIMIT 1;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "1	1	(-1)	NULL	(TRUE)	(FALSE)
1	1	-1	NULL	1	0
x	1.0	1e0	0x10	b'10'
x	1.0	1	0x10	0x02
1
1
1
1" \
    "$accepted_but_deferred"
