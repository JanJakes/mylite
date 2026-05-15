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
    "0x4142430000	ABC	0	ABC" \
    "SELECT CONVERT('ABC', BINARY(5)), CONVERT('ABC', CHAR), "\
"CONVERT('ABC', SIGNED), CONVERT('ABC' USING latin1);" \
    "$DATABASE"

expect_error \
    "too many convert binary type arguments" \
    1064 \
    "42000" \
    "You have an error in your SQL syntax" \
    "SELECT CONVERT('ABC', BINARY, 1);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_convert_syntax_expansion_expectations: ok"
