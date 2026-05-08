#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"

fail() {
    printf '%s\n' "mysql_baseline_character_set_system_variables_expectations: $1" >&2
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

headers=$(run_mysql_with_headers "SELECT 1; SELECT @@character_set_client, @@character_set_connection, @@character_set_results, @@collation_connection, @@warning_count, ROW_COUNT();" | tail -n 2 | head -n 1)
expect_value \
    "charset variable headers" \
    "@@character_set_client	@@character_set_connection	@@character_set_results	@@collation_connection	@@warning_count	ROW_COUNT()" \
    "$headers"

values=$(run_mysql "SELECT 1; SELECT @@character_set_client, @@character_set_connection, @@character_set_results, @@collation_connection, @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value \
    "charset variable values" \
    "utf8mb4	utf8mb4	utf8mb4	utf8mb4_0900_ai_ci	0	-1" \
    "$values"

scoped=$(run_mysql "SELECT 1; SELECT @@session.character_set_client, @@local.character_set_connection, @@global.character_set_results, @@global.collation_connection;" | tail -n 1)
expect_value \
    "charset variable scoped values" \
    "utf8mb4	utf8mb4	utf8mb4	utf8mb4_0900_ai_ci" \
    "$scoped"

case_values=$(run_mysql "SELECT 1; SELECT @@CHARACTER_SET_CLIENT, @@Session.Character_Set_Connection, @@LOCAL.Character_Set_Results, @@GLOBAL.Collation_Connection;" | tail -n 1)
expect_value \
    "charset variable case-insensitive values" \
    "utf8mb4	utf8mb4	utf8mb4	utf8mb4_0900_ai_ci" \
    "$case_values"

quoted_headers=$(run_mysql_with_headers "SELECT 1; SELECT @@session.\`character_set_client\`, @@\`character_set_results\`, @@global.\`collation_connection\`;" | tail -n 2 | head -n 1)
expect_value \
    "quoted charset variable headers" \
    "@@session.\`character_set_client\`	@@\`character_set_results\`	@@global.\`collation_connection\`" \
    "$quoted_headers"

quoted_values=$(run_mysql "SELECT 1; SELECT @@session.\`character_set_client\`, @@\`character_set_results\`, @@global.\`collation_connection\`;" | tail -n 1)
expect_value \
    "quoted charset variable values" \
    "utf8mb4	utf8mb4	utf8mb4_0900_ai_ci" \
    "$quoted_values"

warning_then_scalar=$(run_mysql "SELECT 1; SHOW PROCESSLIST; SELECT @@character_set_client, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;" | tail -n 2 | tr '\n' '|')
expect_value \
    "charset scalar select reads and clears warning diagnostics" \
    "utf8mb4	1	0	-1|0|" \
    "$warning_then_scalar"

parse_error_scalar=$(printf '%s\n' 'BAD SQL; SELECT @@character_set_connection, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "charset scalar select reads and clears error diagnostics" \
    "utf8mb4	1	1	-1|0|" \
    "$parse_error_scalar"

expect_error \
    "unknown unscoped charset system variable rejected" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_charset_variable'" \
    "SELECT 1; SELECT @@no_such_charset_variable;"

expect_error \
    "unknown scoped charset system variable reports final name" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_charset_variable'" \
    "SELECT 1; SELECT @@session.no_such_charset_variable;"

expect_error \
    "unknown global charset system variable reports final name" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_charset_variable'" \
    "SELECT 1; SELECT @@global.no_such_charset_variable;"

expect_error \
    "quoted scope rejected by MySQL parser" \
    1064 \
    42000 \
    "\`session\`.character_set_client" \
    "SELECT 1; SELECT @@\`session\`.character_set_client;"
