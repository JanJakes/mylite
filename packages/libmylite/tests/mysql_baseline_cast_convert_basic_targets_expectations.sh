#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_cast_convert_basic_targets_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_cast_convert_basic_targets_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw \
            --binary-as-hex=1 --default-character-set=utf8mb4 \
            --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw \
            --binary-as-hex=1 --default-character-set=utf8mb4 "$@"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; USE ${DATABASE}; SET NAMES utf8mb4;" >/dev/null

char_expected=$(cat <<\EXPECTED
ABC	ABC	123	-7	NULL	1	0
-1	0
EXPECTED
)
expect_output \
    "char target values" \
    "$char_expected" \
    "SELECT CAST('ABC' AS CHAR), CONVERT('ABC', CHAR), CAST(123 AS CHAR), "\
"CONVERT(-7, CHAR), CAST(NULL AS CHAR), CAST(TRUE AS CHAR), "\
"CONVERT(FALSE, CHAR); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

integer_expected=$(cat <<\EXPECTED
0	0	123	-12	NULL	1	0
Warning	1292	Truncated incorrect INTEGER value: 'ABC'
Warning	1292	Truncated incorrect INTEGER value: 'ABC'
Warning	1292	Truncated incorrect INTEGER value: '123abc'
Warning	1292	Truncated incorrect INTEGER value: '  -12x'
EXPECTED
)
expect_output \
    "signed target values and warnings" \
    "$integer_expected" \
    "SELECT CAST('ABC' AS SIGNED), CONVERT('ABC', SIGNED), "\
"CAST('123abc' AS SIGNED), CAST('  -12x' AS SIGNED), "\
"CAST(NULL AS SIGNED), CAST(TRUE AS SIGNED), CAST(FALSE AS SIGNED); "\
"SHOW WARNINGS;" \
    "$DATABASE"

unsigned_expected=$(cat <<\EXPECTED
0	0	18446744073709551615	18446744073709551615	18446744073709551615	18446744073709551615
Warning	1292	Truncated incorrect INTEGER value: 'ABC'
Warning	1292	Truncated incorrect INTEGER value: 'ABC'
Warning	1105	Cast to unsigned converted negative integer to its positive complement
Warning	1292	Truncated incorrect INTEGER value: '18446744073709551616'
EXPECTED
)
expect_output \
    "unsigned target values and warnings" \
    "$unsigned_expected" \
    "SELECT CAST('ABC' AS UNSIGNED), CONVERT('ABC', UNSIGNED), "\
"CAST('-1' AS UNSIGNED), CAST(-1 AS UNSIGNED), "\
"CAST('18446744073709551615' AS UNSIGNED), "\
"CAST('18446744073709551616' AS UNSIGNED); SHOW WARNINGS;" \
    "$DATABASE"

target_synonyms_expected=$(cat <<\EXPECTED
1	1	1	1
EXPECTED
)
expect_output \
    "target synonyms" \
    "$target_synonyms_expected" \
    "SELECT CAST('1' AS SIGNED INTEGER), CAST('1' AS UNSIGNED INTEGER), "\
"CONVERT('1', SIGNED INT), CONVERT('1', UNSIGNED INT);" \
    "$DATABASE"

space_and_malformed_expected=$(cat <<\EXPECTED
123	123	12	0	0	0	0
Warning	1292	Truncated incorrect INTEGER value: ' 123x '
Warning	1292	Truncated incorrect INTEGER value: '+-12'
Warning	1292	Truncated incorrect INTEGER value: ' - 12'
Warning	1292	Truncated incorrect INTEGER value: ''
Warning	1292	Truncated incorrect INTEGER value: ' '
EXPECTED
)
expect_output \
    "spaces and malformed strings and warnings" \
    "$space_and_malformed_expected" \
    "SELECT CAST(' 123 ' AS SIGNED), CAST(' 123x ' AS SIGNED), "\
