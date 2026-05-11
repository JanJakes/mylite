#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_sign_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_sign_function_expectations: $1" >&2
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
    "core sign values" \
    "SIGN(NULL)	SIGN(0)	SIGN(-0)	SIGN(+0)	SIGN(1)	SIGN(-1)	SIGN(TRUE)	SIGN(FALSE)	SIGN(+1)	SIGN(-9223372036854775808)	SIGN(-9223372036854775807)	SIGN(9223372036854775807)	SIGN(9223372036854775808)	SIGN(-9223372036854775809)	SIGN(18446744073709551615)	SIGN(-18446744073709551615)	SIGN(18446744073709551616)	SIGN(-18446744073709551616)	SIGN(999999999999999999999999999999999999999999999999999999999999999999)	SIGN(-999999999999999999999999999999999999999999999999999999999999999999)
NULL	0	0	0	1	-1	1	0	1	-1	-1	1	1	-1	1	-1	1	-1	1	-1" \
    "SELECT SIGN(NULL),SIGN(0),SIGN(-0),SIGN(+0),SIGN(1),SIGN(-1),
            SIGN(TRUE),SIGN(FALSE),SIGN(+1),SIGN(-9223372036854775808),
            SIGN(-9223372036854775807),SIGN(9223372036854775807),
            SIGN(9223372036854775808),SIGN(-9223372036854775809),
            SIGN(18446744073709551615),SIGN(-18446744073709551615),
            SIGN(18446744073709551616),SIGN(-18446744073709551616),
            SIGN(999999999999999999999999999999999999999999999999999999999999999999),
            SIGN(-999999999999999999999999999999999999999999999999999999999999999999);" \
    "$DATABASE"

expect_output_with_headers \
    "dual expression operands" \
    "a	b	c	d	e	f
1	1	1	0	1	-1" \
    "SELECT SIGN(5&3) AS a,SIGN(~0) b,SIGN(1<<63) c,
            SIGN(1<<64) d,SIGN(5 DIV 2) e,SIGN(IFNULL(NULL,-7)) f
       FROM DUAL;" \
    "$DATABASE"

expect_output \
    "select child warning staging" \
    "NULL	0	0
Warning	1365	Division by 0
1	-1" \
    "DO 0; SELECT SIGN(5 DIV 0),@@warning_count,ROW_COUNT();
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do sign without warnings" \
    "0	0" \
    "DO SIGN(NULL),SIGN(-1),SIGN(18446744073709551615); SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do child warning staging" \
    "Warning	1365	Division by 0
1	-1" \
    "DO SIGN(5 DIV 0); SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "empty sign arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'SIGN'" \
    "SELECT SIGN();" \
    "$DATABASE"

expect_error \
    "extra sign arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'SIGN'" \
    "DO SIGN(1,2);" \
    "$DATABASE"

expect_error \
    "child overflow under sign" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT SIGN(3037000500*3037000500);" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT SIGN('64'),SIGN('-64'),SIGN(_binary '64'),SIGN(X'40'),SIGN(b'1111'),
            SIGN(5.5),SIGN(-5.5),SIGN(1e1),SIGN(-0.1),SIGN('foo');
     SELECT id,SIGN(id) FROM t ORDER BY id IS NULL,id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "SIGN('64')	SIGN('-64')	SIGN(_binary '64')	SIGN(X'40')	SIGN(b'1111')	SIGN(5.5)	SIGN(-5.5)	SIGN(1e1)	SIGN(-0.1)	SIGN('foo')
1	-1	1	1	1	1	-1	1	-1	0
id	SIGN(id)
-1	-1
0	0
1	1
2	1
NULL	NULL" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_sign_function_expectations: ok"
