#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_scalar_div_projection_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_scalar_div_projection_expectations: $1" >&2
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
    "core div values" \
    "5 DIV 2	-5 DIV 2	5 DIV -2	-5 DIV -2	TRUE DIV 2	FALSE DIV 2	5 DIV TRUE	+5 DIV +2	-5 DIV +2
2	-2	-2	2	0	0	5	2	-2" \
    "SELECT 5 DIV 2, -5 DIV 2, 5 DIV -2, -5 DIV -2, TRUE DIV 2, FALSE DIV 2, 5 DIV TRUE, +5 DIV +2, -5 DIV +2;" \
    "$DATABASE"

expect_output_with_headers \
    "precedence and associativity" \
    "1+5 DIV 2*3	(1+5) DIV 2*3	5 DIV 2+3	5*3 DIV 4	5 DIV 3 DIV 2	-(5 DIV 2)	5 DIV 2 % 2	5 % 2 DIV 1
7	9	5	3	0	-2	0	1" \
    "SELECT 1+5 DIV 2*3, (1+5) DIV 2*3, 5 DIV 2+3, 5*3 DIV 4, 5 DIV 3 DIV 2, -(5 DIV 2), 5 DIV 2 % 2, 5 % 2 DIV 1;" \
    "$DATABASE"

expect_output_with_headers \
    "dual scalar function operands" \
    "a	b	c
2	2	NULL" \
    "SELECT IFNULL(NULL,5) DIV 2 AS a, 5 DIV IF(1,2,3) b, NULLIF(5,5) DIV 2 c FROM DUAL;" \
    "$DATABASE"

expect_output \
    "null operands no warnings" \
    "NULL	NULL	NULL	NULL	0	0
0	-1" \
    "DO 0; SELECT NULL DIV 0, 0 DIV NULL, 5 DIV NULL, NULL DIV 5, @@warning_count, ROW_COUNT(); SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "zero divisor warnings" \
    "5 DIV 0	5 DIV FALSE	@@warning_count	ROW_COUNT()
NULL	NULL	0	0
Level	Code	Message
Warning	1365	Division by 0
Warning	1365	Division by 0
@@warning_count	ROW_COUNT()
2	-1" \
    "DO 0; SELECT 5 DIV 0, 5 DIV FALSE, @@warning_count, ROW_COUNT(); SHOW WARNINGS; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "nested null and zero warnings" \
    "5 DIV 0 DIV 1	5 DIV NULL DIV 0	NULL DIV 0 DIV 0	(5 DIV 0) + 1	@@warning_count
NULL	NULL	NULL	NULL	0
Level	Code	Message
Warning	1365	Division by 0
Warning	1365	Division by 0
@@warning_count
2" \
    "DO 0; SELECT 5 DIV 0 DIV 1, 5 DIV NULL DIV 0, NULL DIV 0 DIV 0, (5 DIV 0) + 1, @@warning_count; SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

expect_output_with_headers \
    "left null short circuit warnings" \
    "NULL DIV (5 DIV 0)	+NULL DIV (5 DIV 0)	IFNULL(NULL,NULL) DIV (5 DIV 0)	COALESCE(NULL,NULL) DIV (5 DIV 0)	NULLIF(NULL,5 DIV 0) DIV (5 DIV 0)	NULLIF(1,1) DIV (5 DIV 0)	(5 DIV 0) DIV (5 DIV 0)	@@warning_count
NULL	NULL	NULL	NULL	NULL	NULL	NULL	0
Level	Code	Message
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
@@warning_count
3" \
    "DO 0; SELECT NULL DIV (5 DIV 0), +NULL DIV (5 DIV 0), IFNULL(NULL,NULL) DIV (5 DIV 0), COALESCE(NULL,NULL) DIV (5 DIV 0), NULLIF(NULL,5 DIV 0) DIV (5 DIV 0), NULLIF(1,1) DIV (5 DIV 0), (5 DIV 0) DIV (5 DIV 0), @@warning_count; SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

expect_output_with_headers \
    "signed boundary values" \
    "9223372036854775807 DIV 2	-9223372036854775807 DIV 2	(-9223372036854775807-1) DIV 2	(-9223372036854775807-1) DIV -2	9223372036854775807 DIV -1	9223372036854775807 DIV 1	9223372036854775807 DIV -9223372036854775807	(-9223372036854775807-1) DIV 9223372036854775807
4611686018427387903	-4611686018427387903	-4611686018427387904	4611686018427387904	-9223372036854775807	9223372036854775807	-1	-1" \
    "SELECT 9223372036854775807 DIV 2, -9223372036854775807 DIV 2, (-9223372036854775807-1) DIV 2, (-9223372036854775807-1) DIV -2, 9223372036854775807 DIV -1, 9223372036854775807 DIV 1, 9223372036854775807 DIV -9223372036854775807, (-9223372036854775807-1) DIV 9223372036854775807;" \
    "$DATABASE"

expect_error \
    "signed minimum div one" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT (-9223372036854775807-1) DIV 1;" \
    "$DATABASE"

expect_error \
    "signed minimum div negative one" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT (-9223372036854775807-1) DIV -1;" \
    "$DATABASE"

expect_error \
    "child overflow under div" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT 3037000500*3037000500 DIV 2;" \
    "$DATABASE"

expect_error \
    "div function syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT DIV(5,2);" \
    "$DATABASE"

expect_error \
    "div missing right operand" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT 5 DIV;" \
    "$DATABASE"

expect_error \
    "div missing left operand" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT DIV 2;" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT '5' DIV 2, 5 DIV '2', 5.5 DIV 2, 5 DIV 2.5;
     SELECT id DIV 2 FROM t ORDER BY id IS NULL, id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "'5' DIV 2	5 DIV '2'	5.5 DIV 2	5 DIV 2.5
2	2	2	2
id DIV 2
0
0
NULL" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_scalar_div_projection_expectations: ok"
