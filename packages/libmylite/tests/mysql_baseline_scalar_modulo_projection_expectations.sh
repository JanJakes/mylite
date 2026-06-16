#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_scalar_modulo_projection_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_scalar_modulo_projection_expectations: $1" >&2
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
    "core modulo values" \
    "5%2	5 MOD 2	MOD(5,2)	-5%2	5%-2	-5%-2	MOD(-5,2)	MOD(5,-2)	MOD(-5,-2)	TRUE%2	FALSE%2	5%TRUE	+5%+2	-5%+2
1	1	1	-1	1	-1	-1	1	-1	1	0	0	1	-1" \
    "SELECT 5%2, 5 MOD 2, MOD(5,2), -5%2, 5%-2, -5%-2, MOD(-5,2), MOD(5,-2), MOD(-5,-2), TRUE%2, FALSE%2, 5%TRUE, +5%+2, -5%+2;" \
    "$DATABASE"

expect_output_with_headers \
    "precedence and associativity" \
    "1+5%2*3	(1+5)%2*3	5%2+3	5*3%4	5%3%2	-(5%2)
4	0	4	3	0	-1" \
    "SELECT 1+5%2*3, (1+5)%2*3, 5%2+3, 5*3%4, 5%3%2, -(5%2);" \
    "$DATABASE"

expect_output_with_headers \
    "dual scalar function operands" \
    "a	b	c	d
1	1	1	NULL" \
    "SELECT IFNULL(NULL,5)%2 AS a, 5%IF(1,2,3) b, MOD(IFNULL(NULL,5),2) c, MOD(NULLIF(5,5),2) d FROM DUAL;" \
    "$DATABASE"

expect_output \
    "null operands no warnings" \
    "NULL	NULL	NULL	NULL	0	0
0	-1" \
    "DO 0; SELECT NULL%0, 0%NULL, NULL MOD 0, 5%NULL, @@warning_count, ROW_COUNT(); SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "zero divisor warnings" \
    "5%0	5 MOD 0	MOD(5,0)	5%FALSE	MOD(5,FALSE)	@@warning_count	ROW_COUNT()
NULL	NULL	NULL	NULL	NULL	0	0
Level	Code	Message
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
@@warning_count	ROW_COUNT()
5	-1" \
    "DO 0; SELECT 5%0, 5 MOD 0, MOD(5,0), 5%FALSE, MOD(5,FALSE), @@warning_count, ROW_COUNT(); SHOW WARNINGS; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "signed boundaries" \
    "9223372036854775807 % 2	-9223372036854775807 % 2	(-9223372036854775807 - 1) % 2	(-9223372036854775807 - 1) % -1
1	-1	0	0" \
    "SELECT 9223372036854775807 % 2, -9223372036854775807 % 2, (-9223372036854775807 - 1) % 2, (-9223372036854775807 - 1) % -1;" \
    "$DATABASE"

expect_error \
    "child overflow under modulo" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT 3037000500*3037000500 % 2;" \
    "$DATABASE"

expect_error \
    "mod no arguments" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT MOD();" \
    "$DATABASE"

expect_error \
    "mod one argument" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT MOD(5);" \
    "$DATABASE"

expect_error \
    "mod three arguments" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT MOD(5,2,1);" \
    "$DATABASE"

accepted_broader_forms=$(run_mysql_with_headers \
    "SELECT '5'%2, 5%'2', 5.5%2, 5%2.5, MOD(5.5,2);
     SELECT id%2 FROM t ORDER BY id IS NULL, id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted broader modulo forms" \
    "'5'%2	5%'2'	5.5%2	5%2.5	MOD(5.5,2)
1	1	1.5	0.0	1.5
id%2
0
1
NULL" \
    "$accepted_broader_forms"

printf '%s\n' "mysql_baseline_scalar_modulo_projection_expectations: ok"
