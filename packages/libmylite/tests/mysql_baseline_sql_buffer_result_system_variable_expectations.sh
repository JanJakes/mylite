#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"

fail() {
    printf '%s\n' "mysql_baseline_sql_buffer_result_system_variable_expectations: $1" >&2
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expected_values="0	0	0	0	0	0	-1"
values=$(run_mysql \
    "SELECT 1; SELECT @@sql_buffer_result, @@global.sql_buffer_result, \
     @@session.sql_buffer_result, @@local.sql_buffer_result, \
     @@warning_count, @@error_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "sql_buffer_result variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@sql_buffer_result	@@global.sql_buffer_result	@@session.\`sql_buffer_result\`	@@\`sql_buffer_result\`
0	0	0	0
EOF
)
expect_output_with_headers \
    "sql_buffer_result labels preserve source text" \
    "$expected_headers" \
    "SELECT @@sql_buffer_result, @@global.sql_buffer_result, \
     @@session.\`sql_buffer_result\`, @@\`sql_buffer_result\`;"

expect_output \
    "case-insensitive sql_buffer_result variables" \
    "0	0" \
    "SELECT @@SQL_BUFFER_RESULT, @@Global.Sql_Buffer_Result;"

expect_output \
    "from dual returns sql_buffer_result" \
    "0" \
    "SELECT @@sql_buffer_result FROM DUAL;"

mutable_values=$(run_mysql \
    "SELECT @@sql_buffer_result, @@global.sql_buffer_result; \
     SET SESSION sql_buffer_result=1; \
     SELECT @@sql_buffer_result, @@global.sql_buffer_result, @@session.sql_buffer_result, \
            @@local.sql_buffer_result, @@warning_count, @@error_count, ROW_COUNT(); \
     SET SESSION sql_buffer_result=DEFAULT;" \
    | tail -n 1)
expect_value \
    "mysql session sql_buffer_result is mutable upstream" \
    "1	0	1	1	0	0	0" \
    "$mutable_values"

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@sql_buffer_result, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_buffer_result variable reads and clears warning diagnostics" \
    "0	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@sql_buffer_result, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_buffer_result variable reads and clears error diagnostics" \
    "0	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "unknown unscoped sql_buffer_result variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_buffer_result_variable'" \
    "SELECT @@no_such_sql_buffer_result_variable;"

expect_error \
    "unknown scoped sql_buffer_result variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_buffer_result_variable'" \
    "SELECT @@global.no_such_sql_buffer_result_variable;"

expect_error \
    "quoted sql_buffer_result variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.sql_buffer_result;"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "1" \
    "SELECT @@sql_buffer_result + 1;"
