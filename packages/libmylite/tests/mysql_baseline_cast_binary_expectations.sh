#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_cast_binary_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_cast_binary_expectations: $1" >&2
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
ABC		NULL	123	1	0	0
-1	0
EXPECTED
)
expect_output \
    "cast binary scalar values" \
    "$scalar_expected" \
    "DO 0; SELECT CAST('ABC' AS BINARY) AS \`binary\`, CAST('' AS BINARY), "\
"CAST(NULL AS BINARY), CAST(123 AS BINARY), CAST(TRUE AS BINARY), "\
"CAST(FALSE AS BINARY), @@warning_count; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
CAST('ABC' AS BINARY)	binary	(CAST('x' AS BINARY))
ABC	ABC	x
EXPECTED
)
expect_output_with_headers \
    "cast binary labels" \
    "$labels_expected" \
    "SELECT CAST('ABC' AS BINARY), CAST('ABC' AS BINARY) AS \`binary\`, "\
"(CAST('x' AS BINARY));" \
    "$DATABASE"

expect_output \
    "cast binary from dual" \
    "ABC" \
    "SELECT CAST('ABC' AS BINARY) FROM DUAL;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "cast binary do status" \
    "$do_expected" \
    "DO CAST('ABC' AS BINARY), CAST(NULL AS BINARY); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_binary_hex_output \
    "cast binary hex client display" \
    "0x414243	414243	binary	binary" \
    "SELECT CAST('ABC' AS BINARY), HEX(CAST('ABC' AS BINARY)), "\
"CHARSET(CAST('ABC' AS BINARY)), COLLATION(CAST('ABC' AS BINARY));" \
    "$DATABASE"

accepted_deferred_expected=$(cat <<EXPECTED
0x4142430000	0x4142	ABC	0x414243	0x414243	0	0
EXPECTED
)
expect_binary_hex_output \
    "mysql accepted forms deferred by MyLite" \
    "$accepted_deferred_expected" \
    "SELECT CAST('ABC' AS BINARY(5)), CAST('ABC' AS BINARY(2)), "\
"CAST('ABC' AS CHAR), CONVERT('ABC' USING BINARY), BINARY 'ABC', "\
"CAST('a' AS BINARY) = 'A', CAST('a' AS BINARY) = 'a ';" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_cast_binary_expectations: ok"
