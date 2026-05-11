#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_ceil_floor_functions_expectations_$$"
HUGE81="999999999999999999999999999999999999999999999999999999999999999999999999999999999"
HUGE82="9999999999999999999999999999999999999999999999999999999999999999999999999999999999"
HUGE82_RESULT="99999999999999999999999999999999999999999999999999999999999999999"

fail() {
    printf '%s\n' "mysql_baseline_ceil_floor_functions_expectations: $1" >&2
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
    "core ceil floor values" \
    "CEIL(NULL)	CEILING(NULL)	FLOOR(NULL)	CEIL(TRUE)	CEILING(FALSE)	FLOOR(TRUE)	CEIL(0)	CEIL(-0)	CEIL(+0)	FLOOR(-0)	FLOOR(+0)
NULL	NULL	NULL	1	0	1	0	0	0	0	0" \
    "SELECT CEIL(NULL),CEILING(NULL),FLOOR(NULL),CEIL(TRUE),CEILING(FALSE),FLOOR(TRUE),
            CEIL(0),CEIL(-0),CEIL(+0),FLOOR(-0),FLOOR(+0);" \
    "$DATABASE"

expect_output_with_headers \
    "integer boundaries" \
    "CEIL(-1)	CEILING(-1)	FLOOR(-1)	CEIL(9223372036854775807)	FLOOR(-9223372036854775808)	CEIL(9223372036854775808)	FLOOR(-9223372036854775809)	CEIL(18446744073709551615)	FLOOR(-18446744073709551615)
-1	-1	-1	9223372036854775807	-9223372036854775808	9223372036854775808	-9223372036854775809	18446744073709551615	-18446744073709551615" \
    "SELECT CEIL(-1),CEILING(-1),FLOOR(-1),CEIL(9223372036854775807),
            FLOOR(-9223372036854775808),CEIL(9223372036854775808),
            FLOOR(-9223372036854775809),CEIL(18446744073709551615),
            FLOOR(-18446744073709551615);" \
    "$DATABASE"

expect_output_with_headers \
    "admitted huge exact integer literals" \
    "ceil_huge	floor_huge
${HUGE81}	-${HUGE81}" \
    "SELECT CEIL(${HUGE81}) AS ceil_huge,FLOOR(-${HUGE81}) AS floor_huge;" \
    "$DATABASE"

expect_output_with_headers \
    "bitwise and arithmetic operands" \
    "a	b	c	d	e	f
1	18446744073709551615	9223372036854775808	0	2	-7" \
    "SELECT CEIL(5&3) AS a,CEILING(~0) b,FLOOR(1<<63) c,
            CEIL(1<<64) d,FLOOR(5 DIV 2) e,CEILING(IFNULL(NULL,-7)) f;" \
    "$DATABASE"

expect_output \
    "select child warning staging" \
    "NULL	0	0
Warning	1365	Division by 0
1	-1" \
    "DO 0; SELECT CEIL(5 DIV 0),@@warning_count,ROW_COUNT();
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do floor child warning staging" \
    "Warning	1365	Division by 0
1	-1" \
    "DO FLOOR(5 DIV 0); SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do rounding without warnings" \
    "0	0" \
    "DO CEIL(NULL),CEILING(-1),FLOOR(18446744073709551615);
     SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "empty ceil arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CEIL'" \
    "SELECT CEIL();" \
    "$DATABASE"

expect_error \
    "extra ceil arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CEIL'" \
    "SELECT CEIL(1,2);" \
    "$DATABASE"

expect_error \
    "empty ceiling arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CEILING'" \
    "SELECT CEILING();" \
    "$DATABASE"

expect_error \
    "extra ceiling arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CEILING'" \
    "SELECT CEILING(1,2);" \
    "$DATABASE"

expect_error \
    "empty floor arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'FLOOR'" \
    "SELECT FLOOR();" \
    "$DATABASE"

expect_error \
    "extra floor arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'FLOOR'" \
    "SELECT FLOOR(1,2);" \
    "$DATABASE"

expect_error \
    "ceil child overflow" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT CEIL(3037000500*3037000500);" \
    "$DATABASE"

expect_error \
    "floor child overflow" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT FLOOR(3037000500*3037000500);" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT CEIL(1.2),CEIL(-1.2),FLOOR(1.8),FLOOR(-1.8),
            CEIL('64'),FLOOR('foo'),CEIL(X'40'),FLOOR(b'1111');
     SELECT CEIL(${HUGE82}) AS huge_pos,FLOOR(-${HUGE82}) AS huge_neg;
     SELECT id,CEIL(id),CEILING(id),FLOOR(id) FROM t ORDER BY id IS NULL,id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "CEIL(1.2)	CEIL(-1.2)	FLOOR(1.8)	FLOOR(-1.8)	CEIL('64')	FLOOR('foo')	CEIL(X'40')	FLOOR(b'1111')
2	-1	1	-2	64	0	64	15
huge_pos	huge_neg
${HUGE82_RESULT}	-${HUGE82_RESULT}
id	CEIL(id)	CEILING(id)	FLOOR(id)
-1	-1	-1	-1
0	0	0	0
1	1	1	1
2	2	2	2
NULL	NULL	NULL	NULL" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_ceil_floor_functions_expectations: ok"
