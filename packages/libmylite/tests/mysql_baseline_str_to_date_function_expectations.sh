#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_str_to_date_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_str_to_date_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; USE ${DATABASE}; SET NAMES utf8mb4; SET SESSION sql_mode = '';" >/dev/null

core_expected=$(cat <<EXPECTED
2013-05-01	2024-01-02 03:04:05	09:30:17	23:22:00	00:05:00	23:22:01	23:22:01	1	1	1	1
EXPECTED
)
expect_output \
    "core str_to_date values" \
    "$core_expected" \
    "SELECT STR_TO_DATE('01,5,2013','%d,%m,%Y'), "\
"STR_TO_DATE('2024-01-02 03:04:05','%Y-%m-%d %H:%i:%s'), "\
"STR_TO_DATE('09:30:17','%H:%i:%s'), STR_TO_DATE('11:22 PM','%h:%i %p'), "\
"STR_TO_DATE('12:05 AM','%h:%i %p'), STR_TO_DATE('23:22:01','%T'), "\
"STR_TO_DATE('11:22:01 PM','%r'), STR_TO_DATE(NULL,'%Y-%m-%d') IS NULL, "\
"STR_TO_DATE('2024-01-02',NULL) IS NULL, STR_TO_DATE(NULL,'%f') IS NULL, "\
"STR_TO_DATE(1,NULL) IS NULL;" \
    "$DATABASE"

width_expected=$(cat <<EXPECTED
2024-01-02	2024-01-02	2024-01-02	2069-01-02	1970-01-02	1999-01-02	2000-01-02	0123-02-03
EXPECTED
)
expect_output \
    "str_to_date numeric width values" \
    "$width_expected" \
    "SELECT STR_TO_DATE('2024-1-2','%Y-%m-%d'), STR_TO_DATE('20240102','%Y%m%d'), "\
"STR_TO_DATE('24-01-02','%y-%m-%d'), STR_TO_DATE('69-01-02','%y-%m-%d'), "\
"STR_TO_DATE('70-01-02','%y-%m-%d'), STR_TO_DATE('99-01-02','%Y-%m-%d'), "\
"STR_TO_DATE('0-01-02','%Y-%m-%d'), STR_TO_DATE('123-2-3','%Y-%m-%d');" \
    "$DATABASE"

trailing_expected=$(cat <<EXPECTED
2024-01-02	2024-01-02 03:04:05	09:30:17
Warning	1292	Truncated incorrect date value: '2024-01-02x'
Warning	1292	Truncated incorrect datetime value: '2024-01-02 03:04:05x'
Warning	1292	Truncated incorrect time value: '09:30:17a'
EXPECTED
)
expect_output \
    "str_to_date trailing warnings" \
    "$trailing_expected" \
    "SELECT STR_TO_DATE('2024-01-02x','%Y-%m-%d'), "\
"STR_TO_DATE('2024-01-02 03:04:05x','%Y-%m-%d %H:%i:%s'), "\
"STR_TO_DATE('09:30:17a','%H:%i:%s'); SHOW WARNINGS;" \
    "$DATABASE"

invalid_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL	NULL
Warning	1411	Incorrect datetime value: 'bad' for function str_to_date
Warning	1411	Incorrect datetime value: '2024-13-01' for function str_to_date
Warning	1411	Incorrect datetime value: '2024-02-31' for function str_to_date
Warning	1411	Incorrect datetime value: 'PM' for function str_to_date
Warning	1411	Incorrect datetime value: 'PM 11' for function str_to_date
EXPECTED
)
expect_output \
    "str_to_date invalid warnings" \
    "$invalid_expected" \
    "SELECT STR_TO_DATE('bad','%Y-%m-%d'), STR_TO_DATE('2024-13-01','%Y-%m-%d'), "\
"STR_TO_DATE('2024-02-31','%Y-%m-%d'), STR_TO_DATE('PM','%p'), "\
"STR_TO_DATE('PM 11','%p %h'); SHOW WARNINGS;" \
    "$DATABASE"

zero_relaxed_expected=$(cat <<EXPECTED
0000-00-00	0000-09-00	00:00:09
EXPECTED
)
expect_output \
    "str_to_date relaxed zero values" \
    "$zero_relaxed_expected" \
    "SET SESSION sql_mode = ''; SELECT STR_TO_DATE('00/00/0000','%m/%d/%Y'), "\
"STR_TO_DATE('9','%m'), STR_TO_DATE('9','%s');" \
    "$DATABASE"

zero_rejected_expected=$(cat <<EXPECTED
NULL	NULL
Warning	1411	Incorrect datetime value: '00/00/0000' for function str_to_date
Warning	1411	Incorrect datetime value: '9' for function str_to_date
EXPECTED
)
expect_output \
    "str_to_date zero mode warnings" \
    "$zero_rejected_expected" \
    "SET SESSION sql_mode = 'NO_ZERO_DATE'; SELECT STR_TO_DATE('00/00/0000','%m/%d/%Y'), "\
