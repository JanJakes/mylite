#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_scalar_division_projection_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_scalar_division_projection_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t(id INT, n INT NULL);
           INSERT INTO t VALUES (0, NULL), (1, 2), (2, 0), (-1, -2);" >/dev/null

expect_output_with_headers \
    "core integer division values" \
    "3/5	1/2	1/3	10/4	-5/2	5/-2	-5/-2	TRUE/2	FALSE/2	5/TRUE
0.6000	0.5000	0.3333	2.5000	-2.5000	-2.5000	2.5000	0.5000	0.0000	5.0000" \
    "SELECT 3/5,1/2,1/3,10/4,-5/2,5/-2,-5/-2,TRUE/2,FALSE/2,5/TRUE;" \
    "$DATABASE"

expect_output_with_headers \
    "division rounding values" \
    "1/6	1/7	2/3	1/8	1/20	1/20000	1/200000	-1/6	-1/20000	-1/200000
0.1667	0.1429	0.6667	0.1250	0.0500	0.0001	0.0000	-0.1667	-0.0001	0.0000" \
    "SELECT 1/6,1/7,2/3,1/8,1/20,1/20000,1/200000,-1/6,-1/20000,-1/200000;" \
    "$DATABASE"

expect_output_with_headers \
    "division arithmetic operands" \
    "a	b	c	d	e	f
3.0000	2.5000	2.5000	2.5000	0.5000	5.0000" \
    "SELECT (1+5)/2 AS a,5/(5 DIV 2) b,IFNULL(NULL,5)/2 c,5/IF(1,2,3) d,
            (5%2)/2 e,5/(5 MOD 2) f FROM DUAL;" \
    "$DATABASE"

expect_output_with_headers \
    "division signed boundaries" \
    "9223372036854775807/1	(-9223372036854775807-1)/1	(-9223372036854775807-1)/-1	9223372036854775807/-1	9223372036854775807/2	(-9223372036854775807-1)/2
9223372036854775807.0000	-9223372036854775808.0000	9223372036854775808.0000	-9223372036854775807.0000	4611686018427387903.5000	-4611686018427387904.0000" \
    "SELECT 9223372036854775807/1,(-9223372036854775807-1)/1,
            (-9223372036854775807-1)/-1,9223372036854775807/-1,
            9223372036854775807/2,(-9223372036854775807-1)/2;" \
    "$DATABASE"

expect_output \
    "division warning staging" \
    "NULL	NULL	0	0
Warning	1365	Division by 0
Warning	1365	Division by 0
2	-1" \
    "DO 0; SELECT 5/0,5/FALSE,@@warning_count,ROW_COUNT();
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "division do warning staging" \
    "Warning	1365	Division by 0
1	-1" \
    "DO 5/0; SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "division do no warning" \
    "0	0" \
    "DO 1/2,TRUE/2,NULL/0; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "division null child warning behavior" \
    "NULL	NULL	NULL	NULL	NULL	NULL	NULL	0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
5	-1" \
    "DO 0; SELECT NULL/(5/0),NULLIF(1,1)/(5/0),(5/0)/(5/0),
            IF(0,1,NULL)/(5/0),IFNULL(NULL,NULL)/(5/0),
            COALESCE(NULL,NULL)/(5/0),(NULL+0)/(5/0),@@warning_count;
     SHOW WARNINGS; SELECT @@warning_count,ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "division child overflow" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT 3037000500*3037000500/2;" \
    "$DATABASE"

expect_error \
    "division child overflow behind null" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT NULL/(3037000500*3037000500);" \
    "$DATABASE"

expect_error \
    "prefix slash syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT /2;" \
    "$DATABASE"

expect_error \
    "trailing slash syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT 1/;" \
    "$DATABASE"

accepted_broader_forms=$(run_mysql_with_headers \
    "SELECT 1+5/2*3,5/2/2,5/2%2,5/2 DIV 1,ABS(5/2),IF(1,5/2,3),
            CASE WHEN TRUE THEN 5/2 END,5.5/2,5e0/2,'5'/2,0x10/2,b'1010'/2,X'35'/2;
     SELECT id,n,id/2,n/2 FROM t ORDER BY id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted broader division forms" \
    "1+5/2*3	5/2/2	5/2%2	5/2 DIV 1	ABS(5/2)	IF(1,5/2,3)	CASE WHEN TRUE THEN 5/2 END	5.5/2	5e0/2	'5'/2	0x10/2	b'1010'/2	X'35'/2
8.5000	1.25000000	0.5000	2	2.5000	2.5000	2.5000	2.75000	2.5	2.5	8.0000	5.0000	26.5000
id	n	id/2	n/2
-1	-2	-0.5000	-1.0000
0	NULL	0.0000	NULL
1	2	0.5000	1.0000
2	0	1.0000	0.0000" \
    "$accepted_broader_forms"

printf '%s\n' "mysql_baseline_scalar_division_projection_expectations: ok"
