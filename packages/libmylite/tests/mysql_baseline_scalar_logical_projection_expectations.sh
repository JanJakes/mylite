#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_scalar_logical_projection_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_scalar_logical_projection_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t(id INT); INSERT INTO t VALUES (1), (0), (NULL);" >/dev/null

expect_output_with_headers \
    "and truth table" \
    "1 AND 1	1 AND 0	1 AND NULL	0 AND NULL	NULL AND 0	NULL AND 1	NULL AND NULL
1	0	NULL	0	0	NULL	NULL" \
    "SELECT 1 AND 1, 1 AND 0, 1 AND NULL, 0 AND NULL, NULL AND 0, NULL AND 1, NULL AND NULL;" \
    "$DATABASE"

expect_output_with_headers \
    "or truth table" \
    "1 OR 1	1 OR 0	0 OR 0	0 OR NULL	1 OR NULL	NULL OR NULL
1	1	0	NULL	1	NULL" \
    "SELECT 1 OR 1, 1 OR 0, 0 OR 0, 0 OR NULL, 1 OR NULL, NULL OR NULL;" \
    "$DATABASE"

expect_output_with_headers \
    "xor truth table" \
    "1 XOR 1	1 XOR 0	0 XOR 0	1 XOR NULL	0 XOR NULL	NULL XOR NULL	1 XOR 1 XOR 1
0	1	0	NULL	NULL	NULL	1" \
    "SELECT 1 XOR 1, 1 XOR 0, 0 XOR 0, 1 XOR NULL, 0 XOR NULL, NULL XOR NULL, 1 XOR 1 XOR 1;" \
    "$DATABASE"

expect_output_with_headers \
    "not truth table" \
    "NOT 10	NOT 0	NOT NULL	NOT -1	NOT TRUE	NOT FALSE
0	1	NULL	0	0	1" \
    "SELECT NOT 10, NOT 0, NOT NULL, NOT -1, NOT TRUE, NOT FALSE;" \
    "$DATABASE"

expect_output_with_headers \
    "logical precedence and comparisons" \
    "1<2 AND 2<3	1<2 AND 2>3	1<2 OR 2>3	1>2 OR 2>3	1<2 XOR 2<3	NOT 1<2	NOT (1>2)	0 OR 0 AND 1	1 XOR 1 AND 0
1	0	1	0	0	0	1	0	1" \
    "SELECT 1<2 AND 2<3, 1<2 AND 2>3, 1<2 OR 2>3, 1>2 OR 2>3, 1<2 XOR 2<3, NOT 1<2, NOT (1>2), 0 OR 0 AND 1, 1 XOR 1 AND 0;" \
    "$DATABASE"

expect_output_with_headers \
    "arithmetic and scalar function operands" \
    "1+2 AND 0	0 OR 2*3	5 DIV 2 AND 1	5 % 2 XOR 0	a	b	c	d
0	1	1	1	1	NULL	1	1" \
    "SELECT 1+2 AND 0, 0 OR 2*3, 5 DIV 2 AND 1, 5 % 2 XOR 0, IFNULL(NULL,1) AND 1 AS a, NULLIF(1,1) OR 0 AS b, ISNULL(NULL) XOR FALSE AS c, NOT COALESCE(NULL,0) AS d FROM DUAL;" \
    "$DATABASE"

expect_output_with_headers \
    "logical results compared" \
    "(1 AND 1)=1	(1 AND 0)=0	(1 OR 0)<=>1	(NULL OR 0)<=>NULL
1	1	1	1" \
    "SELECT (1 AND 1)=1, (1 AND 0)=0, (1 OR 0)<=>1, (NULL OR 0)<=>NULL;" \
    "$DATABASE"

expect_output_with_headers \
    "child warning short circuit" \
    "0 AND 5 DIV 0	1 AND 5 DIV 0	NULL AND 5 DIV 0	1 OR 5 DIV 0	0 OR 5 DIV 0	NULL OR 5 DIV 0	1 XOR 5 DIV 0	0 XOR 5 DIV 0	NULL XOR 5 DIV 0	@@warning_count	ROW_COUNT()
0	NULL	NULL	1	NULL	NULL	NULL	NULL	NULL	0	0
Level	Code	Message
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
@@warning_count	ROW_COUNT()
6	-1" \
    "DO 0; SELECT 0 AND 5 DIV 0, 1 AND 5 DIV 0, NULL AND 5 DIV 0, 1 OR 5 DIV 0, 0 OR 5 DIV 0, NULL OR 5 DIV 0, 1 XOR 5 DIV 0, 0 XOR 5 DIV 0, NULL XOR 5 DIV 0, @@warning_count, ROW_COUNT(); SHOW WARNINGS; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "signed boundary logical operands" \
    "9223372036854775807 AND 1	-9223372036854775807 AND 1	(-9223372036854775807-1) OR 0	NOT (-9223372036854775807-1)
1	1	1	0" \
    "SELECT 9223372036854775807 AND 1, -9223372036854775807 AND 1, (-9223372036854775807-1) OR 0, NOT (-9223372036854775807-1);" \
    "$DATABASE"

expect_error \
    "child overflow under logical" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT 3037000500*3037000500 AND 1;" \
    "$DATABASE"

expect_error \
    "logical missing right operand" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT 1 AND;" \
    "$DATABASE"

expect_error \
    "logical missing left operand" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT AND 1;" \
    "$DATABASE"

expect_error \
    "not missing operand" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT NOT;" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT !1, 1&&1, 1||0, 'a' AND 1, 1.5 OR 0, 0x31 AND 1, b'1' XOR 0, 1 + (1 AND 0);
     SELECT id AND 1 FROM t ORDER BY id IS NULL, id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "!1	1&&1	1||0	'a' AND 1	1.5 OR 0	0x31 AND 1	b'1' XOR 0	1 + (1 AND 0)
0	1	1	0	1	1	1	1
id AND 1
0
1
NULL" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_scalar_logical_projection_expectations: ok"
