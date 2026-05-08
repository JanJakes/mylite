#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"
DATABASE="mylite_sql_require_primary_key_probe"

fail() {
    printf '%s\n' "mysql_baseline_sql_require_primary_key_system_variable_expectations: $1" >&2
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
    "SELECT 1; SELECT @@sql_require_primary_key, @@global.sql_require_primary_key, \
     @@session.sql_require_primary_key, @@local.sql_require_primary_key, \
     @@warning_count, @@error_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "sql_require_primary_key variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@sql_require_primary_key	@@global.sql_require_primary_key	@@session.\`sql_require_primary_key\`	@@\`sql_require_primary_key\`
0	0	0	0
EOF
)
expect_output_with_headers \
    "sql_require_primary_key labels preserve source text" \
    "$expected_headers" \
    "SELECT @@sql_require_primary_key, @@global.sql_require_primary_key, \
     @@session.\`sql_require_primary_key\`, @@\`sql_require_primary_key\`;"

expect_output \
    "case-insensitive sql_require_primary_key variables" \
    "0	0" \
    "SELECT @@SQL_REQUIRE_PRIMARY_KEY, @@Global.Sql_Require_Primary_Key;"

expect_output \
    "from dual returns sql_require_primary_key" \
    "0" \
    "SELECT @@sql_require_primary_key FROM DUAL;"

mutable_values=$(run_mysql \
    "SELECT @@sql_require_primary_key, @@global.sql_require_primary_key; \
     SET SESSION sql_require_primary_key=1; \
     SELECT @@sql_require_primary_key, @@global.sql_require_primary_key, \
            @@session.sql_require_primary_key, @@local.sql_require_primary_key, \
            @@warning_count, @@error_count, ROW_COUNT(); \
     SET SESSION sql_require_primary_key=DEFAULT;" \
    | tail -n 1)
expect_value \
    "mysql session sql_require_primary_key is mutable upstream" \
    "1	0	1	1	0	0	0" \
    "$mutable_values"

set +e
primary_key_error=$(printf '%s\n' \
    "SET SESSION sql_require_primary_key=1; \
     DROP DATABASE IF EXISTS \`$DATABASE\`; \
     CREATE DATABASE \`$DATABASE\`; \
     USE \`$DATABASE\`; \
     CREATE TABLE no_pk (id INT); \
     SET SESSION sql_require_primary_key=DEFAULT; \
     DROP DATABASE IF EXISTS \`$DATABASE\`;" \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names 2>&1)
primary_key_status=$?
set -e
run_mysql \
    "SET SESSION sql_require_primary_key=DEFAULT; \
     DROP DATABASE IF EXISTS \`$DATABASE\`;" \
    >/dev/null
if [ "$primary_key_status" -eq 0 ]; then
    fail "expected primary-key enforcement error, command succeeded with [$primary_key_error]"
fi
case "$primary_key_error" in
    *"ERROR 3750 (HY000)"*"without a primary key"*) ;;
    *)
        fail "expected primary-key enforcement error 3750/HY000, got [$primary_key_error]"
        ;;
esac

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@sql_require_primary_key, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_require_primary_key variable reads and clears warning diagnostics" \
    "0	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@sql_require_primary_key, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_require_primary_key variable reads and clears error diagnostics" \
    "0	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "unknown unscoped sql_require_primary_key variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_require_primary_key_variable'" \
    "SELECT @@no_such_sql_require_primary_key_variable;"

expect_error \
    "unknown scoped sql_require_primary_key variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_require_primary_key_variable'" \
    "SELECT @@global.no_such_sql_require_primary_key_variable;"

expect_error \
    "quoted sql_require_primary_key variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.sql_require_primary_key;"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "1" \
    "SELECT @@sql_require_primary_key + 1;"
