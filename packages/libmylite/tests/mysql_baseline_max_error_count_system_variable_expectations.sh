#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"
MYSQL_DATABASE="mylite_max_error_count_expectations"

fail() {
    printf '%s\n' "mysql_baseline_max_error_count_system_variable_expectations: $1" >&2
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    expect_value "$label" "$expected" "$output"
}

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
    expect_value "$label" "$expected" "$output"
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

run_mysql "DROP DATABASE IF EXISTS $MYSQL_DATABASE; CREATE DATABASE $MYSQL_DATABASE;"

expect_output \
    "default scalar values" \
    "1024	1024	1024	1024" \
    "SELECT @@max_error_count, @@session.max_error_count, \
            @@local.max_error_count, @@global.max_error_count;"

expected_show=$(cat <<EOF
Variable_name	Value
max_error_count	1024
EOF
)
expect_output_with_headers \
    "SHOW VARIABLES max_error_count" \
    "$expected_show" \
    "SHOW VARIABLES LIKE 'max_error_count';"

expect_output_with_headers \
    "SHOW GLOBAL VARIABLES max_error_count" \
    "$expected_show" \
    "SHOW GLOBAL VARIABLES LIKE 'max_error_count';"

session_values=$(run_mysql \
    "SET SESSION max_error_count = 2; \
     SELECT @@max_error_count, @@session.max_error_count, @@global.max_error_count; \
     SET max_error_count = DEFAULT; \
     SELECT @@max_error_count;")
expect_value \
    "session mutation and default" \
    "2	2	1024
1024" \
    "$session_values"

clamp_negative=$(run_mysql \
    "SET SESSION max_error_count = DEFAULT; \
     SET max_error_count = -1; \
     SHOW COUNT(*) WARNINGS; \
     SHOW WARNINGS; \
     SELECT @@max_error_count, @@warning_count;")
expect_value \
    "negative clamps to zero" \
    "1
Warning	1292	Truncated incorrect max_error_count value: '-1'
0	1" \
    "$clamp_negative"

clamp_high=$(run_mysql \
    "SET SESSION max_error_count = DEFAULT; \
     SET max_error_count = 65536; \
     SHOW COUNT(*) WARNINGS; \
     SHOW WARNINGS; \
     SELECT @@max_error_count, @@warning_count;")
expect_value \
    "large value clamps to max" \
    "1
Warning	1292	Truncated incorrect max_error_count value: '65536'
65535	1" \
    "$clamp_high"

expect_output \
    "booleans assign integers" \
    "1	0" \
    "SET max_error_count = TRUE; SET @true_value = @@max_error_count; \
     SET max_error_count = FALSE; SELECT @true_value, @@max_error_count;"

expect_error \
    "quoted value has incorrect type" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'max_error_count'" \
    "SET max_error_count = '7';"

expect_error \
    "NULL value has incorrect type" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'max_error_count'" \
    "SET max_error_count = NULL;"

capped_rows=$(run_mysql \
    "USE $MYSQL_DATABASE; \
     SET SESSION max_error_count = 1; \
     DROP TABLE IF EXISTS a, b, c; \
     SHOW COUNT(*) WARNINGS; \
     SHOW WARNINGS; \
     SELECT @@warning_count, @@error_count, @@max_error_count;")
expect_value \
    "retained warning rows are capped" \
    "3
Note	1051	Unknown table '$MYSQL_DATABASE.a'
3	0	1" \
    "$capped_rows"

zero_rows=$(run_mysql \
    "USE $MYSQL_DATABASE; \
     SET SESSION max_error_count = 0; \
     DROP TABLE IF EXISTS a, b, c; \
     SHOW COUNT(*) WARNINGS; \
     SHOW WARNINGS; \
     SELECT @@warning_count, @@error_count, @@max_error_count;")
expect_value \
    "zero cap keeps count but no rows for notes" \
    "3
3	0	0" \
    "$zero_rows"

parse_zero=$(printf '%s\n' \
    'SET SESSION max_error_count = 0; BAD SQL; SELECT @@warning_count, @@error_count; SHOW COUNT(*) WARNINGS; SHOW WARNINGS; SHOW COUNT(*) ERRORS; SHOW ERRORS; SET SESSION max_error_count = DEFAULT;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null)
expect_value \
    "zero cap suppresses retained parse error diagnostics" \
    "0	0
0
0" \
    "$parse_zero"

notes_suppressed=$(run_mysql \
    "USE $MYSQL_DATABASE; \
     SET SESSION max_error_count = 5; \
     SET SESSION sql_notes = 0; \
     DROP TABLE IF EXISTS suppressed_note; \
     SHOW COUNT(*) WARNINGS; \
     SHOW WARNINGS; \
     SELECT @@sql_notes, @@warning_count, @@max_error_count; \
     SET SESSION sql_notes = DEFAULT;")
expect_value \
    "sql_notes disables notes before cap" \
    "0
0	0	5" \
    "$notes_suppressed"

run_mysql "DROP DATABASE IF EXISTS $MYSQL_DATABASE; SET SESSION max_error_count = DEFAULT;"

printf '%s\n' "mysql_baseline_max_error_count_system_variable_expectations: ok"