"CAST('+12' AS SIGNED), CAST('+-12' AS SIGNED), "\
"CAST(' - 12' AS SIGNED), CAST('' AS SIGNED), CAST(' ' AS SIGNED); "\
"SHOW WARNINGS;" \
    "$DATABASE"

signed_boundary_expected=$(cat <<\EXPECTED
9223372036854775807	-9223372036854775808	-9223372036854775808	-9223372036854775808
Warning	1105	Cast to signed converted positive out-of-range integer to its negative complement
Warning	1292	Truncated incorrect INTEGER value: '-9223372036854775809'
EXPECTED
)
expect_output \
    "signed string boundaries and warnings" \
    "$signed_boundary_expected" \
    "SELECT CAST('9223372036854775807' AS SIGNED), "\
"CAST('9223372036854775808' AS SIGNED), "\
"CAST('-9223372036854775808' AS SIGNED), "\
"CAST('-9223372036854775809' AS SIGNED); SHOW WARNINGS;" \
    "$DATABASE"

decimal_boundary_expected=$(cat <<\EXPECTED
9223372036854775807	-9223372036854775808	-1	9223372036854775807
Warning	1292	Truncated incorrect DECIMAL value: '18446744073709551616'
EXPECTED
)
expect_output \
    "signed decimal literal boundaries and warnings" \
    "$decimal_boundary_expected" \
    "SELECT CAST(9223372036854775807 AS SIGNED), "\
"CAST(9223372036854775808 AS SIGNED), "\
"CAST(18446744073709551615 AS SIGNED), "\
"CAST(18446744073709551616 AS SIGNED); SHOW WARNINGS;" \
    "$DATABASE"

unsigned_negative_boundary_expected=$(cat <<\EXPECTED
9223372036854775808	9223372036854775808	9223372036854775808
Warning	1292	Truncated incorrect DECIMAL value: '-9223372036854775809'
Warning	1292	Truncated incorrect INTEGER value: '-9223372036854775809'
EXPECTED
)
expect_output \
    "unsigned negative boundaries and warnings" \
    "$unsigned_negative_boundary_expected" \
    "SELECT CAST(-9223372036854775808 AS UNSIGNED), "\
"CAST(-9223372036854775809 AS UNSIGNED), "\
"CAST('-9223372036854775809' AS UNSIGNED); SHOW WARNINGS;" \
    "$DATABASE"

labels_expected=$(cat <<\EXPECTED
CAST('1' AS SIGNED)	signed_value	(CAST('2' AS UNSIGNED))	CAST(123 AS CHAR)	CONVERT('3', SIGNED)	CONVERT('4', UNSIGNED INTEGER)	CONVERT('ABC', CHAR)
1	1	2	123	3	4	ABC
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT CAST('1' AS SIGNED), CAST('1' AS SIGNED) AS signed_value, "\
"(CAST('2' AS UNSIGNED)), CAST(123 AS CHAR), CONVERT('3', SIGNED), "\
"CONVERT('4', UNSIGNED INTEGER), CONVERT('ABC', CHAR);" \
    "$DATABASE"

expect_output \
    "from dual values" \
    "9	9	x" \
    "SELECT CAST('9' AS SIGNED), CONVERT('9', UNSIGNED), CAST('x' AS CHAR) FROM DUAL;" \
    "$DATABASE"

expect_output \
    "do result state" \
    "0	2" \
    "DO CONVERT('ABC', SIGNED), CONVERT('-1', UNSIGNED), CAST(NULL AS CHAR); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "cast bare int target" \
    1064 \
    "42000" \
    "You have an error in your SQL syntax" \
    "SELECT CAST('1' AS INT);" \
    "$DATABASE"

expect_error \
    "convert bare integer target" \
    1064 \
    "42000" \
    "You have an error in your SQL syntax" \
    "SELECT CONVERT('1', INTEGER);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_cast_convert_basic_targets_expectations: ok"
