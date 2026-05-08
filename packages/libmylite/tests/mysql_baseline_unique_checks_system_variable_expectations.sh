#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"

fail() {
    printf '%s\n' "mysql_baseline_unique_checks_system_variable_expectations: $1" >&2
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

expected_values="1	1	1	1	0	0	-1"
values=$(run_mysql \
    "SELECT 1; SELECT @@unique_checks, @@global.unique_checks, \
     @@session.unique_checks, @@local.unique_checks, \
     @@warning_count, @@error_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "unique_checks variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@unique_checks	@@global.unique_checks	@@session.\`unique_checks\`	@@\`unique_checks\`
1	1	1	1
EOF
)
expect_output_with_headers \
    "unique_checks labels preserve source text" \
    "$expected_headers" \
    "SELECT @@unique_checks, @@global.unique_checks, \
     @@session.\`unique_checks\`, @@\`unique_checks\`;"

expect_output \
    "case-insensitive unique_checks variables" \
    "1	1" \
    "SELECT @@UNIQUE_CHECKS, @@Global.Unique_Checks;"

expect_output \
    "from dual returns unique_checks" \
    "1" \
    "SELECT @@unique_checks FROM DUAL;"

mutable_values=$(run_mysql \
    "SELECT @@unique_checks, @@global.unique_checks; \
     SET SESSION unique_checks=0; \
     SELECT @@unique_checks, @@global.unique_checks, @@session.unique_checks, \
            @@local.unique_checks, @@warning_count, @@error_count, ROW_COUNT(); \
     SET SESSION unique_checks=1;" \
    | tail -n 1)
expect_value \
    "mysql session unique_checks is mutable upstream" \
    "0	1	0	0	0	0	0" \
    "$mutable_values"

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@unique_checks, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "unique_checks variable reads and clears warning diagnostics" \
    "1	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@unique_checks, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "unique_checks variable reads and clears error diagnostics" \
    "1	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "unknown unscoped unique_checks variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_unique_checks_variable'" \
    "SELECT @@no_such_unique_checks_variable;"

expect_error \
    "unknown scoped unique_checks variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_unique_checks_variable'" \
    "SELECT @@global.no_such_unique_checks_variable;"

expect_error \
    "quoted unique_checks variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.unique_checks;"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "2" \
    "SELECT @@unique_checks + 1;"
