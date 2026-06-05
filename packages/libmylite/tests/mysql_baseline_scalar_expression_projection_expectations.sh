#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_scalar_expression_projection_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_scalar_expression_projection_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t(id INT); INSERT INTO t VALUES (1), (NULL), (0);" >/dev/null

expect_output_with_headers \
    "mixed scalar value projection" \
    "1	NULL	TRUE	FALSE	2	-3	IF(1,4,5)	IFNULL(NULL,6)	COALESCE(NULL,7)	NULLIF(8,8)	ISNULL(NULL)
1	NULL	1	0	2	-3	4	6	7	NULL	1" \
    "SELECT 1, NULL, TRUE, FALSE, +2, -3, IF(1,4,5), IFNULL(NULL,6), COALESCE(NULL,7), NULLIF(8,8), ISNULL(NULL);" \
    "$DATABASE"

expect_output_with_headers \
    "dual aliases and all" \
    "one	if_result	n	bare_alias
1	5	9	0" \
    "SELECT ALL 1 AS one, IF(0,4,5) if_result, NULLIF(9,10) AS n, ISNULL(1) bare_alias FROM DUAL;" \
    "$DATABASE"

expect_output_with_headers \
    "string literal labels" \
    "abc	xyz
abc	xyz" \
    "SELECT 'abc', ('xyz');" \
    "$DATABASE"

expect_output_with_headers \
    "binary literal projection" \
    "0x417a	X'417a'	x'417a'	b'0100000101111010'	B'0100000101111010'	0b0100000101111010	0b1	0b01	0b001	0b00000001	0b000000001
0x417A	0x417A	0x417A	0x417A	0x417A	0x417A	0x01	0x01	0x01	0x01	0x0001" \
    "SELECT 0x417a, X'417a', x'417a', b'0100000101111010', B'0100000101111010', 0b0100000101111010, 0b1, 0b01, 0b001, 0b00000001, 0b000000001;" \
    "$DATABASE"

expect_output_with_headers \
    "parenthesized top-level and nested values" \
    "1	NULL	(TRUE)	2	(-3)	(IF(1,4,5))	IFNULL((NULL),(6))	COALESCE((NULL),(7))	NULLIF((8),(8))	ISNULL((NULL))
1	NULL	1	2	-3	4	6	7	NULL	1" \
    "SELECT (1), (NULL), (TRUE), (+2), (-3), (IF(1,4,5)), IFNULL((NULL),(6)), COALESCE((NULL),(7)), NULLIF((8),(8)), ISNULL((NULL));" \
    "$DATABASE"

expect_output_with_headers \
    "parenthesized operands" \
    "IF((1),(2),(3))	IFNULL((NULL),(4))	COALESCE((NULL),(NULL),(5))	NULLIF((1),(1))	ISNULL((NULL))
2	4	5	NULL	1" \
    "SELECT IF((1),(2),(3)), IFNULL((NULL),(4)), COALESCE((NULL),(NULL),(5)), NULLIF((1),(1)), ISNULL((NULL));" \
    "$DATABASE"

expect_output \
    "dual row count and warning count" \
    "1	2	1
0	-1" \
    "DO 0; SELECT 1, IF(1,2,3), ISNULL(NULL) FROM DUAL; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "wrong IF arity in mixed projection" \
    1064 \
    42000 \
    "near '), 1'" \
    "SELECT IF(1,2), 1;" \
    "$DATABASE"

expect_error \
    "wrong ISNULL arity in mixed projection" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ISNULL'" \
    "SELECT 1, ISNULL();" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT IF(1,2,3), 1+2;
     SELECT (1+2), IF(1,2,3);
     SELECT IF(1,2,3) FROM t ORDER BY id IS NULL, id;
     SELECT 1, IF(1,2,3) WHERE TRUE;
     SELECT 1, IF(1,2,3) LIMIT 1;
     SELECT 1, IF(1,2,3) ORDER BY 1;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "IF(1,2,3)	1+2
2	3
(1+2)	IF(1,2,3)
3	2
IF(1,2,3)
2
2
2
1	IF(1,2,3)
1	2
1	IF(1,2,3)
1	2
1	IF(1,2,3)
1	2" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_scalar_expression_projection_expectations: ok"
