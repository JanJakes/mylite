#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_current_role_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_current_role_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

role_values=$(run_mysql "SELECT 1; SELECT CURRENT_ROLE(), @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value \
    "current role returns no active role and no warnings" \
    "NONE	0	-1" \
    "$role_values"

expected_lower_headers=$(cat <<EOF
current_role()
NONE
EOF
)
expect_output_with_headers \
    "lower-case current role label remains result label" \
    "$expected_lower_headers" \
    "SELECT current_role();"

expected_spaced_headers=$(cat <<EOF
CURRENT_ROLE ()
NONE
EOF
)
expect_output_with_headers \
    "spaced current role label remains result label" \
    "$expected_spaced_headers" \
    "SELECT CURRENT_ROLE ();"

expected_parenthesized_headers=$(cat <<EOF
(CURRENT_ROLE())
NONE
EOF
)
expect_output_with_headers \
    "parenthesized current role label remains result label" \
    "$expected_parenthesized_headers" \
    "SELECT (CURRENT_ROLE());"

expect_output \
    "from dual returns current role" \
    "NONE" \
    "SELECT CURRENT_ROLE() FROM DUAL;"

expect_output \
    "selected database does not change current role" \
    "NONE	${DATABASE}" \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; SELECT CURRENT_ROLE(), DATABASE();"

expect_error \
    "current role rejects integer argument" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT CURRENT_ROLE(1);"

expect_error \
    "current role rejects null argument" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT CURRENT_ROLE(NULL);"

expect_error \
    "current role rejects string argument" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT CURRENT_ROLE('x');"

expect_error \
    "current role rejects multiple arguments" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT CURRENT_ROLE(1, 2);"

expect_error \
    "bare current role is not a current role function" \
    1054 \
    42S22 \
    "Unknown column 'CURRENT_ROLE'" \
    "SELECT CURRENT_ROLE;"

expect_output \
    "mysql accepts limit outside this mylite slice" \
    "NONE" \
    "SELECT CURRENT_ROLE() LIMIT 1;"

expect_output \
    "mysql accepts expression outside this mylite slice" \
    "1" \
    "SELECT CURRENT_ROLE() + 1;"

cleanup

run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE current_role (current_role INT); DROP DATABASE ${DATABASE};" >/dev/null