"STR_TO_DATE('9','%m'); SHOW WARNINGS;" \
    "$DATABASE"

zero_in_date_expected=$(cat <<EXPECTED
0000-09-01	0000-00-00	NULL
Warning	1411	Incorrect datetime value: '2024-00-01' for function str_to_date
EXPECTED
)
expect_output \
    "str_to_date no zero in date warnings" \
    "$zero_in_date_expected" \
    "SET SESSION sql_mode = 'NO_ZERO_IN_DATE'; SELECT "\
"STR_TO_DATE('0000-09-01','%Y-%m-%d'), STR_TO_DATE('0000-00-00','%Y-%m-%d'), "\
"STR_TO_DATE('2024-00-01','%Y-%m-%d'); SHOW WARNINGS;" \
    "$DATABASE"

allow_invalid_expected=$(cat <<EXPECTED
2024-02-31	NULL	2024-00-01
Warning	1411	Incorrect datetime value: '2024-13-01' for function str_to_date
EXPECTED
)
expect_output \
    "str_to_date allow invalid dates warnings" \
    "$allow_invalid_expected" \
    "SET SESSION sql_mode = 'ALLOW_INVALID_DATES'; SELECT "\
"STR_TO_DATE('2024-02-31','%Y-%m-%d'), STR_TO_DATE('2024-13-01','%Y-%m-%d'), "\
"STR_TO_DATE('2024-00-01','%Y-%m-%d'); SHOW WARNINGS;" \
    "$DATABASE"

allow_invalid_nozero_expected=$(cat <<EXPECTED
2024-02-31	NULL	0000-00-00
Warning	1411	Incorrect datetime value: '2024-00-01' for function str_to_date
EXPECTED
)
expect_output \
    "str_to_date allow invalid dates no zero in date warnings" \
    "$allow_invalid_nozero_expected" \
    "SET SESSION sql_mode = 'ALLOW_INVALID_DATES,NO_ZERO_IN_DATE'; SELECT "\
"STR_TO_DATE('2024-02-31','%Y-%m-%d'), STR_TO_DATE('2024-00-01','%Y-%m-%d'), "\
"STR_TO_DATE('0000-00-00','%Y-%m-%d'); SHOW WARNINGS;" \
    "$DATABASE"

run_mysql \
    "SET SESSION sql_mode = ''; CREATE TABLE t(id INT, v VARCHAR(32), body TEXT, n INT); "\
"INSERT INTO t VALUES (1, '2024-01-02', '09:30:17', 1), "\
"(2, '2024-01-02x', '09:30:17a', 2), (3, 'bad', NULL, 3), (4, NULL, 'bad', 4);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<EXPECTED
1	2024-01-02	09:30:17
2	2024-01-02	09:30:17
3	NULL	NULL
4	NULL	NULL
Warning	1292	Truncated incorrect date value: '2024-01-02x'
Warning	1292	Truncated incorrect time value: '09:30:17a'
Warning	1411	Incorrect datetime value: 'bad' for function str_to_date
Warning	1411	Incorrect datetime value: 'bad' for function str_to_date
EXPECTED
)
expect_output \
    "table str_to_date values" \
    "$table_expected" \
    "SELECT id, STR_TO_DATE(v,'%Y-%m-%d'), STR_TO_DATE(body,'%H:%i:%s') "\
"FROM t ORDER BY id; SHOW WARNINGS;" \
    "$DATABASE"

table_null_expected=$(cat <<EXPECTED
NULL	NULL
NULL	NULL
NULL	NULL
NULL	NULL
EXPECTED
)
expect_output \
    "table str_to_date null short circuit" \
    "$table_null_expected" \
    "SELECT STR_TO_DATE(n,NULL), STR_TO_DATE(NULL,v) FROM t ORDER BY id; SHOW WARNINGS;" \
    "$DATABASE"

nested_table_null_expected=$(cat <<EXPECTED
NULL
NULL
NULL
NULL
EXPECTED
)
expect_output \
    "table str_to_date nested null short circuit" \
    "$nested_table_null_expected" \
    "SELECT STR_TO_DATE(NULL,n + 1) FROM t ORDER BY id; SHOW WARNINGS;" \
    "$DATABASE"

expect_error \
    "str_to_date rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'STR_TO_DATE'" \
    "SELECT STR_TO_DATE();" \
    "$DATABASE"

expect_error \
    "str_to_date rejects one argument" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'STR_TO_DATE'" \
    "SELECT STR_TO_DATE('a');" \
    "$DATABASE"

expect_error \
    "str_to_date rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'STR_TO_DATE'" \
    "SELECT STR_TO_DATE('a', 'b', 'c');" \
    "$DATABASE"

expect_error \
    "str_to_date resolves nested missing format column before null short circuit" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "SELECT STR_TO_DATE(NULL, missing + 1) FROM t;" \
    "$DATABASE"

expect_error \
    "str_to_date resolves nested missing value column before null short circuit" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "SELECT STR_TO_DATE(missing + 1, NULL) FROM t;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_str_to_date_function_expectations: ok"
