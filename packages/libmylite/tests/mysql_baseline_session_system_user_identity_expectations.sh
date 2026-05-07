#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_session_system_user_identity_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_session_system_user_identity_expectations: $1" >&2
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

expect_output \
    "identity aliases return client identity and no warnings" \
    "${client_identity}	${client_identity}	${client_identity}	0" \
    "DO 0; SELECT USER(), SESSION_USER(), SYSTEM_USER(), @@warning_count;"

expected_lower_headers=$(cat <<EOF
session_user()	system_user()
${client_identity}	${client_identity}
EOF
)
expect_output_with_headers \
    "lower-case alias labels remain result labels" \
    "$expected_lower_headers" \
    "SELECT session_user(), system_user();"

expected_parenthesized_headers=$(cat <<EOF
(SESSION_USER())	(System_User())
${client_identity}	${client_identity}
EOF
)
expect_output_with_headers \
    "parenthesized alias labels remain result labels" \
    "$expected_parenthesized_headers" \
    "SELECT (SESSION_USER()), (System_User());"

expected_comment_headers=$(cat <<EOF
SESSION_USER(/* inside */ )	SYSTEM_USER(/* inside */ )
${client_identity}	${client_identity}
EOF
)
expect_output_with_headers \
    "comments inside empty alias argument lists are accepted" \
    "$expected_comment_headers" \
    "SELECT SESSION_USER(/* inside */), SYSTEM_USER(/* inside */);"

expect_output \
    "from dual returns session user alias" \
    "${client_identity}" \
    "SELECT SESSION_USER() FROM DUAL;"

expect_output \
    "from dual returns system user alias" \
    "${client_identity}" \
    "SELECT SYSTEM_USER() FROM DUAL;"

expect_output \
    "selected database does not change identity aliases" \
    "${client_identity}	${client_identity}	${DATABASE}" \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; SELECT SESSION_USER(), SYSTEM_USER(), DATABASE();"

expect_error \
    "session user function rejects arguments" \
    1064 \
    42000 \
    "near '1)'" \
    "SELECT SESSION_USER(1);"

expect_error \
    "system user function rejects arguments" \
    1064 \
    42000 \
    "near '1)'" \
    "SELECT SYSTEM_USER(1);"

expect_error \
    "bare session user is not an identity function" \
    1054 \
    42S22 \
    "Unknown column 'SESSION_USER'" \
    "SELECT SESSION_USER;"

expect_error \
    "bare system user is not an identity function" \
    1054 \
    42S22 \
    "Unknown column 'SYSTEM_USER'" \
    "SELECT SYSTEM_USER;"

expect_error \
    "spaced session user needs selected database for stored function resolution" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT SESSION_USER ();"

expect_error \
    "spaced system user resolves as missing stored function" \
    1630 \
    42000 \
    "FUNCTION ${DATABASE}.SYSTEM_USER does not exist" \
    "USE ${DATABASE}; SELECT SYSTEM_USER ();"

expect_error \
    "comment-separated session user resolves as missing stored function" \
    1630 \
    42000 \
    "FUNCTION ${DATABASE}.SESSION_USER does not exist" \
    "USE ${DATABASE}; SELECT SESSION_USER/**/();"

expect_output \
    "mysql accepts limit outside this mylite slice" \
    "${client_identity}" \
    "SELECT SESSION_USER() LIMIT 1;"
