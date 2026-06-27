#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"
DATABASE="mylite_srv_charset_$$"

fail() {
    printf '%s\n' "mysql_baseline_server_character_set_system_variables_expectations: $1" >&2
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
    "SELECT 1; SELECT @@character_set_server, @@global.character_set_server, \
     @@session.character_set_server, @@local.character_set_server, \
     @@collation_server, @@global.collation_server, @@session.collation_server, \
     @@local.collation_server, @@warning_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "server charset variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@character_set_server	@@global.collation_server	@@session.\`character_set_server\`	@@\`collation_server\`
utf8mb4	utf8mb4_0900_ai_ci	utf8mb4	utf8mb4_0900_ai_ci
EOF
)
expect_output_with_headers \
    "server charset labels preserve source text" \
    "$expected_headers" \
    "SELECT @@character_set_server, @@global.collation_server, \
     @@session.\`character_set_server\`, @@\`collation_server\`;"

expect_output \
    "case-insensitive server charset variables" \
    "utf8mb4	utf8mb4_0900_ai_ci" \
    "SELECT @@CHARACTER_SET_SERVER, @@GLOBAL.COLLATION_SERVER;"

expect_output \
    "from dual returns server charset variables" \
    "utf8mb4	utf8mb4_0900_ai_ci" \
    "SELECT @@character_set_server, @@collation_server FROM DUAL;"

assignment_values=$(run_mysql \
    "SET SESSION character_set_server=latin1; \
     SELECT @@character_set_server, @@global.character_set_server, \
            @@session.character_set_server, @@local.character_set_server, \
            @@collation_server, @@global.collation_server, @@session.collation_server, \
            @@local.collation_server, @@warning_count, ROW_COUNT(); \
     SET LOCAL collation_server=utf8mb4_bin; \
     SELECT @@character_set_server, @@global.character_set_server, \
            @@session.character_set_server, @@local.character_set_server, \
            @@collation_server, @@global.collation_server, @@session.collation_server, \
            @@local.collation_server, @@warning_count, ROW_COUNT(); \
     SET character_set_server=33; \
     SELECT @@character_set_server, @@global.character_set_server, \
            @@session.character_set_server, @@local.character_set_server, \
            @@collation_server, @@global.collation_server, @@session.collation_server, \
            @@local.collation_server, @@warning_count, ROW_COUNT(); \
     SET @server_collation_id = 255; SET collation_server=@server_collation_id; \
     SELECT @@character_set_server, @@global.character_set_server, \
            @@session.character_set_server, @@local.character_set_server, \
            @@collation_server, @@global.collation_server, @@session.collation_server, \
            @@local.collation_server, @@warning_count, ROW_COUNT(); \
     SET character_set_server=DEFAULT; \
     SELECT @@character_set_server, @@global.character_set_server, \
            @@session.character_set_server, @@local.character_set_server, \
            @@collation_server, @@global.collation_server, @@session.collation_server, \
            @@local.collation_server, @@warning_count, ROW_COUNT();" \
    | tail -n 5 \
    | tr '\n' '|')
expect_value \
    "server charset assignment canonicalization and collation coupling" \
    "latin1	utf8mb4	latin1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci	latin1_swedish_ci	latin1_swedish_ci	0	0|utf8mb4	utf8mb4	utf8mb4	utf8mb4	utf8mb4_bin	utf8mb4_0900_ai_ci	utf8mb4_bin	utf8mb4_bin	0	0|utf8mb3	utf8mb4	utf8mb3	utf8mb3	utf8mb3_general_ci	utf8mb4_0900_ai_ci	utf8mb3_general_ci	utf8mb3_general_ci	1	0|utf8mb4	utf8mb4	utf8mb4	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci	0	0|utf8mb4	utf8mb4	utf8mb4	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci	0	0|" \
    "$assignment_values"

expect_output \
    "selected database does not change server defaults" \
    "utf8mb4	utf8mb4_0900_ai_ci	${DATABASE}" \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; \
     SELECT @@character_set_server, @@collation_server, DATABASE();"

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@character_set_server, @@collation_server, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "server charset variables read and clear warning diagnostics" \
    "utf8mb4	utf8mb4_0900_ai_ci	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@character_set_server, @@collation_server, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "server charset variables read and clear error diagnostics" \
    "utf8mb4	utf8mb4_0900_ai_ci	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "unknown unscoped server charset variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_server_charset_variable'" \
    "SELECT @@no_such_server_charset_variable;"

expect_error \
    "unknown scoped server charset variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_server_charset_variable'" \
    "SELECT @@global.no_such_server_charset_variable;"

expect_error \
    "quoted server charset variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.character_set_server;"

expect_error \
    "unknown server charset assignment" \
    1115 \
    42000 \
    "Unknown character set: 'nosuch'" \
    "SET SESSION character_set_server=nosuch;"

expect_error \
    "string digit server charset assignment is a charset name" \
    1115 \
    42000 \
    "Unknown character set: '33'" \
    "SET SESSION character_set_server='33';"

expect_error \
    "unknown server collation assignment" \
    1273 \
    HY000 \
    "Unknown collation: 'nosuch'" \
    "SET SESSION collation_server=nosuch;"

expect_error \
    "string digit server collation assignment is a collation name" \
    1273 \
    HY000 \
    "Unknown collation: '255'" \
    "SET SESSION collation_server='255';"

expect_error \
    "unknown server collation id" \
    1273 \
    HY000 \
    "Unknown collation: '999'" \
    "SET SESSION collation_server=999;"

expect_error \
    "decimal server collation assignment type" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'collation_server'" \
    "SET SESSION collation_server=33.0;"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "1" \
    "SELECT @@character_set_server + 1;"

cleanup
