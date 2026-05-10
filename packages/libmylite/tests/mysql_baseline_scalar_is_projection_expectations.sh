#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_scalar_is_projection_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_scalar_is_projection_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t(id INT, nullable_int INT NULL); INSERT INTO t VALUES (1, NULL), (0, 0), (-2, 5);" >/dev/null

expect_output_with_headers \
    "is true truth table" \
    "one_true	zero_true	neg_true	null_true	true_true	false_true
1	0	1	0	1	0" \
    "SELECT 1 IS TRUE AS one_true, 0 IS TRUE AS zero_true, -1 IS TRUE AS neg_true, NULL IS TRUE AS null_true, TRUE IS TRUE AS true_true, FALSE IS TRUE AS false_true;" \
    "$DATABASE"

expect_output_with_headers \
    "is false truth table" \
    "one_false	zero_false	neg_false	null_false	true_false	false_false
0	1	0	0	0	1" \
    "SELECT 1 IS FALSE AS one_false, 0 IS FALSE AS zero_false, -1 IS FALSE AS neg_false, NULL IS FALSE AS null_false, TRUE IS FALSE AS true_false, FALSE IS FALSE AS false_false;" \
    "$DATABASE"

expect_output_with_headers \
    "is unknown truth table" \
    "one_unknown	zero_unknown	neg_unknown	null_unknown	true_unknown	false_unknown
0	0	0	1	0	0" \
    "SELECT 1 IS UNKNOWN AS one_unknown, 0 IS UNKNOWN AS zero_unknown, -1 IS UNKNOWN AS neg_unknown, NULL IS UNKNOWN AS null_unknown, TRUE IS UNKNOWN AS true_unknown, FALSE IS UNKNOWN AS false_unknown;" \
    "$DATABASE"

expect_output_with_headers \
    "is not true truth table" \
    "one_not_true	zero_not_true	neg_not_true	null_not_true	true_not_true	false_not_true
0	1	0	1	0	1" \
    "SELECT 1 IS NOT TRUE AS one_not_true, 0 IS NOT TRUE AS zero_not_true, -1 IS NOT TRUE AS neg_not_true, NULL IS NOT TRUE AS null_not_true, TRUE IS NOT TRUE AS true_not_true, FALSE IS NOT TRUE AS false_not_true;" \
    "$DATABASE"

expect_output_with_headers \
    "is not false truth table" \
    "one_not_false	zero_not_false	neg_not_false	null_not_false	true_not_false	false_not_false
1	0	1	1	1	0" \
    "SELECT 1 IS NOT FALSE AS one_not_false, 0 IS NOT FALSE AS zero_not_false, -1 IS NOT FALSE AS neg_not_false, NULL IS NOT FALSE AS null_not_false, TRUE IS NOT FALSE AS true_not_false, FALSE IS NOT FALSE AS false_not_false;" \
    "$DATABASE"

expect_output_with_headers \
    "is not unknown truth table" \
    "one_not_unknown	zero_not_unknown	neg_not_unknown	null_not_unknown	true_not_unknown	false_not_unknown
1	1	1	0	1	1" \
    "SELECT 1 IS NOT UNKNOWN AS one_not_unknown, 0 IS NOT UNKNOWN AS zero_not_unknown, -1 IS NOT UNKNOWN AS neg_not_unknown, NULL IS NOT UNKNOWN AS null_not_unknown, TRUE IS NOT UNKNOWN AS true_not_unknown, FALSE IS NOT UNKNOWN AS false_not_unknown;" \
    "$DATABASE"

expect_output_with_headers \
    "is null truth table" \
    "one_null	zero_null	neg_null	null_null	true_null	false_null
0	0	0	1	0	0" \
    "SELECT 1 IS NULL AS one_null, 0 IS NULL AS zero_null, -1 IS NULL AS neg_null, NULL IS NULL AS null_null, TRUE IS NULL AS true_null, FALSE IS NULL AS false_null;" \
    "$DATABASE"

expect_output_with_headers \
    "is not null truth table" \
    "one_not_null	zero_not_null	neg_not_null	null_not_null	true_not_null	false_not_null
1	1	1	0	1	1" \
    "SELECT 1 IS NOT NULL AS one_not_null, 0 IS NOT NULL AS zero_not_null, -1 IS NOT NULL AS neg_not_null, NULL IS NOT NULL AS null_not_null, TRUE IS NOT NULL AS true_not_null, FALSE IS NOT NULL AS false_not_null;" \
    "$DATABASE"

expect_output_with_headers \
    "arithmetic and scalar function operands" \
    "1+2 IS TRUE	0*3 IS FALSE	5 DIV 2 IS TRUE	5 % 2 IS TRUE	a	b	c	d
1	1	1	1	1	1	1	1" \
    "SELECT 1+2 IS TRUE, 0*3 IS FALSE, 5 DIV 2 IS TRUE, 5 % 2 IS TRUE, IFNULL(NULL,1) IS TRUE AS a, NULLIF(1,1) IS UNKNOWN AS b, ISNULL(NULL) IS TRUE AS c, COALESCE(NULL,0) IS FALSE AS d;" \
    "$DATABASE"

