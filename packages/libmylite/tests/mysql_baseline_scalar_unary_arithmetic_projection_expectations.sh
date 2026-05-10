#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_scalar_unary_arithmetic_projection_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_scalar_unary_arithmetic_projection_expectations: $1" >&2
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
    "core unary arithmetic values" \
    "-(1+2)	+(1+2)	- -1	+ -1	- +1	1	-NULL	NULL	-TRUE	+FALSE
-3	3	1	-1	-1	1	NULL	NULL	-1	0" \
    "SELECT -(1+2), +(1+2), - -1, + -1, - +1, + +1, -NULL, +NULL, -TRUE, +FALSE;" \
    "$DATABASE"

expect_output_with_headers \
    "function unary values and aliases" \
    "neg	pos
-10	12" \
    "SELECT -(IFNULL(NULL,5)*2) AS neg, +(3*4) pos FROM DUAL;" \
    "$DATABASE"

expect_output_with_headers \
    "signed lower boundary" \
    "+(-9223372036854775807 - 1)	-9223372036854775807 - 1
-9223372036854775808	-9223372036854775808" \
    "SELECT +(-9223372036854775807 - 1), -9223372036854775807 - 1;" \
    "$DATABASE"

expect_output \
    "row count and warnings" \
    "-3	0	0
0	-1" \
    "DO 0; SELECT -(1+2), @@warning_count, ROW_COUNT(); SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "child overflow under unary" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT -(9223372036854775807+1);" \
    "$DATABASE"

expect_error \
    "child overflow below null propagation" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT NULL * -(9223372036854775807+1);" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT -(-9223372036854775807 - 1);
     SELECT -'2', +'2', -1.5, +1.5, -@@warning_count, +VERSION();
     SELECT -id FROM t ORDER BY id IS NULL, id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "-(-9223372036854775807 - 1)
9223372036854775808
-'2'	2	-1.5	1.5	-@@warning_count	+VERSION()
-2	2	-1.5	1.5	0	8.4.9
-id
0
-1
NULL" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_scalar_unary_arithmetic_projection_expectations: ok"
