#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"

fail() {
    printf '%s\n' "mysql_baseline_version_system_variables_expectations: $1" >&2
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

comment=$(run_mysql 'SELECT @@version_comment;')

headers=$(run_mysql_with_headers "SELECT 1; SELECT @@version, @@global.version, @@version_comment, @@global.version_comment, @@warning_count, ROW_COUNT();" | tail -n 2 | head -n 1)
expect_value \
    "version variable headers" \
    "@@version	@@global.version	@@version_comment	@@global.version_comment	@@warning_count	ROW_COUNT()" \
    "$headers"

values=$(run_mysql "SELECT 1; SELECT @@version, @@global.version, @@version_comment, @@global.version_comment, @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value \
    "version variable values" \
    "${version}	${version}	${comment}	${comment}	0	-1" \
    "$values"

case_values=$(run_mysql "SELECT 1; SELECT @@VERSION, @@GLOBAL.VERSION_COMMENT;" | tail -n 1)
expect_value \
    "version variable case-insensitive values" \
    "${version}	${comment}" \
    "$case_values"

quoted_headers=$(run_mysql_with_headers "SELECT 1; SELECT @@\`version\`, @@global.\`version_comment\`;" | tail -n 2 | head -n 1)
expect_value \
    "quoted version variable headers" \
    "@@\`version\`	@@global.\`version_comment\`" \
    "$quoted_headers"

quoted_values=$(run_mysql "SELECT 1; SELECT @@\`version\`, @@global.\`version_comment\`;" | tail -n 1)
expect_value \
    "quoted version variable values" \
    "${version}	${comment}" \
    "$quoted_values"

warning_then_scalar=$(run_mysql "SELECT 1; SHOW PROCESSLIST; SELECT @@version, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;" | tail -n 2 | tr '\n' '|')
expect_value \
    "version scalar select reads and clears warning diagnostics" \
    "${version}	1	0	-1|0|" \
    "$warning_then_scalar"

parse_error_scalar=$(printf '%s\n' 'BAD SQL; SELECT @@version, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "version scalar select reads and clears error diagnostics" \
    "${version}	1	1	-1|0|" \
    "$parse_error_scalar"

expect_error \
    "session version rejected as global variable" \
    1238 \
    HY000 \
    "GLOBAL variable" \
    "SELECT 1; SELECT @@session.version;"

expect_error \
    "local version rejected as global variable" \
    1238 \
    HY000 \
    "GLOBAL variable" \
    "SELECT 1; SELECT @@local.version;"

expect_error \
    "session version comment rejected as global variable" \
    1238 \
    HY000 \
    "GLOBAL variable" \
    "SELECT 1; SELECT @@session.version_comment;"

expect_error \
    "local version comment rejected as global variable" \
    1238 \
    HY000 \
    "GLOBAL variable" \
    "SELECT 1; SELECT @@local.version_comment;"

expect_error \
    "unknown unscoped version system variable rejected" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_version_variable'" \
    "SELECT 1; SELECT @@no_such_version_variable;"

expect_error \
    "unknown scoped version system variable reports final name" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_version_variable'" \
    "SELECT 1; SELECT @@session.no_such_version_variable;"

expect_error \
    "unknown global version system variable reports final name" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_version_variable'" \
    "SELECT 1; SELECT @@global.no_such_version_variable;"

expect_error \
    "quoted global scope rejected by MySQL parser" \
    1064 \
    42000 \
    "\`global\`.version" \
    "SELECT 1; SELECT @@\`global\`.version;"

coerced=$(run_mysql "SELECT 1; SELECT @@version + 1;" | tail -n 1)
expect_value \
    "mysql accepts expression outside this mylite slice" \
    "9.4" \
    "$coerced"