expect_output_with_headers \
    "comparison logical and nested operands" \
    "(1=1) IS TRUE	(1=NULL) IS UNKNOWN	(NULL<=>NULL) IS TRUE	(1 AND 0) IS FALSE	(1 OR NULL) IS TRUE	(NULL XOR 1) IS UNKNOWN	(1 IS TRUE) IS TRUE	(1 IS NULL) IS FALSE	(1 IS TRUE)=1	1=(1 IS TRUE)
1	1	1	1	1	1	1	1	1	1" \
    "SELECT (1=1) IS TRUE, (1=NULL) IS UNKNOWN, (NULL<=>NULL) IS TRUE, (1 AND 0) IS FALSE, (1 OR NULL) IS TRUE, (NULL XOR 1) IS UNKNOWN, (1 IS TRUE) IS TRUE, (1 IS NULL) IS FALSE, (1 IS TRUE)=1, 1=(1 IS TRUE);" \
    "$DATABASE"

expect_output_with_headers \
    "precedence" \
    "NOT 1 IS TRUE	NOT (1 IS TRUE)	1 IS TRUE AND 0	1 AND 0 IS FALSE	1 IS TRUE XOR 0	0 OR NULL IS UNKNOWN
0	0	0	1	1	1" \
    "SELECT NOT 1 IS TRUE, NOT (1 IS TRUE), 1 IS TRUE AND 0, 1 AND 0 IS FALSE, 1 IS TRUE XOR 0, 0 OR NULL IS UNKNOWN;" \
    "$DATABASE"

expect_output_with_headers \
    "child warnings" \
    "5 DIV 0 IS NULL	5 DIV 0 IS UNKNOWN	5 DIV 0 IS TRUE	5 DIV 0 IS FALSE	@@warning_count	ROW_COUNT()
1	1	0	0	0	0
Level	Code	Message
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
@@warning_count	ROW_COUNT()
4	-1" \
    "DO 0; SELECT 5 DIV 0 IS NULL, 5 DIV 0 IS UNKNOWN, 5 DIV 0 IS TRUE, 5 DIV 0 IS FALSE, @@warning_count, ROW_COUNT(); SHOW WARNINGS; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "logical child short circuit" \
    "(0 AND 5 DIV 0) IS FALSE	(1 OR 5 DIV 0) IS TRUE	(NULL XOR 5 DIV 0) IS UNKNOWN	@@warning_count	ROW_COUNT()
1	1	1	0	0
@@warning_count	ROW_COUNT()
0	-1" \
    "DO 0; SELECT (0 AND 5 DIV 0) IS FALSE, (1 OR 5 DIV 0) IS TRUE, (NULL XOR 5 DIV 0) IS UNKNOWN, @@warning_count, ROW_COUNT(); SHOW WARNINGS; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "signed boundary operands" \
    "9223372036854775807 IS TRUE	-9223372036854775807 IS TRUE	(-9223372036854775807-1) IS TRUE	0 IS TRUE	0 IS FALSE	NULL IS NOT TRUE
1	1	1	0	1	1" \
    "SELECT 9223372036854775807 IS TRUE, -9223372036854775807 IS TRUE, (-9223372036854775807-1) IS TRUE, 0 IS TRUE, 0 IS FALSE, NULL IS NOT TRUE;" \
    "$DATABASE"

expect_error \
    "child overflow under is" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "SELECT 3037000500*3037000500 IS TRUE;" \
    "$DATABASE"

expect_error \
    "is missing right operand" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT 1 IS;" \
    "$DATABASE"

expect_error \
    "is not missing right operand" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT 1 IS NOT;" \
    "$DATABASE"

expect_error \
    "is unsupported right operand" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT 1 IS 1;" \
    "$DATABASE"

expect_error \
    "is missing left operand" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT IS TRUE;" \
    "$DATABASE"

expect_error \
    "is true chained directly" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT 1 IS TRUE IS TRUE;" \
    "$DATABASE"

expect_error \
    "is true compared directly" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT 1 IS TRUE = 1;" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT 1 IS NULL IS TRUE, 1 IS NOT NULL IS TRUE, 1 IS NULL = 0, 'a' IS TRUE, '1' IS TRUE, 1.5 IS TRUE, 0x31 IS TRUE, b'1' IS TRUE, 1 + (0 IS FALSE), (1 IS TRUE) + 1;
     SELECT id IS TRUE AS id_true, nullable_int IS UNKNOWN AS n_unknown FROM t ORDER BY id;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "1 IS NULL IS TRUE	1 IS NOT NULL IS TRUE	1 IS NULL = 0	'a' IS TRUE	'1' IS TRUE	1.5 IS TRUE	0x31 IS TRUE	b'1' IS TRUE	1 + (0 IS FALSE)	(1 IS TRUE) + 1
0	1	1	0	1	1	1	1	2	2
id_true	n_unknown
1	0
0	0
1	1" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_scalar_is_projection_expectations: ok"
