#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_date_format_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_date_format_function_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

core_expected=$(cat <<EXPECTED
2008|08|01|1|02|2|13|13|01|01|1|29|17|17|13:29:17|01:29:17 PM|PM|000000|%|q|%	2008-01-02 00:00:00	NULL	NULL	1	1	0	NULL	0
-1	0
EXPECTED
)
expect_output \
    "core DATE_FORMAT values" \
    "$core_expected" \
    "DO 0; SELECT "\
"DATE_FORMAT('2008-01-02 13:29:17','%Y|%y|%m|%c|%d|%e|%H|%k|%h|%I|%l|%i|%S|%s|%T|%r|%p|%f|%%|%q|%'), "\
"DATE_FORMAT('2008-01-02','%Y-%m-%d %H:%i:%s'), "\
"DATE_FORMAT(NULL,'%Y'), DATE_FORMAT('2008-01-02',NULL), "\
"DATE_FORMAT('2008-01-02 13:29:17','%H.%i') = 13.29, "\
"DATE_FORMAT('2008-01-02 00:42:00','%H.%i') = 0.42, "\
"DATE_FORMAT('2008-01-02 13:29:17','%H.%i') = 0.42, "\
"DATE_FORMAT(NULL,'%H.%i') = 0.42, @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

truncated_numeric_expected=$(cat <<EXPECTED
1	1
Warning	1292	Truncated incorrect DOUBLE value: '2008-01-02'
EXPECTED
)
expect_output \
    "DATE_FORMAT broader numeric comparison warning deferred by MyLite" \
    "$truncated_numeric_expected" \
    "SELECT DATE_FORMAT('2008-01-02','%Y-%m-%d') = 2008, @@warning_count; "\
"SHOW WARNINGS;" \
    "$DATABASE"

names_expected=$(cat <<EXPECTED
Wed|Wednesday|Jan|January|2nd|002|3
EXPECTED
)
expect_output \
    "DATE_FORMAT names and ordinal values" \
    "$names_expected" \
    "SELECT DATE_FORMAT('2008-01-02 13:29:17','%a|%W|%b|%M|%D|%j|%w');" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
DATE_FORMAT ('2008-01-02','%Y')	formatted
2008	13.29
EXPECTED
)
expect_output_with_headers \
    "DATE_FORMAT labels and whitespace" \
    "$labels_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT DATE_FORMAT ('2008-01-02','%Y'), "\
"DATE_FORMAT('2008-01-02 13:29:17','%H.%i') AS formatted FROM DUAL;" \
    "$DATABASE"

identifier_expected=$(cat <<EXPECTED
date_format
date_format
EXPECTED
)
expect_output \
    "DATE_FORMAT identifier remains nonreserved" \
    "$identifier_expected" \
    "SET SESSION sql_mode = ''; "\
"DROP TABLE IF EXISTS date_format; CREATE TABLE date_format(id INT); "\
"SHOW TABLES LIKE 'date_format'; DROP TABLE date_format; "\
"SET SESSION sql_mode = 'IGNORE_SPACE'; CREATE TABLE date_format(id INT); "\
"SHOW TABLES LIKE 'date_format'; DROP TABLE date_format;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	13.29	2008-01-02 00:00:00	2008-01-02 13:29:17	2008-01-02 13:29:17
2	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "DATE_FORMAT table-backed projection" \
    "$table_expected" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE t(id INT, option_value VARCHAR(32), d DATE, dt DATETIME, ts TIMESTAMP NULL); "\
"INSERT INTO t VALUES "\
"(1,'2008-01-02 13:29:17','2008-01-02','2008-01-02 13:29:17','2008-01-02 13:29:17'), "\
"(2,NULL,NULL,NULL,NULL); "\
"SELECT id, DATE_FORMAT(option_value,'%H.%i'), DATE_FORMAT(d,'%Y-%m-%d %H:%i:%s'), "\
"DATE_FORMAT(dt,'%Y-%m-%d %H:%i:%s'), DATE_FORMAT(ts,'%Y-%m-%d %H:%i:%s') "\
"FROM t ORDER BY id;" \
    "$DATABASE"

comparison_expected=$(cat <<EXPECTED
1	1
2	0
3	NULL
4	NULL
Warning	1292	Incorrect datetime value: 'not-a-date'
1
EXPECTED
)
expect_output \
    "DATE_FORMAT table-backed numeric comparison and warning" \
    "$comparison_expected" \
    "DROP TABLE IF EXISTS options; "\
"CREATE TABLE options(id INT, option_value VARCHAR(32)); "\
"INSERT INTO options VALUES "\
"(1,'2008-01-02 00:42:00'),(2,'2008-01-02 13:29:17'),(3,NULL),(4,'not-a-date'); "\
"SELECT id, DATE_FORMAT(option_value, '%H.%i') = 0.42 FROM options ORDER BY id; "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DATE_FORMAT DO status" \
    "$do_expected" \
    "DO DATE_FORMAT('2008-01-02','%Y'), DATE_FORMAT(NULL,'%Y'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

invalid_expected=$(cat <<EXPECTED
NULL
Warning	1292	Incorrect datetime value: 'not-a-date'
EXPECTED
)
expect_output \
    "DATE_FORMAT invalid temporal warning" \
    "$invalid_expected" \
    "SELECT DATE_FORMAT('not-a-date','%Y'); SHOW WARNINGS;" \
    "$DATABASE"

expect_error \
    "DATE_FORMAT zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT DATE_FORMAT();" \
    "$DATABASE"

expect_error \
    "DATE_FORMAT one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT DATE_FORMAT('2008-01-02');" \
    "$DATABASE"

expect_error \
    "DATE_FORMAT three arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT DATE_FORMAT('2008-01-02','%Y','extra');" \
    "$DATABASE"

expect_upstream_accepts \
    "week tokens accepted by MySQL but deferred by MyLite" \
    "SELECT DATE_FORMAT('2008-01-02','%U|%u|%V|%v|%X|%x');" \
    "$DATABASE"

expect_upstream_accepts \
    "TIME values accepted by MySQL but deferred by MyLite" \
    "CREATE TABLE time_values(tm TIME); INSERT INTO time_values VALUES ('13:29:17'); "\
"SELECT DATE_FORMAT(tm,'%H.%i') FROM time_values;" \
    "$DATABASE"

expect_upstream_accepts \
    "format columns accepted by MySQL but deferred by MyLite" \
    "CREATE TABLE formats(v VARCHAR(32), f VARCHAR(16)); "\
"INSERT INTO formats VALUES ('2008-01-02 13:29:17','%H.%i'); "\
"SELECT DATE_FORMAT(v, f) FROM formats;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_date_format_function_expectations: ok"
