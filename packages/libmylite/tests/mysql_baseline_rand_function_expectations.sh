#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_rand_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_rand_function_expectations: $1" >&2
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

expect_rand_status_row() {
    label=$1
    sql=$2
    shift 2

    output=$(run_mysql "$sql" "$@")
    printf '%s\n' "$output" | awk -F '\t' '
        NF != 4 { exit 1 }
        $1 !~ /^[0-9]+([.][0-9]+)?$/ { exit 1 }
        $2 !~ /^[0-9]+([.][0-9]+)?$/ { exit 1 }
        {
            left = $1 + 0
            right = $2 + 0
            if (!(left >= 0 && left < 1 && right >= 0 && right < 1)) {
                exit 1
            }
            if ($3 != "0" || $4 != "0") {
                exit 1
            }
        }
    ' || fail "$label: expected two RAND values in [0,1), warning 0, row_count 0; got [$output]"
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

expect_rand_header_and_range() {
    label=$1
    expected_header=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
    header=$(printf '%s\n' "$output" | sed -n '1p')
    values=$(printf '%s\n' "$output" | sed -n '2p')

    if [ "$header" != "$expected_header" ]; then
        fail "$label: expected header [$expected_header], got [$header]"
    fi
    printf '%s\n' "$values" | awk -F '\t' '
        NF < 1 { exit 1 }
        {
            for (i = 1; i <= NF; ++i) {
                if ($i !~ /^[0-9]+([.][0-9]+)?$/) {
                    exit 1
                }
                value = $i + 0
                if (!(value >= 0 && value < 1)) {
                    exit 1
                }
            }
        }
    ' || fail "$label: expected RAND values in [0,1), got [$values]"
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

expect_rand_status_row \
    "RAND values and status" \
    "DO 0; SELECT RAND(), RAND(), @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_rand_header_and_range \
    "RAND labels and DUAL" \
    "RAND ()	r" \
    "SELECT RAND (), RAND() AS r FROM DUAL;" \
    "$DATABASE"

expect_output \
    "RAND DO status" \
    "0	0" \
    "DO RAND(), RAND(); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "bare RAND identifier" \
    1054 \
    "42S22" \
    "Unknown column 'RAND' in 'field list'" \
    "SELECT RAND;" \
    "$DATABASE"

expect_accepts \
    "RAND identifier table" \
    "DROP TABLE IF EXISTS rand; CREATE TABLE rand (id INT); SELECT COUNT(*) FROM rand;" \
    "$DATABASE"

expect_accepts \
    "seeded RAND accepted by MySQL" \
    "SELECT RAND(3), RAND(NULL), RAND(TRUE), RAND(FALSE), RAND(-1);" \
    "$DATABASE"

expect_accepts \
    "table-backed RAND accepted by MySQL" \
    "DROP TABLE IF EXISTS t; CREATE TABLE t(id INT); INSERT INTO t VALUES (1), (2); "\
"SELECT id, RAND() FROM t ORDER BY id;" \
    "$DATABASE"

expect_error \
    "RAND too many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'RAND'" \
    "SELECT RAND(1, 2);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_rand_function_expectations: ok"
