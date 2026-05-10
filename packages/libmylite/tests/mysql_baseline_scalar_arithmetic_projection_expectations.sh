#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_scalar_arithmetic_projection_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_scalar_arithmetic_projection_expectations: $1" >&2
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

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5
    shift 5

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
        fail "$label: expected error $code/$state, command succeeded with [$output]"
    fi

    case "$output" in
        *"ERROR $code ($state)"*"$message"*) ;;
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t(id INT); INSERT INTO t VALUES (0), (1), (NULL);" >/dev/null

expect_output_with_headers \
    "core arithmetic values" \
    "1+2*3	(1+2)*3	7-10	3*-2	TRUE+2	FALSE*9	NULL+1	IF(1,2,3)+4	IFNULL(NULL,5)*2	COALESCE(NULL,7)-2	NULLIF(8,8)+1	ISNULL(NULL)+9
7	9	-3	-6	3	0	NULL	6	10	5	NULL	10" \
    "SELECT 1+2*3, (1+2)*3, 7-10, 3*-2, TRUE+2, FALSE*9, NULL+1, IF(1,2,3)+4, IFNULL(NULL,5)*2, COALESCE(NULL,7)-2, NULLIF(8,8)+1, ISNULL(NULL)+9;" \
    "$DATABASE"

expect_output_with_headers \
    "dual aliases and all" \
    "sum	diff	product	nullable
3	-2	12	NULL" \
    "SELECT ALL 1+2 AS sum, 5-7 diff, 3*4 product, NULL*9 AS nullable FROM DUAL;" \
    "$DATABASE"

expect_output_with_headers \
    "parenthesized arithmetic labels" \
    "(1+2)	(1+2)*3	(+2)+(-3)	((1+2)*3)
3	9	-1	9" \
    "SELECT (1+2), (1+2)*3, (+2)+(-3), ((1+2)*3);" \
    "$DATABASE"

expect_output \
    "row count and warnings" \
    "3	0	0
0	-1" \
    "DO 0; SELECT 1+2, @@warning_count, ROW_COUNT(); SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "signed lower boundary" \
    "-9223372036854775807 - 1	3037000499*3037000499	-3037000499*3037000499
-9223372036854775808	9223372030926249001	-9223372030926249001" \
    "SELECT -9223372036854775807 - 1, 3037000499*3037000499, -3037000499*3037000499;" \
    "$DATABASE"

expect_error \
    "addition overflow" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT 9223372036854775807+1;" \
    "$DATABASE"

expect_error \
    "subtraction overflow" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT 9223372036854775807 - -1;" \
    "$DATABASE"

expect_error \
    "multiplication overflow" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT 3037000500*3037000500;" \
    "$DATABASE"

expect_error \
    "overflow below null propagation" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT NULL * (9223372036854775807 + 1);" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT -(1+2);
     SELECT 1/0, 1 DIV 0, 1%0, 1 MOD 0;
     SELECT 1 + '2';
     SELECT 1.5 + 2;
     SELECT VERSION()+1;
     SELECT 1 + @@warning_count;
     SELECT 1+id FROM t ORDER BY id IS NULL, id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "-(1+2)
-3
1/0	1 DIV 0	1%0	1 MOD 0
NULL	NULL	NULL	NULL
1 + '2'
3
1.5 + 2
3.5
VERSION()+1
9.4
1 + @@warning_count
2
1+id
1
2
NULL" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_scalar_arithmetic_projection_expectations: ok"
