#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"
DATABASE="mylite_system_charset_vars_$$"

fail() {
    printf '%s\n' "mysql_baseline_system_character_set_system_variable_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS "$@"
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

expected_values="utf8mb3	utf8mb3	0	-1"
values=$(run_mysql \
    "SELECT 1; SELECT @@character_set_system, @@global.character_set_system, \
     @@warning_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "system charset variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@character_set_system	@@global.\`character_set_system\`	@@\`character_set_system\`
utf8mb3	utf8mb3	utf8mb3
EOF
)
expect_output_with_headers \
    "system charset labels preserve source text" \
    "$expected_headers" \
    "SELECT @@character_set_system, @@global.\`character_set_system\`, \
     @@\`character_set_system\`;"

expect_output \
    "case-insensitive system charset variables" \
    "utf8mb3	utf8mb3" \
    "SELECT @@CHARACTER_SET_SYSTEM, @@GLOBAL.CHARACTER_SET_SYSTEM;"

expect_output \
    "from dual returns system charset variable" \
    "utf8mb3" \
    "SELECT @@character_set_system FROM DUAL;"

expect_output \
    "selected database does not change system charset variable" \
    "utf8mb3	${DATABASE}" \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; \
     SELECT @@character_set_system, DATABASE();"

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@character_set_system, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "system charset variable reads and clears warning diagnostics" \
    "utf8mb3	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@character_set_system, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "system charset variable reads and clears error diagnostics" \
    "utf8mb3	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "session system charset variable is global-only" \
    1238 \
    HY000 \
    "Variable 'character_set_system' is a GLOBAL variable" \
    "SELECT @@session.character_set_system;"

expect_error \
    "local system charset variable is global-only" \
    1238 \
    HY000 \
    "Variable 'character_set_system' is a GLOBAL variable" \
    "SELECT @@local.character_set_system;"

expect_error \
    "unknown unscoped system charset variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_system_charset_variable'" \
    "SELECT @@no_such_system_charset_variable;"

expect_error \
    "unknown scoped system charset variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_system_charset_variable'" \
    "SELECT @@global.no_such_system_charset_variable;"

expect_error \
    "quoted system charset variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.character_set_system;"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "1" \
    "SELECT @@character_set_system + 1;"

cleanup
