#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"

fail() {
    printf '%s\n' "mysql_baseline_sql_auto_is_null_system_variable_expectations: $1" >&2
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
    "SELECT 1; SELECT @@sql_auto_is_null, @@global.sql_auto_is_null, \
     @@session.sql_auto_is_null, @@local.sql_auto_is_null, \
     @@warning_count, @@error_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "sql_auto_is_null variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@sql_auto_is_null	@@global.sql_auto_is_null	@@session.\`sql_auto_is_null\`	@@\`sql_auto_is_null\`
0	0	0	0
EOF
)
expect_output_with_headers \
    "sql_auto_is_null labels preserve source text" \
    "$expected_headers" \
    "SELECT @@sql_auto_is_null, @@global.sql_auto_is_null, \
     @@session.\`sql_auto_is_null\`, @@\`sql_auto_is_null\`;"

expect_output \
    "case-insensitive sql_auto_is_null variables" \
    "0	0" \
    "SELECT @@SQL_AUTO_IS_NULL, @@Global.Sql_Auto_Is_Null;"

expect_output \
    "from dual returns sql_auto_is_null" \
    "0" \
    "SELECT @@sql_auto_is_null FROM DUAL;"

mutable_values=$(run_mysql \
    "SELECT @@sql_auto_is_null, @@global.sql_auto_is_null; \
     SET SESSION sql_auto_is_null=1; \
     SELECT @@sql_auto_is_null, @@global.sql_auto_is_null, @@session.sql_auto_is_null, \
            @@local.sql_auto_is_null, @@warning_count, @@error_count, ROW_COUNT(); \
     SET SESSION sql_auto_is_null=DEFAULT;" \
    | tail -n 1)
expect_value \
    "mysql session sql_auto_is_null is mutable upstream" \
    "1	0	1	1	0	0	0" \
    "$mutable_values"

auto_is_null_values=$(run_mysql \
    "DROP DATABASE IF EXISTS mylite_sql_auto_is_null_expectations; \
     CREATE DATABASE mylite_sql_auto_is_null_expectations; \
     USE mylite_sql_auto_is_null_expectations; \
     CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, label INT NULL); \
     SET SESSION sql_auto_is_null=1; \
     INSERT INTO t (label) VALUES (10); \
     SELECT id, label FROM t WHERE id IS NULL; \
     INSERT INTO t (label) VALUES (20),(30); \
     SELECT id, label FROM t WHERE id IS NULL ORDER BY id; \
     SET SESSION sql_auto_is_null=0; \
     INSERT INTO t (label) VALUES (40); \
     SELECT COUNT(*) FROM t WHERE id IS NULL; \
     SELECT COUNT(*) FROM t WHERE label IS NULL; \
     DROP DATABASE mylite_sql_auto_is_null_expectations;" \
    | tr '\n' '|')
expect_value \
    "mysql auto-increment IS NULL lookup depends on sql_auto_is_null" \
    "1	10|2	20|0|0|" \
    "$auto_is_null_values"

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@sql_auto_is_null, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_auto_is_null variable reads and clears warning diagnostics" \
    "0	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@sql_auto_is_null, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_auto_is_null variable reads and clears error diagnostics" \
    "0	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "unknown unscoped sql_auto_is_null variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_auto_is_null_variable'" \
    "SELECT @@no_such_sql_auto_is_null_variable;"

expect_error \
    "unknown scoped sql_auto_is_null variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_auto_is_null_variable'" \
    "SELECT @@global.no_such_sql_auto_is_null_variable;"

expect_error \
    "quoted sql_auto_is_null variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.sql_auto_is_null;"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "1" \
    "SELECT @@sql_auto_is_null + 1;"
