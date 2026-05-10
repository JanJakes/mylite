#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_case_operator_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_case_operator_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t(id INT, n INT NULL); INSERT INTO t VALUES (1, NULL), (0, 5), (-1, 7);" >/dev/null

expect_output_with_headers \
    "searched case truth table" \
    "searched_true	searched_false	searched_null	searched_negative	no_match_no_else
2	3	3	2	NULL" \
    "SELECT CASE WHEN 1 THEN 2 ELSE 3 END AS searched_true,
            CASE WHEN 0 THEN 2 ELSE 3 END AS searched_false,
            CASE WHEN NULL THEN 2 ELSE 3 END AS searched_null,
            CASE WHEN -1 THEN 2 END AS searched_negative,
            CASE WHEN 0 THEN 2 END AS no_match_no_else;" \
    "$DATABASE"

expect_output_with_headers \
    "simple case matches" \
    "simple_first	simple_second	simple_else	simple_null_equal	simple_when_null
10	20	30	30	30" \
    "SELECT CASE 1 WHEN 1 THEN 10 WHEN 2 THEN 20 ELSE 30 END AS simple_first,
            CASE 2 WHEN 1 THEN 10 WHEN 2 THEN 20 ELSE 30 END AS simple_second,
            CASE 3 WHEN 1 THEN 10 WHEN 2 THEN 20 ELSE 30 END AS simple_else,
            CASE NULL WHEN NULL THEN 10 ELSE 30 END AS simple_null_equal,
            CASE 1 WHEN NULL THEN 10 ELSE 30 END AS simple_when_null;" \
    "$DATABASE"

expect_output_with_headers \
    "result values and labels" \
    "bool_result	null_int_result	normalized	signed_min
1	-5	2	-9223372036854775808" \
    "SELECT CASE WHEN 1 THEN TRUE ELSE FALSE END AS bool_result,
            CASE WHEN 0 THEN NULL ELSE -5 END AS null_int_result,
            CASE 1 WHEN 1 THEN 0002 ELSE 0003 END AS normalized,
            CASE WHEN 1 THEN (-9223372036854775807-1) ELSE 0 END AS signed_min;" \
    "$DATABASE"

expect_output_with_headers \
    "scalar expression operands" \
    "arithmetic_over_case	is_over_case	comparison_condition	is_condition	logical_condition
5	1	2	2	4" \
    "SELECT CASE WHEN 1 THEN 2 ELSE 3 END + 3 AS arithmetic_over_case,
            (CASE WHEN 1 THEN 2 END) IS TRUE AS is_over_case,
            CASE WHEN 1=1 THEN 2 ELSE 3 END AS comparison_condition,
            CASE WHEN 1 IS TRUE THEN 2 ELSE 3 END AS is_condition,
            CASE WHEN 1 AND NOT 0 THEN 4 ELSE 5 END AS logical_condition;" \
    "$DATABASE"

expect_output_with_headers \
    "nested case and labels" \
    "nested_searched	nested_simple	nested_condition	CASE WHEN 1 THEN 2 ELSE 3 END	chosen	parenthesized
3	4	6	2	2	2" \
    "SELECT CASE WHEN 1 THEN CASE WHEN 0 THEN 2 ELSE 3 END ELSE 4 END AS nested_searched,
            CASE CASE WHEN 1 THEN 2 ELSE 3 END WHEN 2 THEN 4 ELSE 5 END AS nested_simple,
            CASE WHEN CASE WHEN 1 THEN 1 ELSE 0 END THEN 6 ELSE 7 END AS nested_condition,
            CASE WHEN 1 THEN 2 ELSE 3 END,
            CASE WHEN 1 THEN 2 ELSE 3 END chosen,
            (CASE WHEN 1 THEN 2 ELSE 3 END) AS parenthesized;" \
    "$DATABASE"

expect_output_with_headers \
    "dual diagnostics" \
    "CASE WHEN 1 THEN 2 ELSE 3 END
2
@@warning_count	ROW_COUNT()
0	-1" \
    "DO 0; SELECT CASE WHEN 1 THEN 2 ELSE 3 END FROM DUAL; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "branch short circuit" \
    "result_short_true	result_short_false	condition_short_true	simple_when_short_true
2	2	2	2
@@warning_count	ROW_COUNT()
0	-1" \
    "DO 0;
     SELECT CASE WHEN 1 THEN 2 ELSE 5 DIV 0 END AS result_short_true,
            CASE WHEN 0 THEN 5 DIV 0 ELSE 2 END AS result_short_false,
            CASE WHEN 1 THEN 2 WHEN 5 DIV 0 THEN 3 ELSE 4 END AS condition_short_true,
            CASE 1 WHEN 1 THEN 2 WHEN 5 DIV 0 THEN 3 ELSE 4 END AS simple_when_short_true;
     SHOW WARNINGS;
     SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "evaluated warning expressions" \
    "condition_warning	simple_case_warning	evaluated_second_condition	evaluated_simple_compare
2	3	3	3
Level	Code	Message
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
Warning	1365	Division by 0
@@warning_count	ROW_COUNT()
4	-1" \
    "DO 0;
     SELECT CASE WHEN 5 DIV 0 THEN 1 ELSE 2 END AS condition_warning,
            CASE 5 DIV 0 WHEN 1 THEN 1 WHEN 2 THEN 2 ELSE 3 END AS simple_case_warning,
            CASE WHEN 0 THEN 1 WHEN 5 DIV 0 THEN 2 ELSE 3 END AS evaluated_second_condition,
            CASE 2 WHEN 1 THEN 1 WHEN 5 DIV 0 THEN 2 ELSE 3 END AS evaluated_simple_compare;
     SHOW WARNINGS;
     SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT CASE WHEN 'x' THEN 2 ELSE 3 END AS string_condition,
            CASE WHEN 1 THEN 'a' ELSE 'b' END AS string_result,
            CASE WHEN 1.5 THEN 2 ELSE 3 END AS decimal_condition,
            CASE 0x31 WHEN 49 THEN 2 ELSE 3 END AS hex_simple,
            CASE b'1' WHEN 1 THEN 2 ELSE 3 END AS bit_simple;
     SELECT CASE WHEN id THEN 2 ELSE 3 END AS table_condition FROM t ORDER BY id DESC;
     SELECT CASE id WHEN 1 THEN 10 WHEN 0 THEN 20 ELSE 30 END AS table_simple FROM t ORDER BY id DESC;
     SELECT CASE WHEN 1 THEN 2 ELSE 3 END LIMIT 1;
     SELECT CASE WHEN 1 THEN 2 ELSE 3 END ORDER BY 1;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "string_condition	string_result	decimal_condition	hex_simple	bit_simple
3	a	2	2	2
table_condition
2
3
2
table_simple
10
20
30
CASE WHEN 1 THEN 2 ELSE 3 END
2
CASE WHEN 1 THEN 2 ELSE 3 END
2" \
    "$accepted_but_deferred"

expect_error \
    "empty case" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT CASE END;" \
    "$DATABASE"

expect_error \
    "searched case missing condition" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT CASE WHEN THEN 1 END;" \
    "$DATABASE"

expect_error \
    "searched case missing then" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT CASE WHEN 1 END;" \
    "$DATABASE"

expect_error \
    "simple case missing when list" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT CASE 1 ELSE 2 END;" \
    "$DATABASE"

expect_error \
    "end case expression terminator" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT CASE WHEN 1 THEN 2 END CASE;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_case_operator_expectations: ok"
