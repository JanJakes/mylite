#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_seeded_rand_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_seeded_rand_function_expectations: $1" >&2
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

expect_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept behavior, got [$output]"
    fi
}

expect_header_and_output() {
    label=$1
    expected_header=$2
    expected_output=$3
    sql=$4
    shift 4

    output=$(run_mysql_with_headers "$sql" "$@")
    header=$(printf '%s\n' "$output" | sed -n '1p')
    values=$(printf '%s\n' "$output" | sed -n '2p')

    if [ "$header" != "$expected_header" ]; then
        fail "$label: expected header [$expected_header], got [$header]"
    fi
    if [ "$values" != "$expected_output" ]; then
        fail "$label: expected values [$expected_output], got [$values]"
    fi
}

expect_mixed_seeded_unseeded() {
    label=$1
    sql=$2
    shift 2

    output=$(run_mysql "$sql" "$@")
    printf '%s\n' "$output" | awk -F '\t' '
        NF != 5 { exit 1 }
        $1 != "0.40540353712197724" { exit 1 }
        $3 != "0.40540353712197724" { exit 1 }
        $4 != "0" { exit 1 }
        $5 != "0" { exit 1 }
        $2 !~ /^[0-9]+([.][0-9]+)?$/ { exit 1 }
        {
            value = $2 + 0
            if (!(value >= 0 && value < 1)) {
                exit 1
            }
        }
    ' || fail "$label: expected seeded exact values around one unseeded value, got [$output]"
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

expect_output \
    "seeded RAND exact values" \
    "0.15522042769493574	0.40540353712197724	0.6555866465490187	0.9057697559760601	0.15522042769493574	0.40540353712197724	0.15522042769493574	0.9050373219931845" \
    "SELECT RAND(0), RAND(1), RAND(2), RAND(3), RAND(NULL), RAND(TRUE), RAND(FALSE), RAND(-1);" \
    "$DATABASE"

expect_output \
    "seeded RAND status" \
    "0.15522042769493574	0.40540353712197724	0	0" \
    "DO 0; SELECT RAND(0), RAND(1), @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "seeded RAND uint32 wrapping" \
    "0.9050373219931845	0.15522042769493574	0.40540353712197724	0.9050373219931845" \
    "SELECT RAND(4294967295), RAND(4294967296), RAND(4294967297), RAND(18446744073709551615);" \
    "$DATABASE"

expect_header_and_output \
    "seeded RAND labels and DUAL" \
    "RAND (1)	r" \
    "0.40540353712197724	0.15522042769493574" \
    "SELECT RAND (1), RAND(NULL) AS r FROM DUAL;" \
    "$DATABASE"

expect_mixed_seeded_unseeded \
    "mixed seeded and unseeded RAND" \
    "DO 0; SELECT RAND(1), RAND(), RAND(1), @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "seeded RAND DO status" \
    "0	0" \
    "DO RAND(1), RAND(NULL), RAND(-1); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_accepts \
    "table-backed seeded RAND accepted by MySQL" \
    "DROP TABLE IF EXISTS t; CREATE TABLE t(id INT); INSERT INTO t VALUES (1), (2), (3); "\
"SELECT id, RAND(1) FROM t ORDER BY id;" \
    "$DATABASE"

expect_error \
    "seeded RAND too many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'RAND'" \
    "SELECT RAND(1, 2);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_seeded_rand_function_expectations: ok"
