#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_version_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_version_function_expectations: $1" >&2
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

expect_output \
    "version function returns value and no warnings" \
    "${version}	0" \
    "DO 0; SELECT VERSION(), @@warning_count;"

expected_lower_headers=$(cat <<EOF
version()
${version}
EOF
)
expect_output_with_headers \
    "lower-case version label remains result label" \
    "$expected_lower_headers" \
    "SELECT version();"

expected_spaced_headers=$(cat <<EOF
VERSION ()
${version}
EOF
)
expect_output_with_headers \
    "spaced version label remains result label" \
    "$expected_spaced_headers" \
    "SELECT VERSION ();"

expected_parenthesized_headers=$(cat <<EOF
(VERSION())
${version}
EOF
)
expect_output_with_headers \
    "parenthesized version label remains result label" \
    "$expected_parenthesized_headers" \
    "SELECT (VERSION());"

expect_output \
    "from dual returns version" \
    "${version}" \
    "SELECT VERSION() FROM DUAL;"

expect_output \
    "selected database does not change version function" \
    "${version}	${DATABASE}" \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; SELECT VERSION(), DATABASE();"

expect_error \
    "version function rejects integer argument" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT VERSION(1);"

expect_error \
    "version function rejects null argument" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT VERSION(NULL);"

expect_error \
    "version function rejects string argument" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT VERSION('x');"

expect_error \
    "version function rejects multiple arguments" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT VERSION(1, 2);"

expect_error \
    "bare version is not a version function" \
    1054 \
    42S22 \
    "Unknown column 'VERSION'" \
    "SELECT VERSION;"

expect_output \
    "mysql accepts limit outside this mylite slice" \
    "${version}" \
    "SELECT VERSION() LIMIT 1;"
