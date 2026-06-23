#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_diagnostics_count_variables_expectations: $1" >&2
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

count_headers=$(run_mysql_with_headers "SELECT 1; SELECT @@warning_count, @@session.warning_count, @@local.warning_count, @@error_count, @@session.error_count, @@local.error_count, ROW_COUNT();" | tail -n 2 | head -n 1)
expect_value \
    "diagnostics count variable headers" \
    "@@warning_count	@@session.warning_count	@@local.warning_count	@@error_count	@@session.error_count	@@local.error_count	ROW_COUNT()" \
    "$count_headers"

count_values=$(run_mysql "SELECT 1; SELECT @@warning_count, @@session.warning_count, @@local.warning_count, @@error_count, @@session.error_count, @@local.error_count, ROW_COUNT();" | tail -n 1)
expect_value "empty diagnostics count values" "0	0	0	0	0	0	-1" "$count_values"

case_values=$(run_mysql "SELECT 1; SELECT @@WARNING_COUNT, @@SESSION.ERROR_COUNT, @@Local.Warning_Count;" | tail -n 1)
expect_value "case-insensitive diagnostics count variables" "0	0	0" "$case_values"

quoted_headers=$(run_mysql_with_headers "SELECT 1; SELECT (@@warning_count), @@session.\`warning_count\`, @@\`error_count\`;" | tail -n 2 | head -n 1)
expect_value \
    "quoted diagnostics count variable headers" \
    "(@@warning_count)	@@session.\`warning_count\`	@@\`error_count\`" \
    "$quoted_headers"
quoted_values=$(run_mysql "SELECT 1; SELECT (@@warning_count), @@session.\`warning_count\`, @@\`error_count\`;" | tail -n 1)
expect_value "quoted diagnostics count variable values" "0	0	0" "$quoted_values"

expression_values=$(run_mysql "SELECT 1; SELECT @@warning_count + 1, @@error_count + 2;" | tail -n 1)
expect_value "diagnostics count expression values" "1	2" "$expression_values"

warning_then_scalar=$(run_mysql "SELECT 1; SHOW PROCESSLIST; SELECT @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;" | tail -n 2 | tr '\n' '|')
expect_value "scalar warning_count reads and clears warning diagnostics" "1	0	-1|0|" "$warning_then_scalar"

diagnostic_show_then_scalar=$(printf '%s\n' 'BAD SQL; SHOW COUNT(*) ERRORS; SHOW COUNT(*) WARNINGS; SELECT @@error_count, @@warning_count, ROW_COUNT(); SHOW COUNT(*) ERRORS; SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names --force 2>/dev/null \
    | tail -n 5 \
    | tr '\n' '|')
expect_value \
    "diagnostic show counts preserve until scalar count select clears" \
    "1|1|1	1	-1|0|0|" \
    "$diagnostic_show_then_scalar"

parse_error_scalar=$(printf '%s\n' 'BAD SQL; SELECT @@error_count, @@warning_count, ROW_COUNT(); SHOW COUNT(*) ERRORS; SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names --force 2>/dev/null \
    | tail -n 3 \
    | tr '\n' '|')
expect_value "parse error count variables clear diagnostics" "1	1	-1|0|0|" "$parse_error_scalar"

expect_error \
    "global warning_count rejected as session variable" \
    1238 \
    HY000 \
    "SESSION variable" \
    "SELECT 1; SELECT @@global.warning_count;"
expect_error \
    "global error_count rejected as session variable" \
    1238 \
    HY000 \
    "SESSION variable" \
    "SELECT 1; SELECT @@global.error_count;"
expect_error \
    "unknown system variable rejected" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_mylite_variable'" \
    "SELECT 1; SELECT @@no_such_mylite_variable;"
expect_error \
    "mixed-case local unknown system variable reports final name" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_mylite_variable'" \
    "SELECT 1; SELECT @@Local.no_such_mylite_variable;"
expect_error \
    "mixed-case session unknown system variable reports final name" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_mylite_variable'" \
    "SELECT 1; SELECT @@SESSION.no_such_mylite_variable;"
expect_error \
    "mixed-case global unknown system variable reports final name" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_mylite_variable'" \
    "SELECT 1; SELECT @@Global.no_such_mylite_variable;"
expect_error \
    "quoted scoped unknown system variable reports final name" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_mylite_variable'" \
    "SELECT 1; SELECT @@session.\`no_such_mylite_variable\`;"
expect_error \
    "quoted unknown system variable reports unquoted name" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_mylite_variable'" \
    "SELECT 1; SELECT @@\`no_such_mylite_variable\`;"
expect_error \
    "quoted scope rejected by MySQL parser" \
    1064 \
    42000 \
    "\`session\`.warning_count" \
    "SELECT 1; SELECT @@\`session\`.warning_count;"
