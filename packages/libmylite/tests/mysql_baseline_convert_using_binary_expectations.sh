#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_convert_using_binary_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_convert_using_binary_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw "$@"
}

run_mysql_binary_hex() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 --skip-column-names "$@"
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

expect_binary_hex_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_binary_hex "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

scalar_expected=$(cat <<EXPECTED
ABC	0
-1	0
EXPECTED
)
expect_output \
    "convert using binary scalar value" \
    "$scalar_expected" \
    "DO 0; SELECT CONVERT('ABC' USING BINARY) AS \`binary\`, @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
CONVERT('ABC' USING BINARY)	binary	(CONVERT('x' USING BINARY))
ABC	ABC	x
EXPECTED
)
expect_output_with_headers \
    "convert using binary labels" \
    "$labels_expected" \
    "SELECT CONVERT('ABC' USING BINARY), CONVERT('ABC' USING BINARY) AS \`binary\`, "\
"(CONVERT('x' USING BINARY));" \
    "$DATABASE"

expect_output \
    "convert using binary from dual" \
    "ABC" \
    "SELECT CONVERT('ABC' USING BINARY) FROM DUAL;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "convert using binary do status" \
    "$do_expected" \
    "DO CONVERT('ABC' USING BINARY); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_binary_hex_output \
    "convert using binary hex client display" \
    "0x414243	414243	binary	binary" \
    "SELECT CONVERT('ABC' USING BINARY), HEX(CONVERT('ABC' USING BINARY)), "\
"CHARSET(CONVERT('ABC' USING BINARY)), COLLATION(CONVERT('ABC' USING BINARY));" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_convert_using_binary_expectations: ok"
