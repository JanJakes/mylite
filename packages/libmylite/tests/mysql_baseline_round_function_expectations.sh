#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_round_function_expectations_$$"
HUGE81="999999999999999999999999999999999999999999999999999999999999999999999999999999999"

fail() {
    printf '%s\n' "mysql_baseline_round_function_expectations: $1" >&2
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t(id INT);
           INSERT INTO t VALUES (0), (1), (2), (NULL), (-1);" >/dev/null

expect_output_with_headers \
    "core round values" \
    "ROUND(NULL)	ROUND(TRUE)	ROUND(FALSE)	ROUND(0)	ROUND(-0)	ROUND(+0)	ROUND(-1)
NULL	1	0	0	0	0	-1" \
    "SELECT ROUND(NULL),ROUND(TRUE),ROUND(FALSE),ROUND(0),ROUND(-0),ROUND(+0),ROUND(-1);" \
    "$DATABASE"

expect_output_with_headers \
    "integer boundaries" \
    "ROUND(9223372036854775807)	ROUND(-9223372036854775808)	ROUND(9223372036854775808)	ROUND(18446744073709551615)	ROUND(-18446744073709551615)
9223372036854775807	-9223372036854775808	9223372036854775808	18446744073709551615	-18446744073709551615" \
    "SELECT ROUND(9223372036854775807),ROUND(-9223372036854775808),
            ROUND(9223372036854775808),ROUND(18446744073709551615),
            ROUND(-18446744073709551615);" \
    "$DATABASE"

expect_output_with_headers \
    "admitted huge exact integer literals" \
    "round_huge	round_neg_huge
${HUGE81}	-${HUGE81}" \
    "SELECT ROUND(${HUGE81}) AS round_huge,ROUND(-${HUGE81}) AS round_neg_huge;" \
    "$DATABASE"

expect_output_with_headers \
    "bitwise and arithmetic operands" \
    "a	b	c	d	e
18446744073709551615	0	7	2	-7" \
    "SELECT ROUND(~0) AS a,ROUND(1<<64) b,ROUND(1+2*3) c,
            ROUND(5 DIV 2) d,ROUND(IFNULL(NULL,-7)) e;" \
    "$DATABASE"

expect_output \
    "select child warning staging" \
    "NULL	0	0
Warning	1365	Division by 0
1	-1" \
    "DO 0; SELECT ROUND(5 DIV 0),@@warning_count,ROW_COUNT();
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do round child warning staging" \
    "Warning	1365	Division by 0" \
    "DO ROUND(5 DIV 0); SHOW WARNINGS;" \
    "$DATABASE"

expect_error \
    "empty round arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ROUND'" \
    "SELECT ROUND();" \
    "$DATABASE"

expect_error \
    "extra round arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ROUND'" \
    "SELECT ROUND(1,2,3);" \
    "$DATABASE"

expect_error \
    "round child overflow" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT ROUND(3037000500*3037000500);" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT ROUND(123,0),ROUND(123,2),ROUND(123,-1),ROUND(999,-2),
            ROUND(NULL,1),ROUND(123,NULL),ROUND(123,TRUE),ROUND(123,FALSE);
     SELECT ROUND(1.5),ROUND(-1.5),ROUND(25E-1),ROUND('64'),ROUND(X'40'),ROUND(b'1111');
     SELECT id,ROUND(id),ROUND(id,-1) FROM t ORDER BY id IS NULL,id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "ROUND(123,0)	ROUND(123,2)	ROUND(123,-1)	ROUND(999,-2)	ROUND(NULL,1)	ROUND(123,NULL)	ROUND(123,TRUE)	ROUND(123,FALSE)
123	123	120	1000	NULL	NULL	123	123
ROUND(1.5)	ROUND(-1.5)	ROUND(25E-1)	ROUND('64')	ROUND(X'40')	ROUND(b'1111')
2	-2	2	64	64	15
id	ROUND(id)	ROUND(id,-1)
-1	-1	0
0	0	0
1	1	0
2	2	0
NULL	NULL	NULL" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_round_function_expectations: ok"
