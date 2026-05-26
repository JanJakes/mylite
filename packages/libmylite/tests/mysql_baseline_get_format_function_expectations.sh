#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_get_format_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_get_format_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw "$@"
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

date_expected=$(cat <<EXPECTED
%m.%d.%Y	%Y-%m-%d	%Y-%m-%d	%d.%m.%Y	%Y%m%d
EXPECTED
)
expect_output \
    "DATE format mappings" \
    "$date_expected" \
    "SELECT GET_FORMAT(DATE,'USA'), GET_FORMAT(DATE,'JIS'), "\
"GET_FORMAT(DATE,'ISO'), GET_FORMAT(DATE,'EUR'), GET_FORMAT(DATE,'INTERNAL');" \
    "$DATABASE"

time_expected=$(cat <<EXPECTED
%h:%i:%s %p	%H:%i:%s	%H:%i:%s	%H.%i.%s	%H%i%s
EXPECTED
)
expect_output \
    "TIME format mappings" \
    "$time_expected" \
    "SELECT GET_FORMAT(TIME,'USA'), GET_FORMAT(TIME,'JIS'), "\
"GET_FORMAT(TIME,'ISO'), GET_FORMAT(TIME,'EUR'), GET_FORMAT(TIME,'INTERNAL');" \
    "$DATABASE"

datetime_expected=$(cat <<EXPECTED
%Y-%m-%d %H.%i.%s	%Y-%m-%d %H:%i:%s	%Y-%m-%d %H:%i:%s	%Y-%m-%d %H.%i.%s	%Y%m%d%H%i%s
%Y-%m-%d %H.%i.%s	%Y-%m-%d %H:%i:%s	%Y-%m-%d %H:%i:%s	%Y-%m-%d %H.%i.%s	%Y%m%d%H%i%s
EXPECTED
)
expect_output \
    "DATETIME and TIMESTAMP format mappings" \
    "$datetime_expected" \
    "SELECT GET_FORMAT(DATETIME,'USA'), GET_FORMAT(DATETIME,'JIS'), "\
"GET_FORMAT(DATETIME,'ISO'), GET_FORMAT(DATETIME,'EUR'), GET_FORMAT(DATETIME,'INTERNAL'); "\
"SELECT GET_FORMAT(TIMESTAMP,'USA'), GET_FORMAT(TIMESTAMP,'JIS'), "\
"GET_FORMAT(TIMESTAMP,'ISO'), GET_FORMAT(TIMESTAMP,'EUR'), GET_FORMAT(TIMESTAMP,'INTERNAL');" \
    "$DATABASE"

case_expected=$(cat <<EXPECTED
%m.%d.%Y	%m.%d.%Y	%Y%m%d%H%i%s
EXPECTED
)
expect_output \
    "case-insensitive classes and format names" \
    "$case_expected" \
    "SELECT GET_FORMAT(date,'usa'), GET_FORMAT(DaTe,'UsA'), "\
"GET_FORMAT(timestamp,'internal');" \
    "$DATABASE"

null_unknown_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL	NULL	NULL
0
EXPECTED
)
expect_output \
    "NULL unknown numeric and boolean formats" \
    "$null_unknown_expected" \
    "SELECT GET_FORMAT(DATE,NULL), GET_FORMAT(TIME,'bogus'), GET_FORMAT(DATETIME,123), "\
"GET_FORMAT(DATE,-1), GET_FORMAT(DATE,TRUE), GET_FORMAT(DATE,FALSE); "\
"SELECT @@warning_count;" \
    "$DATABASE"

consumer_expected=$(cat <<EXPECTED
2008-01-02	13:29:17	2008-01-02 13:29:17
EXPECTED
)
expect_output \
    "GET_FORMAT as temporal formatter input" \
    "$consumer_expected" \
    "SELECT DATE_FORMAT('2008-01-02', GET_FORMAT(DATE,'ISO')), "\
"TIME_FORMAT('13:29:17', GET_FORMAT(TIME,'ISO')), "\
"STR_TO_DATE('2008-01-02 13:29:17', GET_FORMAT(DATETIME,'ISO'));" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	%m.%d.%Y
2	%m.%d.%Y
EXPECTED
)
expect_output \
    "row-backed constant projection" \
    "$table_expected" \
    "DROP TABLE IF EXISTS t; CREATE TABLE t(id INT); INSERT INTO t VALUES (2), (1); "\
"SELECT id, GET_FORMAT(DATE,'USA') FROM t ORDER BY id;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
GET_FORMAT(DATE,'USA')	fmt
%m.%d.%Y	%H:%i:%s
EXPECTED
)
expect_output_with_headers \
    "labels" \
    "$labels_expected" \
    "SELECT GET_FORMAT(DATE,'USA'), GET_FORMAT(TIME,'ISO') AS fmt FROM DUAL;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO status" \
    "$do_expected" \
    "DO GET_FORMAT(DATE,'USA'); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

identifier_expected=$(cat <<EXPECTED
get_format
get_format
EXPECTED
)
expect_output \
    "GET_FORMAT identifier remains nonreserved" \
    "$identifier_expected" \
    "SET SESSION sql_mode = ''; "\
"DROP TABLE IF EXISTS get_format; CREATE TABLE get_format(id INT); "\
"SHOW TABLES LIKE 'get_format'; DROP TABLE get_format; "\
"SET SESSION sql_mode = 'IGNORE_SPACE'; CREATE TABLE get_format(id INT); "\
"SHOW TABLES LIKE 'get_format'; DROP TABLE get_format;" \
    "$DATABASE"

expect_error \
    "empty argument list syntax" \
    1064 \
    42000 \
    "near ')' at line 1" \
    "SELECT GET_FORMAT();" \
    "$DATABASE"

expect_error \
    "one argument syntax" \
    1064 \
    42000 \
    "near ')' at line 1" \
    "SELECT GET_FORMAT(DATE);" \
    "$DATABASE"

expect_error \
    "too many arguments syntax" \
    1064 \
    42000 \
    "near ','extra')' at line 1" \
    "SELECT GET_FORMAT(DATE,'USA','extra');" \
    "$DATABASE"

expect_error \
    "invalid class syntax" \
    1064 \
    42000 \
    "near 'YEAR,'USA')' at line 1" \
    "SELECT GET_FORMAT(YEAR,'USA');" \
    "$DATABASE"

expect_error \
    "string first argument syntax" \
    1064 \
    42000 \
    "near ''DATE','USA')' at line 1" \
    "SELECT GET_FORMAT('DATE','USA');" \
    "$DATABASE"

expect_upstream_accepts \
    "expression format argument deferred by MyLite" \
    "SELECT GET_FORMAT(DATE, CONCAT('U','SA'));" \
    "$DATABASE"
