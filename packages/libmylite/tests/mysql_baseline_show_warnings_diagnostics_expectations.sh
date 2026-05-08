#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_show_warnings_diagnostics_expectations: $1" >&2
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

warning_rows() {
    sql=$1
    run_mysql "$sql" | awk -F '\t' '$1 == "Warning" || $1 == "Error" { print }'
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expected_warning="Warning	1287	'INFORMATION_SCHEMA.PROCESSLIST' is deprecated and will be removed in a future release. Please use performance_schema.processlist instead"

headers=$(run_mysql_with_headers "SELECT 1; SHOW PROCESSLIST; SHOW WARNINGS;" | awk -F '\t' '$1 == "Level" && $2 == "Code" && $3 == "Message" { print; exit }')
expect_value "show warnings headers" "Level	Code	Message" "$headers"

count_header=$(run_mysql_with_headers "SELECT 1; SHOW COUNT(*) WARNINGS;" | tail -n 2 | head -n 1)
expect_value "show count warnings header" "@@session.warning_count" "$count_header"

empty_count=$(run_mysql "SELECT 1; SHOW COUNT(*) WARNINGS; SELECT @@warning_count, ROW_COUNT();" | tail -n 2 | tr '\n' '|')
expect_value "empty diagnostics count and row count" "0|0	-1|" "$empty_count"

warning=$(warning_rows "SELECT 1; SHOW PROCESSLIST; SHOW WARNINGS;")
expect_value "processlist warning row" "$expected_warning" "$warning"

chained=$(run_mysql "SELECT 1; SHOW PROCESSLIST; SHOW WARNINGS; SHOW COUNT(*) WARNINGS; SHOW WARNINGS; SELECT @@warning_count, ROW_COUNT();" | tail -n 3 | tr '\n' '|')
expect_value "diagnostic statements preserve warning list" "1|$expected_warning|1	-1|" "$chained"

cleared=$(run_mysql "SELECT 1; SHOW PROCESSLIST; SELECT 1; SHOW COUNT(*) WARNINGS;" | tail -n 1)
expect_value "ordinary statement clears warning list" "0" "$cleared"

expect_value \
    "limit zero displays no warning rows" \
    "" \
    "$(warning_rows "SELECT 1; SHOW PROCESSLIST; SHOW WARNINGS LIMIT 0;")"
expect_value \
    "limit one displays warning row" \
    "$expected_warning" \
    "$(warning_rows "SELECT 1; SHOW PROCESSLIST; SHOW WARNINGS LIMIT 1;")"
expect_value \
    "limit comma offset displays warning row" \
    "$expected_warning" \
    "$(warning_rows "SELECT 1; SHOW PROCESSLIST; SHOW WARNINGS LIMIT 0,1;")"
expect_value \
    "limit comma skips warning row" \
    "" \
    "$(warning_rows "SELECT 1; SHOW PROCESSLIST; SHOW WARNINGS LIMIT 1,1;")"
expect_value \
    "limit offset displays warning row" \
    "$expected_warning" \
    "$(warning_rows "SELECT 1; SHOW PROCESSLIST; SHOW WARNINGS LIMIT 1 OFFSET 0;")"
expect_value \
    "limit offset skips warning row" \
    "" \
    "$(warning_rows "SELECT 1; SHOW PROCESSLIST; SHOW WARNINGS LIMIT 1 OFFSET 1;")"
expect_value \
    "uint64 max limit displays warning row" \
    "$expected_warning" \
    "$(warning_rows "SELECT 1; SHOW PROCESSLIST; SHOW WARNINGS LIMIT 18446744073709551615;")"

set +e
parse_error_output=$(printf '%s\n' 'BAD SQL; SHOW WARNINGS; SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names --force 2>&1)
parse_error_status=$?
set -e
if [ "$parse_error_status" -ne 0 ]; then
    fail "forced parse-error diagnostics probe failed with status $parse_error_status: [$parse_error_output]"
fi
case "$parse_error_output" in
    *"ERROR 1064 (42000)"*"Error	1064"*"BAD SQL"*"1"*) ;;
    *) fail "parse-error diagnostics output did not expose one error row: [$parse_error_output]" ;;
esac

expect_error \
    "show count warnings space before paren" \
    1064 \
    42000 \
    "COUNT (*) WARNINGS" \
    "SELECT 1; SHOW COUNT (*) WARNINGS;"
expect_error \
    "show warnings signed positive limit" \
    1064 \
    42000 \
    "+1" \
    "SELECT 1; SHOW PROCESSLIST; SHOW WARNINGS LIMIT +1;"
expect_error \
    "show warnings signed negative limit" \
    1064 \
    42000 \
    "-1" \
    "SELECT 1; SHOW PROCESSLIST; SHOW WARNINGS LIMIT -1;"
expect_error \
    "show warnings too large limit" \
    1064 \
    42000 \
    "18446744073709551616" \
    "SELECT 1; SHOW PROCESSLIST; SHOW WARNINGS LIMIT 18446744073709551616;"
expect_error \
    "show warnings like unsupported" \
    1064 \
    42000 \
    "LIKE 'x'" \
    "SELECT 1; SHOW WARNINGS LIKE 'x';"
expect_error \
    "show warnings where unsupported" \
    1064 \
    42000 \
    "WHERE" \
    "SELECT 1; SHOW WARNINGS WHERE Code = 1287;"
