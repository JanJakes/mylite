#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_current_user_identity_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_current_user_identity_expectations: $1" >&2
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

client_identity=$(run_mysql 'SELECT USER();')
current_identity=$(run_mysql 'SELECT CURRENT_USER();')

expect_output \
    "identity functions return values and no warnings" \
    "${client_identity}	${current_identity}	${current_identity}	0" \
    "DO 0; SELECT USER(), CURRENT_USER(), CURRENT_USER, @@warning_count;"

expected_lower_headers=$(cat <<EOF
user()	current_user()	current_user
${client_identity}	${current_identity}	${current_identity}
EOF
)
expect_output_with_headers \
    "lower-case identity labels remain result labels" \
    "$expected_lower_headers" \
    "SELECT user(), current_user(), current_user;"

expected_spaced_headers=$(cat <<EOF
USER ()	CURRENT_USER ()
${client_identity}	${current_identity}
EOF
)
expect_output_with_headers \
    "spaced identity labels remain result labels" \
    "$expected_spaced_headers" \
    "SELECT USER (), CURRENT_USER ();"

expected_parenthesized_headers=$(cat <<EOF
(USER())	(CURRENT_USER())	(CURRENT_USER)
${client_identity}	${current_identity}	${current_identity}
EOF
)
expect_output_with_headers \
    "parenthesized identity labels remain result labels" \
    "$expected_parenthesized_headers" \
    "SELECT (USER()), (CURRENT_USER()), (CURRENT_USER);"

expect_output \
    "from dual returns client identity" \
    "${client_identity}" \
    "SELECT USER() FROM DUAL;"

expect_output \
    "from dual returns current identity" \
    "${current_identity}" \
    "SELECT CURRENT_USER FROM DUAL;"

expect_output \
    "selected database does not change identity functions" \
    "${client_identity}	${current_identity}	${DATABASE}" \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; SELECT USER(), CURRENT_USER, DATABASE();"

expect_error \
    "user function rejects arguments" \
    1064 \
    42000 \
    "near '1)'" \
    "SELECT USER(1);"

expect_error \
    "current user function rejects arguments" \
    1064 \
    42000 \
    "near '1)'" \
    "SELECT CURRENT_USER(1);"

expect_error \
    "bare user is not an identity function" \
    1054 \
    42S22 \
    "Unknown column 'USER'" \
    "SELECT USER;"

expect_output \
    "mysql accepts limit outside this mylite slice" \
    "${client_identity}" \
    "SELECT USER() LIMIT 1;"
