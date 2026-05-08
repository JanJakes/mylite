#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"
DATABASE="mylite_db_charset_vars_$$"

fail() {
    printf '%s\n' "mysql_baseline_database_character_set_system_variables_expectations: $1" >&2
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

expected_values="utf8mb4	utf8mb4	utf8mb4	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci	0	-1"
values=$(run_mysql \
    "SELECT 1; SELECT @@character_set_database, @@global.character_set_database, \
     @@session.character_set_database, @@local.character_set_database, \
     @@collation_database, @@global.collation_database, @@session.collation_database, \
     @@local.collation_database, @@warning_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "database charset variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@character_set_database	@@global.collation_database	@@session.\`character_set_database\`	@@\`collation_database\`
utf8mb4	utf8mb4_0900_ai_ci	utf8mb4	utf8mb4_0900_ai_ci
EOF
)
expect_output_with_headers \
    "database charset labels preserve source text" \
    "$expected_headers" \
    "SELECT @@character_set_database, @@global.collation_database, \
     @@session.\`character_set_database\`, @@\`collation_database\`;"

expect_output \
    "case-insensitive database charset variables" \
    "utf8mb4	utf8mb4_0900_ai_ci" \
    "SELECT @@CHARACTER_SET_DATABASE, @@GLOBAL.COLLATION_DATABASE;"

expect_output \
    "from dual returns database charset variables" \
    "utf8mb4	utf8mb4_0900_ai_ci" \
    "SELECT @@character_set_database, @@collation_database FROM DUAL;"

expect_output \
    "selected database exposes default charset variables" \
    "${DATABASE}	utf8mb4	utf8mb4_0900_ai_ci	0	0" \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; \
     SELECT DATABASE(), @@character_set_database, @@collation_database, \
     @@warning_count, ROW_COUNT();"

expect_output \
    "dropped selected database falls back to server defaults" \
    "NULL	utf8mb4	utf8mb4_0900_ai_ci" \
    "DROP DATABASE ${DATABASE}; SELECT DATABASE(), @@character_set_database, @@collation_database;"

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@character_set_database, @@collation_database, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "database charset variables read and clear warning diagnostics" \
    "utf8mb4	utf8mb4_0900_ai_ci	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@character_set_database, @@collation_database, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "database charset variables read and clear error diagnostics" \
    "utf8mb4	utf8mb4_0900_ai_ci	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "unknown unscoped database charset variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_database_charset_variable'" \
    "SELECT @@no_such_database_charset_variable;"

expect_error \
    "unknown scoped database charset variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_database_charset_variable'" \
    "SELECT @@global.no_such_database_charset_variable;"

expect_error \
    "quoted database charset variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.character_set_database;"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "1" \
    "SELECT @@character_set_database + 1;"

cleanup
