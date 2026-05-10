#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_scalar_comparison_projection_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_scalar_comparison_projection_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t(id INT); INSERT INTO t VALUES (1), (2), (NULL);" >/dev/null

expect_output_with_headers \
    "core comparison values" \
    "1=1	1=2	1<>2	1!=1	1<2	2<=2	3>2	3>=4	TRUE=1	FALSE=0	-1<0	0>=-1
1	0	1	0	1	1	1	0	1	1	1	1" \
    "SELECT 1=1, 1=2, 1<>2, 1!=1, 1<2, 2<=2, 3>2, 3>=4, TRUE=1, FALSE=0, -1<0, 0>=-1;" \
    "$DATABASE"

expect_output_with_headers \
    "null and null safe comparisons" \
    "1=NULL	NULL=NULL	1<>NULL	NULL<NULL	1<=>NULL	NULL<=>NULL	1<=>1	1<=>2
NULL	NULL	NULL	NULL	0	1	1	0" \
    "SELECT 1=NULL, NULL=NULL, 1<>NULL, NULL<NULL, 1<=>NULL, NULL<=>NULL, 1<=>1, 1<=>2;" \
    "$DATABASE"

expect_output_with_headers \
    "comparison precedence and associativity" \
    "1+2=3	1+2*3=7	1<2=1	1=2<3	2<1=0	1<2<3	3>2>1	NULL=NULL<=>NULL	NULL<=>NULL=1	(NULL<=>NULL)=1
1	1	1	1	1	1	0	1	1	1" \
    "SELECT 1+2=3, 1+2*3=7, 1<2=1, 1=2<3, 2<1=0, 1<2<3, 3>2>1, NULL=NULL<=>NULL, NULL<=>NULL=1, (NULL<=>NULL)=1;" \
    "$DATABASE"

expect_output_with_headers \
    "dual scalar function operands" \
    "a	b	c	d
1	1	1	1" \
    "SELECT IFNULL(NULL,5)=5 AS a, NULLIF(5,5)<=>NULL b, ISNULL(NULL)=TRUE c, COALESCE(NULL,2)>=2 d FROM DUAL;" \
    "$DATABASE"

expect_output_with_headers \
    "child div warning comparisons" \
    "NULL=5 DIV 0	5 DIV 0<=>NULL	NULL DIV (5 DIV 0)<=>NULL	@@warning_count	ROW_COUNT()
NULL	1	1	0	0
Level	Code	Message
Warning	1365	Division by 0
@@warning_count	ROW_COUNT()
1	-1" \
    "DO 0; SELECT NULL=5 DIV 0, 5 DIV 0<=>NULL, NULL DIV (5 DIV 0)<=>NULL, @@warning_count, ROW_COUNT(); SHOW WARNINGS; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "signed boundary comparisons" \
    "9223372036854775807=9223372036854775807	-9223372036854775807<0	(-9223372036854775807-1)<-9223372036854775807	(-9223372036854775807-1)<=>-9223372036854775808
1	1	1	1" \
    "SELECT 9223372036854775807=9223372036854775807, -9223372036854775807<0, (-9223372036854775807-1)<-9223372036854775807, (-9223372036854775807-1)<=>-9223372036854775808;" \
    "$DATABASE"

expect_error \
    "child overflow under comparison" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT 3037000500*3037000500 = 1;" \
    "$DATABASE"

expect_error \
    "comparison missing right operand" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT 1=;" \
    "$DATABASE"

expect_error \
    "comparison missing left operand" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT =1;" \
    "$DATABASE"

expect_error \
    "null safe comparison missing right operand" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT 1<=>;" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT '1'=1, 1='1', 1.0=1, 0x31=49, b'1'=1, 1.5>1;
     SELECT id=1 FROM t ORDER BY id IS NULL, id;
     SELECT (1,2)=(1,2), (1,2)=(1,3), (1,NULL)<=>(1,NULL);" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "'1'=1	1='1'	1.0=1	0x31=49	b'1'=1	1.5>1
1	1	1	1	1	1
id=1
1
0
NULL
(1,2)=(1,2)	(1,2)=(1,3)	(1,NULL)<=>(1,NULL)
1	0	1" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_scalar_comparison_projection_expectations: ok"
