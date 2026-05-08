#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_show_errors_diagnostics_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

error_rows() {
    sql=$1
    run_mysql "$sql" --force 2>/dev/null | awk -F '\t' '$1 == "Error" { print }'
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expected_warning="Warning	1287	'INFORMATION_SCHEMA.PROCESSLIST' is deprecated and will be removed in a future release. Please use performance_schema.processlist instead"

headers=$(run_mysql_with_headers "BAD SQL; SHOW ERRORS;" --force 2>/dev/null | awk -F '\t' '$1 == "Level" && $2 == "Code" && $3 == "Message" { print; exit }')
expect_value "show errors headers" "Level	Code	Message" "$headers"

count_header=$(run_mysql_with_headers "SELECT 1; SHOW COUNT(*) ERRORS;" | tail -n 2 | head -n 1)
expect_value "show count errors header" "@@session.error_count" "$count_header"

empty_count=$(run_mysql "SELECT 1; SHOW COUNT(*) ERRORS; SELECT @@error_count, ROW_COUNT();" | tail -n 2 | tr '\n' '|')
expect_value "empty diagnostics count and row count" "0|0	-1|" "$empty_count"

expect_value \
    "warning-only diagnostics have no error rows" \
    "" \
    "$(error_rows "SELECT 1; SHOW PROCESSLIST; SHOW ERRORS;")"
warning_only_counts=$(run_mysql "SELECT 1; SHOW PROCESSLIST; SHOW COUNT(*) ERRORS; SHOW COUNT(*) WARNINGS;" | tail -n 2 | tr '\n' '|')
expect_value "warning-only counts" "0|1|" "$warning_only_counts"
warning_after_errors=$(run_mysql "SELECT 1; SHOW PROCESSLIST; SHOW ERRORS; SHOW WARNINGS;" | awk -F '\t' '$1 == "Warning" { print }')
expect_value "show errors preserves warning diagnostics" "$expected_warning" "$warning_after_errors"

set +e
parse_error_output=$(printf '%s\n' 'BAD SQL; SHOW ERRORS; SHOW COUNT(*) ERRORS; SHOW WARNINGS; SHOW COUNT(*) WARNINGS; SELECT @@error_count, @@warning_count, ROW_COUNT();' \
    | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names --force 2>&1)
parse_error_status=$?
set -e
if [ "$parse_error_status" -ne 0 ]; then
    fail "forced parse-error diagnostics probe failed with status $parse_error_status: [$parse_error_output]"
fi
case "$parse_error_output" in
    *"ERROR 1064 (42000)"*"Error	1064"*"BAD SQL"*"1"*"Error	1064"*"1"*"1	1	-1"*) ;;
    *) fail "parse-error diagnostics output did not expose one error row/count: [$parse_error_output]" ;;
esac

chained=$(printf '%s\n' 'BAD SQL; SHOW ERRORS; SHOW COUNT(*) ERRORS; SHOW ERRORS; SELECT @@error_count, ROW_COUNT();' \
    | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names --force 2>/dev/null \
    | awk -F '\t' '$1 == "Error" || $1 == "1" || $1 == "1\t-1" { print }' \
    | tr '\n' '|')
case "$chained" in
    *"Error	1064"*"|1|Error	1064"*"|1	-1|"*) ;;
    *) fail "diagnostic statements did not preserve error list: [$chained]" ;;
esac

cleared=$(printf '%s\n' 'BAD SQL; SELECT 1; SHOW COUNT(*) ERRORS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names --force 2>/dev/null \
    | tail -n 1)
expect_value "ordinary statement clears error list" "0" "$cleared"

expect_value \
    "limit zero displays no error rows" \
    "" \
    "$(error_rows "BAD SQL; SHOW ERRORS LIMIT 0;")"
expect_value \
    "limit one displays error row" \
    1 \
    "$(error_rows "BAD SQL; SHOW ERRORS LIMIT 1;" | wc -l | tr -d ' ')"
expect_value \
    "limit comma offset displays error row" \
    1 \
    "$(error_rows "BAD SQL; SHOW ERRORS LIMIT 0,1;" | wc -l | tr -d ' ')"
expect_value \
    "limit comma skips error row" \
    "" \
    "$(error_rows "BAD SQL; SHOW ERRORS LIMIT 1,1;")"
expect_value \
    "limit offset displays error row" \
    1 \
    "$(error_rows "BAD SQL; SHOW ERRORS LIMIT 1 OFFSET 0;" | wc -l | tr -d ' ')"
expect_value \
    "limit offset skips error row" \
    "" \
    "$(error_rows "BAD SQL; SHOW ERRORS LIMIT 1 OFFSET 1;")"
expect_value \
    "uint64 max limit displays error row" \
    1 \
    "$(error_rows "BAD SQL; SHOW ERRORS LIMIT 18446744073709551615;" | wc -l | tr -d ' ')"

expect_error \
    "show count errors space before paren" \
    1064 \
    42000 \
    "COUNT (*) ERRORS" \
    "SELECT 1; SHOW COUNT (*) ERRORS;"
expect_error \
    "show errors signed positive limit" \
    1064 \
    42000 \
    "+1" \
    "SELECT 1; SHOW ERRORS LIMIT +1;"
expect_error \
    "show errors signed negative limit" \
    1064 \
    42000 \
    "-1" \
    "SELECT 1; SHOW ERRORS LIMIT -1;"
expect_error \
    "show errors too large limit" \
    1064 \
    42000 \
    "18446744073709551616" \
    "SELECT 1; SHOW ERRORS LIMIT 18446744073709551616;"
expect_error \
    "show errors like unsupported" \
    1064 \
    42000 \
    "LIKE 'x'" \
    "SELECT 1; SHOW ERRORS LIKE 'x';"
expect_error \
    "show errors where unsupported" \
    1064 \
    42000 \
    "WHERE" \
    "SELECT 1; SHOW ERRORS WHERE Code = 1064;"
