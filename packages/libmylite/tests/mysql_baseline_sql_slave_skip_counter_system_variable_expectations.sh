#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"
DEPRECATION_WARNING="'@@sql_slave_skip_counter' is deprecated and will be removed in a future release. Please use sql_replica_skip_counter instead."

fail() {
    printf '%s\n' "mysql_baseline_sql_slave_skip_counter_system_variable_expectations: $1" >&2
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

run_mysql 'SET GLOBAL sql_replica_skip_counter=0;' >/dev/null

value_and_warning=$(run_mysql \
    "SELECT 1; \
     SELECT @@sql_slave_skip_counter; \
     SHOW COUNT(*) WARNINGS; \
     SHOW WARNINGS;" \
    | tail -n 3 \
    | tr '\n' '|')
expect_value \
    "sql_slave_skip_counter value and deprecation warning" \
    "0|1|Warning	1287	$DEPRECATION_WARNING|" \
    "$value_and_warning"

same_statement_counts=$(run_mysql \
    "SELECT 1; \
     SELECT @@warning_count, @@sql_slave_skip_counter, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_slave_skip_counter same-statement warning count is order-independent" \
    "1	0	0	-1|1|" \
    "$same_statement_counts"

multi_alias_counts=$(run_mysql \
    "SELECT 1; \
     SELECT @@sql_slave_skip_counter, @@warning_count, @@sql_slave_skip_counter, @@warning_count; \
     SHOW COUNT(*) WARNINGS; \
     SHOW WARNINGS;" \
    | tail -n 4 \
    | tr '\n' '|')
expect_value \
    "multiple sql_slave_skip_counter aliases warn once per reference" \
    "0	2	0	2|2|Warning	1287	$DEPRECATION_WARNING|Warning	1287	$DEPRECATION_WARNING|" \
    "$multi_alias_counts"

expected_headers=$(cat <<EOF
@@sql_slave_skip_counter	@@global.sql_slave_skip_counter	@@global.\`sql_slave_skip_counter\`	@@\`sql_slave_skip_counter\`
0	0	0	0
EOF
)
expect_output_with_headers \
    "sql_slave_skip_counter labels preserve source text" \
    "$expected_headers" \
    "SELECT @@sql_slave_skip_counter, @@global.sql_slave_skip_counter, \
     @@global.\`sql_slave_skip_counter\`, @@\`sql_slave_skip_counter\`;"

case_values=$(run_mysql \
    "SELECT 1; \
     SELECT @@SQL_SLAVE_SKIP_COUNTER, @@Global.Sql_Slave_Skip_Counter; \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "case-insensitive sql_slave_skip_counter variables warn twice" \
    "0	0|2|" \
    "$case_values"

from_dual_values=$(run_mysql \
    "SELECT 1; \
     SELECT @@sql_slave_skip_counter FROM DUAL; \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "from dual returns sql_slave_skip_counter and warns once" \
    "0|1|" \
    "$from_dual_values"

mutable_warning=$(run_mysql \
    "SET GLOBAL sql_slave_skip_counter=1; \
     SHOW COUNT(*) WARNINGS; \
     SHOW WARNINGS; \
     SELECT @@sql_replica_skip_counter; \
     SET GLOBAL sql_replica_skip_counter=0;" \
    | tail -n 3 \
    | tr '\n' '|')
expect_value \
    "mysql global sql_slave_skip_counter is mutable upstream with warning" \
    "1|Warning	1287	$DEPRECATION_WARNING|1|" \
    "$mutable_warning"

expect_error \
    "session sql_slave_skip_counter scope is rejected upstream" \
    1238 \
    HY000 \
    "Variable 'sql_slave_skip_counter' is a GLOBAL variable" \
    "SELECT @@session.sql_slave_skip_counter;"

expect_error \
    "local sql_slave_skip_counter scope is rejected upstream" \
    1238 \
    HY000 \
    "Variable 'sql_slave_skip_counter' is a GLOBAL variable" \
    "SELECT @@local.sql_slave_skip_counter;"

session_error_diagnostics=$(printf '%s\n' \
    'SELECT 1; SELECT @@session.sql_slave_skip_counter; SHOW COUNT(*) WARNINGS; SHOW WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "invalid sql_slave_skip_counter scope has only global-variable error diagnostic" \
    "1|Error	1238	Variable 'sql_slave_skip_counter' is a GLOBAL variable|" \
    "$session_error_diagnostics"

valid_alias_before_error=$(printf '%s\n' \
    'SELECT 1; SELECT @@sql_slave_skip_counter, @@session.sql_slave_skip_counter; SHOW COUNT(*) WARNINGS; SHOW WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 3 \
    | tr '\n' '|')
expect_value \
    "valid sql_slave_skip_counter alias before invalid scope stores warning and error" \
    "2|Warning	1287	$DEPRECATION_WARNING|Error	1238	Variable 'sql_slave_skip_counter' is a GLOBAL variable|" \
    "$valid_alias_before_error"

invalid_scope_before_alias=$(printf '%s\n' \
    'SELECT 1; SELECT @@session.sql_slave_skip_counter, @@sql_slave_skip_counter; SHOW COUNT(*) WARNINGS; SHOW WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "invalid sql_slave_skip_counter scope before valid alias stores only error" \
    "1|Error	1238	Variable 'sql_slave_skip_counter' is a GLOBAL variable|" \
    "$invalid_scope_before_alias"

warning_after_previous_error=$(printf '%s\n' \
    'BAD SQL; SELECT @@sql_slave_skip_counter, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS; SHOW WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 3 \
    | tr '\n' '|')
expect_value \
    "sql_slave_skip_counter warning replaces previous parse error diagnostics" \
    "0	1	0	-1|1|Warning	1287	$DEPRECATION_WARNING|" \
    "$warning_after_previous_error"

expect_error \
    "unknown unscoped sql_slave_skip_counter variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_slave_skip_counter_variable'" \
    "SELECT @@no_such_sql_slave_skip_counter_variable;"

expect_error \
    "unknown scoped sql_slave_skip_counter variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_slave_skip_counter_variable'" \
    "SELECT @@global.no_such_sql_slave_skip_counter_variable;"

expect_error \
    "quoted sql_slave_skip_counter variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.sql_slave_skip_counter;"

expression_warning=$(run_mysql \
    "SELECT 1; \
     SELECT @@sql_slave_skip_counter + 1; \
     SHOW COUNT(*) WARNINGS; \
     SHOW WARNINGS;" \
    | tail -n 3 \
    | tr '\n' '|')
expect_value \
    "mysql accepts expressions outside this mylite slice and warns" \
    "1|1|Warning	1287	$DEPRECATION_WARNING|" \
    "$expression_warning"

run_mysql 'SET GLOBAL sql_replica_skip_counter=0;' >/dev/null
