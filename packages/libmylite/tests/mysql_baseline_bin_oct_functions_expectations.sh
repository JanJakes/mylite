#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_bin_oct_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_bin_oct_functions_expectations: $1" >&2
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
    "core bin values" \
    "BIN(NULL)	BIN(0)	BIN(1)	BIN(2)	BIN(12)	BIN(TRUE)	BIN(FALSE)	BIN(+1)	BIN(-0)	BIN(-1)	BIN(-2)	BIN(-9223372036854775808)	BIN(9223372036854775807)	BIN(9223372036854775808)	BIN(18446744073709551615)
NULL	0	1	10	1100	1	0	1	0	1111111111111111111111111111111111111111111111111111111111111111	1111111111111111111111111111111111111111111111111111111111111110	1000000000000000000000000000000000000000000000000000000000000000	111111111111111111111111111111111111111111111111111111111111111	1000000000000000000000000000000000000000000000000000000000000000	1111111111111111111111111111111111111111111111111111111111111111" \
    "SELECT BIN(NULL),BIN(0),BIN(1),BIN(2),BIN(12),
            BIN(TRUE),BIN(FALSE),BIN(+1),BIN(-0),BIN(-1),BIN(-2),
            BIN(-9223372036854775808),BIN(9223372036854775807),
            BIN(9223372036854775808),BIN(18446744073709551615);" \
    "$DATABASE"

expect_output_with_headers \
    "core oct values" \
    "OCT(NULL)	OCT(0)	OCT(1)	OCT(8)	OCT(12)	OCT(TRUE)	OCT(FALSE)	OCT(+1)	OCT(-0)	OCT(-1)	OCT(-2)	OCT(-9223372036854775808)	OCT(9223372036854775807)	OCT(9223372036854775808)	OCT(18446744073709551615)
NULL	0	1	10	14	1	0	1	0	1777777777777777777777	1777777777777777777776	1000000000000000000000	777777777777777777777	1000000000000000000000	1777777777777777777777" \
    "SELECT OCT(NULL),OCT(0),OCT(1),OCT(8),OCT(12),
            OCT(TRUE),OCT(FALSE),OCT(+1),OCT(-0),OCT(-1),OCT(-2),
            OCT(-9223372036854775808),OCT(9223372036854775807),
            OCT(9223372036854775808),OCT(18446744073709551615);" \
    "$DATABASE"

expect_output_with_headers \
    "bin expression operands" \
    "a	b	c	d	e	f
1	1111111111111111111111111111111111111111111111111111111111111111	1000000000000000000000000000000000000000000000000000000000000000	0	10	111" \
    "SELECT BIN(5&3) AS a,BIN(~0) b,BIN(1<<63) c,
            BIN(1<<64) d,BIN(5 DIV 2) e,BIN(IFNULL(NULL,7)) f
       FROM DUAL;" \
    "$DATABASE"

expect_output_with_headers \
    "oct expression operands" \
    "a	b	c	d	e	f
1	1777777777777777777777	1000000000000000000000	0	2	7" \
    "SELECT OCT(5&3) AS a,OCT(~0) b,OCT(1<<63) c,
            OCT(1<<64) d,OCT(5 DIV 2) e,OCT(IFNULL(NULL,7)) f
       FROM DUAL;" \
    "$DATABASE"

expect_output_with_headers \
    "row-backed bin oct values" \
    "id	BIN(id)	OCT(id)	CASE WHEN id=1 THEN BIN(id) END	CONCAT('0b',BIN(id+1))
-1	1111111111111111111111111111111111111111111111111111111111111111	1777777777777777777777	NULL	0b0
0	0	0	NULL	0b1
1	1	1	1	0b10
2	10	2	NULL	0b11
NULL	NULL	NULL	NULL	NULL" \
    "SELECT id,BIN(id),OCT(id),CASE WHEN id=1 THEN BIN(id) END,
            CONCAT('0b',BIN(id+1))
       FROM t ORDER BY id IS NULL,id;" \
    "$DATABASE"

expect_output \
    "select child warning staging" \
    "NULL	NULL	0	0
Warning	1365	Division by 0
Warning	1365	Division by 0
2	-1" \
    "DO 0; SELECT BIN(5 DIV 0),OCT(5 DIV 0),@@warning_count,ROW_COUNT();
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do child warning staging" \
    "Warning	1365	Division by 0
Warning	1365	Division by 0
2	-1" \
    "DO BIN(5 DIV 0),OCT(5 DIV 0); SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "empty bin arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'BIN'" \
    "SELECT BIN();" \
    "$DATABASE"

expect_error \
    "extra bin arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'BIN'" \
    "SELECT BIN(1,2);" \
    "$DATABASE"

expect_error \
    "empty oct arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'OCT'" \
    "SELECT OCT();" \
    "$DATABASE"

expect_error \
    "extra oct arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'OCT'" \
    "SELECT OCT(1,2);" \
    "$DATABASE"

expect_error \
    "child overflow under bin" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT BIN(3037000500*3037000500);" \
    "$DATABASE"

expect_error \
    "child overflow under oct" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT OCT(3037000500*3037000500);" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT BIN('64'),BIN(_binary '64'),BIN(X'40'),BIN(b'1111'),BIN(5.5),BIN(1e1),
            OCT('64'),OCT(_binary '64'),OCT(X'40'),OCT(b'1111'),OCT(5.5),OCT(1e1);
     DO 0;
     SELECT BIN(18446744073709551616),OCT(18446744073709551616),@@warning_count,ROW_COUNT();
     SHOW WARNINGS;
     SELECT @@warning_count,ROW_COUNT();
     SELECT BIN(-9223372036854775809),OCT(-9223372036854775809);" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "BIN('64')	BIN(_binary '64')	BIN(X'40')	BIN(b'1111')	BIN(5.5)	BIN(1e1)	OCT('64')	OCT(_binary '64')	OCT(X'40')	OCT(b'1111')	OCT(5.5)	OCT(1e1)
1000000	1000000	1000000	1111	101	1010	100	100	100	17	5	12
BIN(18446744073709551616)	OCT(18446744073709551616)	@@warning_count	ROW_COUNT()
1111111111111111111111111111111111111111111111111111111111111111	1777777777777777777777	0	0
Level	Code	Message
Warning	1292	Truncated incorrect DECIMAL value: '18446744073709551616'
Warning	1292	Truncated incorrect DECIMAL value: '18446744073709551616'
@@warning_count	ROW_COUNT()
2	-1
BIN(-9223372036854775809)	OCT(-9223372036854775809)
111111111111111111111111111111111111111111111111111111111111111	777777777777777777777" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_bin_oct_functions_expectations: ok"
