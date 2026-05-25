#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_temporal_constructor_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_temporal_constructor_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
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

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; SET SESSION sql_mode = '';" >/dev/null

from_days_expected=$(cat <<EXPECTED
NULL	0000-00-00	0000-00-00	0000-00-00	0001-01-01	2000-07-03	2007-10-07	9999-12-31	NULL	NULL	0000-00-00	0000-00-00	0000-00-00
Warning	1441	Datetime function: from_days field overflow
Warning	1441	Datetime function: from_days field overflow
EXPECTED
)
expect_output \
    "FROM_DAYS values and warnings" \
    "$from_days_expected" \
    "SELECT FROM_DAYS(NULL), FROM_DAYS(-1), FROM_DAYS(0), FROM_DAYS(365), "\
"FROM_DAYS(366), FROM_DAYS(730669), FROM_DAYS(733321), FROM_DAYS(3652424), "\
"FROM_DAYS(3652425), FROM_DAYS(3652499), FROM_DAYS(3652500), FROM_DAYS(TRUE), "\
"FROM_DAYS(FALSE); SHOW WARNINGS;" \
    "$DATABASE"

makedate_expected=$(cat <<EXPECTED
NULL	NULL	2024-01-01	2024-02-29	2024-12-31	2024-01-01	2000-01-01	2001-01-01	2069-01-01	1970-01-01	1999-01-01	0100-01-01	9999-12-31	NULL
0
EXPECTED
)
expect_output \
    "MAKEDATE values" \
    "$makedate_expected" \
    "SELECT MAKEDATE(NULL, 1), MAKEDATE(2024, NULL), MAKEDATE(2024, 1), "\
"MAKEDATE(2024, 60), MAKEDATE(2024, 366), MAKEDATE(2023, 366), "\
"MAKEDATE(0, 1), MAKEDATE(1, 1), MAKEDATE(69, 1), MAKEDATE(70, 1), "\
"MAKEDATE(99, 1), MAKEDATE(100, 1), MAKEDATE(9999, 365), "\
"MAKEDATE(9999, 366); SELECT @@warning_count;" \
    "$DATABASE"

maketime_expected=$(cat <<EXPECTED
NULL	NULL	NULL	01:02:03	-01:02:03	838:59:59	838:59:59	-838:59:59	00:00:00	01:01:01
Warning	1292	Truncated incorrect time value: '839:00:00'
Warning	1292	Truncated incorrect time value: '-839:00:00'
EXPECTED
)
expect_output \
    "MAKETIME values and warnings" \
    "$maketime_expected" \
    "SELECT MAKETIME(NULL, 1, 2), MAKETIME(1, NULL, 2), MAKETIME(1, 2, NULL), "\
"MAKETIME(1, 2, 3), MAKETIME(-1, 2, 3), MAKETIME(838, 59, 59), "\
"MAKETIME(839, 0, 0), MAKETIME(-839, 0, 0), MAKETIME(FALSE, FALSE, FALSE), "\
"MAKETIME(TRUE, TRUE, TRUE); SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "DO row count" \
    "0	0" \
    "DO FROM_DAYS(366), MAKEDATE(2024, 1), MAKETIME(1, 2, 3); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "FROM_DAYS empty argument count" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'FROM_DAYS'" \
    "SELECT FROM_DAYS() AS x;" \
    "$DATABASE"
expect_error \
    "FROM_DAYS extra argument count" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'FROM_DAYS'" \
    "SELECT FROM_DAYS(1, 2) AS x;" \
    "$DATABASE"
expect_error \
    "MAKEDATE one argument count" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'MAKEDATE'" \
    "SELECT MAKEDATE(1) AS x;" \
    "$DATABASE"
expect_error \
    "MAKETIME two argument count" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'MAKETIME'" \
    "SELECT MAKETIME(1, 2) AS x;" \
    "$DATABASE"

expect_upstream_accepts \
    "string FROM_DAYS input deferred by MyLite" \
    "SELECT FROM_DAYS('366');" \
    "$DATABASE"
expect_upstream_accepts \
    "decimal MAKEDATE input deferred by MyLite" \
    "SELECT MAKEDATE(2024, 1.9);" \
    "$DATABASE"
expect_upstream_accepts \
    "fractional MAKETIME input deferred by MyLite" \
    "SELECT MAKETIME(1, 2, 3.5);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_temporal_constructor_functions_expectations: ok"
