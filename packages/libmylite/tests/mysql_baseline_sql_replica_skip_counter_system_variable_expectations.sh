#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"

fail() {
    printf '%s\n' "mysql_baseline_sql_replica_skip_counter_system_variable_expectations: $1" >&2
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

expected_values="0	0	0	0	-1"
values=$(run_mysql \
    "SELECT 1; SELECT @@sql_replica_skip_counter, @@global.sql_replica_skip_counter, \
     @@warning_count, @@error_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "sql_replica_skip_counter variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@sql_replica_skip_counter	@@global.sql_replica_skip_counter	@@global.\`sql_replica_skip_counter\`	@@\`sql_replica_skip_counter\`
0	0	0	0
EOF
)
expect_output_with_headers \
    "sql_replica_skip_counter labels preserve source text" \
    "$expected_headers" \
    "SELECT @@sql_replica_skip_counter, @@global.sql_replica_skip_counter, \
     @@global.\`sql_replica_skip_counter\`, @@\`sql_replica_skip_counter\`;"

expect_output \
    "case-insensitive sql_replica_skip_counter variables" \
    "0	0" \
    "SELECT @@SQL_REPLICA_SKIP_COUNTER, @@Global.Sql_Replica_Skip_Counter;"

expect_output \
    "from dual returns sql_replica_skip_counter" \
    "0" \
    "SELECT @@sql_replica_skip_counter FROM DUAL;"

mutable_values=$(run_mysql \
    "SET GLOBAL sql_replica_skip_counter=1; \
     SELECT @@sql_replica_skip_counter, @@global.sql_replica_skip_counter, \
            @@warning_count, @@error_count, ROW_COUNT(); \
     SET GLOBAL sql_replica_skip_counter=0;" \
    | tail -n 1)
expect_value \
    "mysql global sql_replica_skip_counter is mutable upstream" \
    "1	1	0	0	0" \
    "$mutable_values"

fixed_assignment_values=$(run_mysql \
    "SET GLOBAL sql_replica_skip_counter=0; \
     SELECT @@sql_replica_skip_counter, @@warning_count, @@error_count, ROW_COUNT(); \
     SET GLOBAL sql_replica_skip_counter=DEFAULT; \
     SELECT @@sql_replica_skip_counter, @@warning_count, @@error_count, ROW_COUNT(); \
     SET @@global.sql_replica_skip_counter=0; \
     SELECT @@sql_replica_skip_counter, @@warning_count, @@error_count, ROW_COUNT();" \
    | tail -n 3 \
    | tr '\n' '|')
expect_value \
    "mysql accepts fixed sql_replica_skip_counter no-op global assignments" \
    "0	0	0	0|0	0	0	0|0	0	0	0|" \
    "$fixed_assignment_values"

expect_error \
    "unscoped sql_replica_skip_counter SET requires SET GLOBAL upstream" \
    1229 \
    HY000 \
    "Variable 'sql_replica_skip_counter' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET sql_replica_skip_counter=0;"

expect_error \
    "session sql_replica_skip_counter scope is rejected upstream" \
    1238 \
    HY000 \
    "Variable 'sql_replica_skip_counter' is a GLOBAL variable" \
    "SELECT @@session.sql_replica_skip_counter;"

expect_error \
    "local sql_replica_skip_counter scope is rejected upstream" \
    1238 \
    HY000 \
    "Variable 'sql_replica_skip_counter' is a GLOBAL variable" \
    "SELECT @@local.sql_replica_skip_counter;"

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@sql_replica_skip_counter, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_replica_skip_counter variable reads and clears warning diagnostics" \
    "0	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@sql_replica_skip_counter, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "sql_replica_skip_counter variable reads and clears error diagnostics" \
    "0	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "unknown unscoped sql_replica_skip_counter variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_replica_skip_counter_variable'" \
    "SELECT @@no_such_sql_replica_skip_counter_variable;"

expect_error \
    "unknown scoped sql_replica_skip_counter variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_sql_replica_skip_counter_variable'" \
    "SELECT @@global.no_such_sql_replica_skip_counter_variable;"

expect_error \
    "quoted sql_replica_skip_counter variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.sql_replica_skip_counter;"

alias_values=$(run_mysql \
    "SET GLOBAL sql_replica_skip_counter=0; \
     SELECT @@sql_slave_skip_counter; \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "mysql deprecated sql_slave_skip_counter alias emits warning outside mylite slice" \
    "0|1|" \
    "$alias_values"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "1" \
    "SELECT @@sql_replica_skip_counter + 1;"
