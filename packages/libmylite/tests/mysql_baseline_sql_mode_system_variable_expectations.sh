#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"
DEFAULT_SQL_MODE="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION"

fail() {
    printf '%s\n' "mysql_baseline_sql_mode_system_variable_expectations: $1" >&2
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

expected_values="$DEFAULT_SQL_MODE	$DEFAULT_SQL_MODE	$DEFAULT_SQL_MODE	$DEFAULT_SQL_MODE	0	0	-1"
values=$(run_mysql \
    "SELECT 1; SELECT @@sql_mode, @@global.sql_mode, @@session.sql_mode, \
     @@local.sql_mode, @@warning_count, @@error_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "sql_mode variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@sql_mode	@@global.sql_mode	@@session.\`sql_mode\`	@@\`sql_mode\`
$DEFAULT_SQL_MODE	$DEFAULT_SQL_MODE	$DEFAULT_SQL_MODE	$DEFAULT_SQL_MODE
EOF
)
expect_output_with_headers \
    "sql_mode labels preserve source text" \
    "$expected_headers" \
    "SELECT @@sql_mode, @@global.sql_mode, \
     @@session.\`sql_mode\`, @@\`sql_mode\`;"

expect_output \
    "case-insensitive sql_mode variables" \
    "$DEFAULT_SQL_MODE	$DEFAULT_SQL_MODE" \
    "SELECT @@SQL_MODE, @@Global.Sql_Mode;"

expect_output \
    "from dual returns sql_mode" \
    "$DEFAULT_SQL_MODE" \
    "SELECT @@sql_mode FROM DUAL;"

mutable_values=$(run_mysql \
    "SELECT @@sql_mode, @@global.sql_mode; \
     SET SESSION sql_mode='ANSI_QUOTES'; \
     SELECT @@sql_mode, @@global.sql_mode, @@session.sql_mode, \
            @@local.sql_mode, @@warning_count, @@error_count, ROW_COUNT(); \
     SET SESSION sql_mode=DEFAULT;" \
    | tail -n 1)
expect_value \
    "mysql session sql_mode is mutable upstream" \
    "ANSI_QUOTES	$DEFAULT_SQL_MODE	ANSI_QUOTES	ANSI_QUOTES	0	0	0" \
    "$mutable_values"

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@sql_mode, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_mode variable reads and clears warning diagnostics" \
    "$DEFAULT_SQL_MODE	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@sql_mode, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_mode variable reads and clears error diagnostics" \
    "$DEFAULT_SQL_MODE	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "unknown unscoped sql_mode variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_mode_variable'" \
    "SELECT @@no_such_sql_mode_variable;"

expect_error \
    "unknown scoped sql_mode variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_mode_variable'" \
    "SELECT @@global.no_such_sql_mode_variable;"

expect_error \
    "quoted sql_mode variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.sql_mode;"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "1" \
    "SELECT @@sql_mode + 1;"
