#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"

fail() {
    printf '%s\n' "mysql_baseline_sql_generate_invisible_primary_key_system_variable_expectations: $1" >&2
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
    "SELECT 1; SELECT @@sql_generate_invisible_primary_key, @@global.sql_generate_invisible_primary_key, \
     @@session.sql_generate_invisible_primary_key, @@local.sql_generate_invisible_primary_key, \
     @@warning_count, @@error_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "sql_generate_invisible_primary_key variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@sql_generate_invisible_primary_key	@@global.sql_generate_invisible_primary_key	@@session.\`sql_generate_invisible_primary_key\`	@@\`sql_generate_invisible_primary_key\`
0	0	0	0
EOF
)
expect_output_with_headers \
    "sql_generate_invisible_primary_key labels preserve source text" \
    "$expected_headers" \
    "SELECT @@sql_generate_invisible_primary_key, @@global.sql_generate_invisible_primary_key, \
     @@session.\`sql_generate_invisible_primary_key\`, @@\`sql_generate_invisible_primary_key\`;"

expect_output \
    "case-insensitive sql_generate_invisible_primary_key variables" \
    "0	0" \
    "SELECT @@SQL_GENERATE_INVISIBLE_PRIMARY_KEY, @@Global.Sql_Generate_Invisible_Primary_Key;"

expect_output \
    "from dual returns sql_generate_invisible_primary_key" \
    "0" \
    "SELECT @@sql_generate_invisible_primary_key FROM DUAL;"

mutable_values=$(run_mysql \
    "SELECT @@sql_generate_invisible_primary_key, @@global.sql_generate_invisible_primary_key; \
     SET SESSION sql_generate_invisible_primary_key=1; \
     SELECT @@sql_generate_invisible_primary_key, @@global.sql_generate_invisible_primary_key, @@session.sql_generate_invisible_primary_key, \
            @@local.sql_generate_invisible_primary_key, @@warning_count, @@error_count, ROW_COUNT(); \
     SET SESSION sql_generate_invisible_primary_key=DEFAULT;" \
    | tail -n 1)
expect_value \
    "mysql session sql_generate_invisible_primary_key is mutable upstream" \
    "1	0	1	1	0	0	0" \
    "$mutable_values"

gipk_values=$(run_mysql \
    "DROP DATABASE IF EXISTS mylite_sql_generate_invisible_primary_key_expectations; \
     CREATE DATABASE mylite_sql_generate_invisible_primary_key_expectations; \
     USE mylite_sql_generate_invisible_primary_key_expectations; \
     SET SESSION sql_generate_invisible_primary_key=0; \
     CREATE TABLE off_table (c1 INT); \
     SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS \
      WHERE TABLE_SCHEMA = 'mylite_sql_generate_invisible_primary_key_expectations' \
        AND TABLE_NAME = 'off_table' AND COLUMN_NAME = 'my_row_id'; \
     SET SESSION sql_generate_invisible_primary_key=1; \
     CREATE TABLE on_table (c1 INT); \
     SELECT COLUMN_NAME, DATA_TYPE, COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS \
      WHERE TABLE_SCHEMA = 'mylite_sql_generate_invisible_primary_key_expectations' \
        AND TABLE_NAME = 'on_table' AND COLUMN_NAME = 'my_row_id'; \
     DROP DATABASE mylite_sql_generate_invisible_primary_key_expectations;" \
    | tr '\n' '|')
expect_value \
    "mysql GIPK table creation depends on sql_generate_invisible_primary_key" \
    "0|my_row_id	bigint	PRI|" \
    "$gipk_values"

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@sql_generate_invisible_primary_key, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_generate_invisible_primary_key variable reads and clears warning diagnostics" \
    "0	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@sql_generate_invisible_primary_key, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_generate_invisible_primary_key variable reads and clears error diagnostics" \
    "0	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "unknown unscoped sql_generate_invisible_primary_key variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_generate_invisible_primary_key_variable'" \
    "SELECT @@no_such_sql_generate_invisible_primary_key_variable;"

expect_error \
    "unknown scoped sql_generate_invisible_primary_key variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_generate_invisible_primary_key_variable'" \
    "SELECT @@global.no_such_sql_generate_invisible_primary_key_variable;"

expect_error \
    "quoted sql_generate_invisible_primary_key variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.sql_generate_invisible_primary_key;"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "1" \
    "SELECT @@sql_generate_invisible_primary_key + 1;"
