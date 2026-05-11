#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_sqrt_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_sqrt_function_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
           CREATE TABLE t(id INT);
           INSERT INTO t VALUES (0), (1), (4), (NULL), (-1);" \
    >/dev/null

expect_output_with_headers \
    "core sqrt values" \
    "SQRT(NULL)	SQRT(TRUE)	SQRT(FALSE)	SQRT(0)	SQRT(-0)	SQRT(+0)	SQRT(1)	SQRT(4)	SQRT(9)	SQRT(2)	SQRT(10)	SQRT(20)	SQRT(-1)	SQRT(-16)	SQRT(-9223372036854775808)	SQRT(-18446744073709551615)	@@warning_count	ROW_COUNT()
NULL	1	0	0	0	0	1	2	3	1.4142135623730951	3.1622776601683795	4.47213595499958	NULL	NULL	NULL	NULL	0	0" \
    "DO 0; SELECT SQRT(NULL),SQRT(TRUE),SQRT(FALSE),SQRT(0),SQRT(-0),
            SQRT(+0),SQRT(1),SQRT(4),SQRT(9),SQRT(2),SQRT(10),SQRT(20),
            SQRT(-1),SQRT(-16),SQRT(-9223372036854775808),
            SQRT(-18446744073709551615),@@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "boundary and child values" \
    "SQRT(9223372036854775807)	SQRT(9223372036854775808)	SQRT(18446744073709551615)	SQRT(5&3)	SQRT(~0)	SQRT(1<<63)	SQRT(1<<64)	SQRT(5 DIV 2)	SQRT(IFNULL(NULL,9))
3037000499.97605	3037000499.97605	4294967296	1	4294967296	3037000499.97605	0	1.4142135623730951	3" \
    "SELECT SQRT(9223372036854775807),SQRT(9223372036854775808),
            SQRT(18446744073709551615),SQRT(5&3),SQRT(~0),
            SQRT(1<<63),SQRT(1<<64),SQRT(5 DIV 2),SQRT(IFNULL(NULL,9));" \
    "$DATABASE"

expect_output_with_headers \
    "dual sqrt value" \
    "root
4.47213595499958" \
    "SELECT SQRT(20) AS root FROM DUAL;" \
    "$DATABASE"

expect_output \
    "negative input has no warnings" \
    "NULL
0	0	-1" \
    "SELECT SQRT(-1); SHOW WARNINGS; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "select child warning staging" \
    "NULL	0	0
Warning	1365	Division by 0
1	-1" \
    "DO 0; SELECT SQRT(5 DIV 0),@@warning_count,ROW_COUNT();
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do sqrt status" \
    "0	0" \
    "DO SQRT(NULL),SQRT(4),SQRT(-1); SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do child warning staging" \
    "Warning	1365	Division by 0
1	-1" \
    "DO SQRT(5 DIV 0); SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "empty sqrt arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'SQRT'" \
    "SELECT SQRT();" \
    "$DATABASE"

expect_error \
    "extra sqrt arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'SQRT'" \
    "DO SQRT(1,2);" \
    "$DATABASE"

expect_error \
    "bare sqrt identifier" \
    1054 \
    42S22 \
    "Unknown column 'SQRT' in 'field list'" \
    "SELECT SQRT;" \
    "$DATABASE"

expect_error \
    "child overflow under sqrt" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT SQRT(3037000500*3037000500);" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT SQRT('64'),SQRT(_binary '64'),SQRT(X'40'),SQRT(b'1111'),
            SQRT(5.5),SQRT(1e1),SQRT(18446744073709551616),
            SQRT(999999999999999999999999999999999999999999999999999999999999999999999999999999999);
     SELECT id,SQRT(id) FROM t ORDER BY id IS NULL,id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "SQRT('64')	SQRT(_binary '64')	SQRT(X'40')	SQRT(b'1111')	SQRT(5.5)	SQRT(1e1)	SQRT(18446744073709551616)	SQRT(999999999999999999999999999999999999999999999999999999999999999999999999999999999)
8	8	8	3.872983346207417	2.345207879911715	3.1622776601683795	4294967296	3.162277660168379e40
id	SQRT(id)
-1	NULL
0	0
1	1
4	2
NULL	NULL" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_sqrt_function_expectations: ok"
