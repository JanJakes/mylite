#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"

fail() {
    printf '%s\n' "mysql_baseline_sql_log_bin_system_variable_expectations: $1" >&2
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

expected_values="1	1	1	0	0	-1"
values=$(run_mysql \
    "SELECT 1; SELECT @@sql_log_bin, @@session.sql_log_bin, \
     @@local.sql_log_bin, @@warning_count, @@error_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "sql_log_bin variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@sql_log_bin	@@session.\`sql_log_bin\`	@@\`sql_log_bin\`	(@@sql_log_bin)
1	1	1	1
EOF
)
expect_output_with_headers \
    "sql_log_bin labels preserve source text" \
    "$expected_headers" \
    "SELECT @@sql_log_bin, @@session.\`sql_log_bin\`, @@\`sql_log_bin\`, \
     (@@sql_log_bin);"

expect_output \
    "case-insensitive sql_log_bin variables" \
    "1	1" \
    "SELECT @@SQL_LOG_BIN, @@Session.Sql_Log_Bin;"

expect_output \
    "from dual returns sql_log_bin" \
    "1" \
    "SELECT @@sql_log_bin FROM DUAL;"

mutable_values=$(run_mysql \
    "SELECT @@sql_log_bin; \
     SET SESSION sql_log_bin=0; \
     SELECT @@sql_log_bin, @@session.sql_log_bin, @@local.sql_log_bin, \
            @@warning_count, @@error_count, ROW_COUNT(); \
     SET SESSION sql_log_bin=1;" \
    | tail -n 1)
expect_value \
    "mysql session sql_log_bin is mutable upstream" \
    "0	0	0	0	0	0" \
    "$mutable_values"

show_mutable_values=$(run_mysql \
    "SET SESSION sql_log_bin=0; \
     SHOW VARIABLES LIKE 'sql_log_bin'; \
     SHOW GLOBAL VARIABLES LIKE 'sql_log_bin'; \
     SET SESSION sql_log_bin=1;" \
    | tr '\n' '|')
expect_value \
    "mysql SHOW VARIABLES reflects session sql_log_bin and omits global" \
    "sql_log_bin	OFF|" \
    "$show_mutable_values"

boolean_form_values=$(run_mysql \
    "SET SESSION sql_log_bin=ON; \
     SELECT @@sql_log_bin; \
     SET LOCAL sql_log_bin=FALSE; \
     SELECT @@sql_log_bin; \
     SET @@session.sql_log_bin=TRUE; \
     SELECT @@sql_log_bin; \
     SET SESSION sql_log_bin=DEFAULT; \
     SELECT @@sql_log_bin;" \
    | tr '\n' '|')
expect_value \
    "mysql sql_log_bin accepts boolean session SET forms" \
    "1|0|1|1|" \
    "$boolean_form_values"

expect_error \
    "global sql_log_bin scope is session-only" \
    1238 \
    HY000 \
    "Variable 'sql_log_bin' is a SESSION variable" \
    "SELECT @@global.sql_log_bin;"

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@sql_log_bin, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_log_bin variable reads and clears warning diagnostics" \
    "1	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@sql_log_bin, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_log_bin variable reads and clears error diagnostics" \
    "1	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "unknown unscoped sql_log_bin variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_log_bin_variable'" \
    "SELECT @@no_such_sql_log_bin_variable;"

expect_error \
    "unknown scoped sql_log_bin variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_log_bin_variable'" \
    "SELECT @@session.no_such_sql_log_bin_variable;"

expect_error \
    "quoted sql_log_bin variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.sql_log_bin;"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "2" \
    "SELECT @@sql_log_bin + 1;"
