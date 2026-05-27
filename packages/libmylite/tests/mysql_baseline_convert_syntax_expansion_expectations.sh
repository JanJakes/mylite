#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_convert_syntax_expansion_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_convert_syntax_expansion_expectations: $1" >&2
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

binary_expected=$(cat <<\EXPECTED
0x414243	0x	NULL	0x313233	0x2D37	0x31	0x30
-1	0
EXPECTED
)
expect_output \
    "convert binary type values" \
    "$binary_expected" \
    "DO 0; SELECT CONVERT('ABC', BINARY), CONVERT('', BINARY), "\
"CONVERT(NULL, BINARY), CONVERT(123, BINARY), CONVERT(-7, BINARY), "\
"CONVERT(TRUE, BINARY), CONVERT(FALSE, BINARY); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

utf8mb4_expected=$(cat <<\EXPECTED
ABC	é	NULL	123	-7	1	0
-1	0
EXPECTED
)
expect_output \
    "convert using utf8mb4 values" \
    "$utf8mb4_expected" \
    "SELECT CONVERT('ABC' USING utf8mb4), CONVERT('é' USING utf8mb4), "\
"CONVERT(NULL USING utf8mb4), CONVERT(123 USING utf8mb4), "\
"CONVERT(-7 USING utf8mb4), CONVERT(TRUE USING utf8mb4), "\
"CONVERT(FALSE USING utf8mb4); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

convert_charset_expected=$(cat <<\EXPECTED
ABC	ABC	ABC	ABC	123	NULL
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	1287	'utf8mb3' is deprecated and will be removed in a future release. Please use utf8mb4 instead
-1	2
EXPECTED
)
expect_output \
    "convert using scalar charsets and warnings" \
    "$convert_charset_expected" \
    "SELECT CONVERT('ABC' USING utf8), CONVERT('ABC' USING utf8mb3), "\
"CONVERT('ABC' USING latin1), CONVERT('ABC' USING 'utf8mb4'), "\
"CONVERT(123 USING 'latin1'), CONVERT(NULL USING latin1); "\
"SHOW WARNINGS; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

convert_collate_expected=$(cat <<\EXPECTED
ABC	ABC	ABC
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
-1	1
EXPECTED
)
expect_output \
    "convert using scalar collate" \
    "$convert_collate_expected" \
    "SELECT CONVERT('ABC' USING utf8mb4) COLLATE utf8mb4_bin, "\
"CONVERT('ABC' USING utf8) COLLATE utf8mb3_bin, "\
"CONVERT('ABC' USING latin1) COLLATE 'latin1_bin'; "\
"SHOW WARNINGS; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

integer_boundary_expected=$(cat <<\EXPECTED
0x313233343536373839303132333435363738393031323334353637383930313233343536373839303132333435363738393031323334353637383930313233343536373839303132333435363738393031	123456789012345678901234567890123456789012345678901234567890123456789012345678901
-1	0
EXPECTED
)
expect_output \
    "integer boundary and mixed-case charset" \
    "$integer_boundary_expected" \
    "SELECT "\
"CONVERT(123456789012345678901234567890123456789012345678901234567890123456789012345678901, BINARY), "\
"CONVERT(123456789012345678901234567890123456789012345678901234567890123456789012345678901 "\
"USING UTF8MB4); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<\EXPECTED
binary_value	(CONVERT('x', BINARY))	converted	(CONVERT('y' USING utf8mb4))
0x414243	0x78	ABC	y
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT CONVERT('ABC', BINARY) AS binary_value, (CONVERT('x', BINARY)), "\
"CONVERT('ABC' USING utf8mb4) AS converted, (CONVERT('y' USING utf8mb4));" \
    "$DATABASE"

expect_output \
    "from dual whitespace values" \
    "0x41	B" \
    "SELECT CONVERT ('A', BINARY), CONVERT ('B' USING utf8mb4) FROM DUAL;" \
    "$DATABASE"

expect_output \
    "do row count" \
    "0	0" \
    "DO CONVERT('ABC', BINARY), CONVERT(NULL, BINARY), "\
"CONVERT('ABC' USING utf8mb4); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "mysql accepted deferred broader convert forms" \
    "0x4142430000	ABC	0" \
    "SELECT CONVERT('ABC', BINARY(5)), CONVERT('ABC', CHAR), "\
"CONVERT('ABC', SIGNED);" \
    "$DATABASE"

expect_error \
    "too many convert binary type arguments" \
    1064 \
    "42000" \
    "You have an error in your SQL syntax" \
    "SELECT CONVERT('ABC', BINARY, 1);" \
    "$DATABASE"

expect_error \
    "unknown convert using charset" \
    1115 \
    42000 \
    "Unknown character set: 'nosuch_charset'" \
    "SELECT CONVERT('ABC' USING nosuch_charset);" \
    "$DATABASE"

expect_error \
    "unknown convert collate" \
    1273 \
    HY000 \
    "Unknown collation: 'nosuch_collation'" \
    "SELECT CONVERT('ABC' USING utf8mb4) COLLATE nosuch_collation;" \
    "$DATABASE"

expect_error \
    "convert collate rejects mismatched charset" \
    1253 \
    42000 \
    "COLLATION 'latin1_swedish_ci' is not valid for CHARACTER SET 'utf8mb4'" \
    "SELECT CONVERT('ABC' USING utf8mb4) COLLATE latin1_swedish_ci;" \
    "$DATABASE"

expect_error \
    "collate rejects null binary charset" \
    1253 \
    42000 \
    "COLLATION 'utf8mb4_bin' is not valid for CHARACTER SET 'binary'" \
    "SELECT NULL COLLATE utf8mb4_bin;" \
    "$DATABASE"

expect_error \
    "collate rejects binary literal charset" \
    1253 \
    42000 \
    "COLLATION 'utf8mb4_bin' is not valid for CHARACTER SET 'binary'" \
    "SELECT X'41' COLLATE utf8mb4_bin;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_convert_syntax_expansion_expectations: ok"
