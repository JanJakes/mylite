#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_bit_count_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_bit_count_function_expectations: $1" >&2
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
    "core bit count values" \
    "BIT_COUNT(NULL)	BIT_COUNT(0)	BIT_COUNT(1)	BIT_COUNT(64)	BIT_COUNT(127)	BIT_COUNT(TRUE)	BIT_COUNT(FALSE)	BIT_COUNT(+1)	BIT_COUNT(-0)	BIT_COUNT(-1)	BIT_COUNT(-2)	BIT_COUNT(-9223372036854775808)	BIT_COUNT(9223372036854775807)	BIT_COUNT(9223372036854775808)	BIT_COUNT(18446744073709551615)
NULL	0	1	1	7	1	0	1	0	64	63	1	63	1	64" \
    "SELECT BIT_COUNT(NULL),BIT_COUNT(0),BIT_COUNT(1),BIT_COUNT(64),BIT_COUNT(127),
            BIT_COUNT(TRUE),BIT_COUNT(FALSE),BIT_COUNT(+1),BIT_COUNT(-0),
            BIT_COUNT(-1),BIT_COUNT(-2),BIT_COUNT(-9223372036854775808),
            BIT_COUNT(9223372036854775807),BIT_COUNT(9223372036854775808),
            BIT_COUNT(18446744073709551615);" \
    "$DATABASE"

expect_output_with_headers \
    "dual expression operands" \
    "a	b	c	d	e	f
1	64	1	0	1	3" \
    "SELECT BIT_COUNT(5&3) AS a,BIT_COUNT(~0) b,BIT_COUNT(1<<63) c,
            BIT_COUNT(1<<64) d,BIT_COUNT(5 DIV 2) e,BIT_COUNT(IFNULL(NULL,7)) f
       FROM DUAL;" \
    "$DATABASE"

expect_output \
    "select child warning staging" \
    "NULL	0	0
Warning	1365	Division by 0
1	-1" \
    "DO 0; SELECT BIT_COUNT(5 DIV 0),@@warning_count,ROW_COUNT();
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do bit count without warnings" \
    "0	0" \
    "DO BIT_COUNT(64),BIT_COUNT(NULL),BIT_COUNT(-1); SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do child warning staging" \
    "Warning	1365	Division by 0
1	-1" \
    "DO BIT_COUNT(5 DIV 0); SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "empty bit count arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'BIT_COUNT'" \
    "SELECT BIT_COUNT();" \
    "$DATABASE"

expect_error \
    "extra bit count arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'BIT_COUNT'" \
    "DO BIT_COUNT(1,2);" \
    "$DATABASE"

expect_error \
    "child overflow under bit count" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT BIT_COUNT(3037000500*3037000500);" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT BIT_COUNT('64'),BIT_COUNT(_binary '64'),BIT_COUNT(X'40'),
            BIT_COUNT(b'1111'),BIT_COUNT(5.5),BIT_COUNT(1e1);
     SELECT id,BIT_COUNT(id) FROM t ORDER BY id IS NULL,id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "BIT_COUNT('64')	BIT_COUNT(_binary '64')	BIT_COUNT(X'40')	BIT_COUNT(b'1111')	BIT_COUNT(5.5)	BIT_COUNT(1e1)
1	7	1	4	2	2
id	BIT_COUNT(id)
-1	64
0	0
1	1
2	1
NULL	NULL" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_bit_count_function_expectations: ok"
