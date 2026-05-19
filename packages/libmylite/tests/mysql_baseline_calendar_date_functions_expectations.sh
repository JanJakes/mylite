#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_calendar_date_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_calendar_date_functions_expectations: $1" >&2
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

core_expected=$(cat <<EXPECTED
7	34	2007-02-28	4	366	2008-02-29	2	3	365	0001-01-31	0999-12-31	NULL	NULL	NULL
-1	0
EXPECTED
)
expect_output \
    "core calendar date values" \
    "$core_expected" \
    "DO 0; SELECT DAYOFWEEK('2007-02-03'), DAYOFYEAR('2007-02-03'), "\
"LAST_DAY('2007-02-03'), DAYOFWEEK('2008-01-02 13:29:17'), "\
"DAYOFYEAR('2008-12-31 23:59:59'), LAST_DAY('2008-02-03 13:29:17'), "\
"DAYOFWEEK('0001-01-01'), DAYOFWEEK('0999-12-31'), "\
"DAYOFYEAR('0999-12-31'), LAST_DAY('0001-01-01'), "\
"LAST_DAY('0999-12-31'), DAYOFWEEK(NULL), DAYOFYEAR(NULL), LAST_DAY(NULL); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

zero_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL	NULL	2001-11-30	2	2	0000-01-31	0000-02-28	365
Warning	1292	Incorrect datetime value: '0000-00-00'
Warning	1292	Incorrect datetime value: '0000-00-00'
Warning	1292	Incorrect datetime value: '0000-00-00'
Warning	1292	Incorrect datetime value: '2001-11-00'
Warning	1292	Incorrect datetime value: '2001-11-00'
5
EXPECTED
)
expect_output \
    "zero and partial-zero calendar values" \
    "$zero_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT DAYOFWEEK('0000-00-00'), DAYOFYEAR('0000-00-00'), "\
"LAST_DAY('0000-00-00'), DAYOFWEEK('2001-11-00'), "\
"DAYOFYEAR('2001-11-00'), LAST_DAY('2001-11-00'), "\
"DAYOFWEEK('0000-01-02'), DAYOFYEAR('0000-01-02'), "\
"LAST_DAY('0000-01-02'), LAST_DAY('0000-02-01'), DAYOFYEAR('0000-12-31'); "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

invalid_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
Warning	1292	Incorrect datetime value: '2001-02-29'
Warning	1292	Incorrect datetime value: '2001-02-29'
Warning	1292	Incorrect datetime value: '2001-02-29'
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: '13:29:17'
Warning	1292	Incorrect datetime value: '13:29:17'
Warning	1292	Incorrect datetime value: '13:29:17'
Warning	1292	Incorrect datetime value: '2008-01-02 24:00:00'
Warning	1292	Incorrect datetime value: '2008-01-02 24:00:00'
Warning	1292	Incorrect datetime value: '2008-01-02 24:00:00'
Warning	1292	Incorrect datetime value: '2008-01-02 99:00:00'
Warning	1292	Incorrect datetime value: '2008-01-02 99:00:00'
Warning	1292	Incorrect datetime value: '2008-01-02 99:00:00'
15
EXPECTED
)
expect_output \
    "invalid calendar date warnings" \
    "$invalid_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT DAYOFWEEK('2001-02-29'), DAYOFYEAR('2001-02-29'), "\
"LAST_DAY('2001-02-29'), DAYOFWEEK('not-a-date'), DAYOFYEAR('not-a-date'), "\
"LAST_DAY('not-a-date'), DAYOFWEEK('13:29:17'), DAYOFYEAR('13:29:17'), "\
"LAST_DAY('13:29:17'), DAYOFWEEK('2008-01-02 24:00:00'), "\
"DAYOFYEAR('2008-01-02 24:00:00'), LAST_DAY('2008-01-02 24:00:00'), "\
"DAYOFWEEK('2008-01-02 99:00:00'), DAYOFYEAR('2008-01-02 99:00:00'), "\
"LAST_DAY('2008-01-02 99:00:00'); SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
DAYOFWEEK ('2007-02-03')	doy	month_end
7	34	2007-02-28
EXPECTED
)
expect_output_with_headers \
    "calendar labels and whitespace" \
    "$labels_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT DAYOFWEEK ('2007-02-03'), DAYOFYEAR ('2007-02-03') AS doy, "\
"LAST_DAY ('2007-02-03') AS month_end FROM DUAL;" \
    "$DATABASE"

identifier_expected=$(cat <<EXPECTED
dayofweek
dayofyear
last_day
EXPECTED
)
expect_output \
    "calendar function identifiers remain usable" \
    "$identifier_expected" \
    "DROP TABLE IF EXISTS dayofweek; DROP TABLE IF EXISTS dayofyear; "\
"DROP TABLE IF EXISTS last_day; CREATE TABLE dayofweek(id INT); "\
"CREATE TABLE dayofyear(id INT); CREATE TABLE last_day(id INT); "\
"SHOW TABLES LIKE 'day%'; SHOW TABLES LIKE 'last_day'; "\
"DROP TABLE dayofweek, dayofyear, last_day;" \
    "$DATABASE"

expect_error \
    "calendar ANSI_QUOTES double quotes become identifiers" \
    1054 \
    "42S22" \
    "Unknown column" \
    "SET SESSION sql_mode = 'ANSI_QUOTES'; SELECT DAYOFWEEK(\"2007-02-03\");" \
    "$DATABASE"

expect_output \
    "calendar functions accept ordinary strings after NO_BACKSLASH_ESCAPES" \
    "7	34	2007-02-28" \
    "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'; "\
"SELECT DAYOFWEEK('2007-02-03'), DAYOFYEAR('2007-02-03'), LAST_DAY('2007-02-03'); "\
"SET SESSION sql_mode = '';" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	7	366	2008-02-29	7	34
2	NULL	NULL	NULL	NULL	NULL
3	NULL	NULL	NULL	NULL	NULL
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: '2001-11-00'
2
EXPECTED
)
expect_output \
    "table-backed calendar date projection" \
    "$table_expected" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE t(id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, "\
"s VARCHAR(32), txt TEXT, tm TIME); "\
"INSERT INTO t VALUES "\
"(1,'2007-02-03','2008-12-31 23:59:59','2008-02-03 13:29:17',"\
"'2007-02-03','2008-02-03','13:29:17'),"\
"(2,NULL,NULL,NULL,NULL,NULL,NULL),"\
"(3,'0000-00-00','2001-11-00 00:00:00',NULL,'not-a-date','2001-11-00','01:02:03'); "\
"SELECT id, DAYOFWEEK(d), DAYOFYEAR(dt), LAST_DAY(ts), DAYOFWEEK(s), "\
"DAYOFYEAR(txt) FROM t ORDER BY id; SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "calendar DO status" \
    "$do_expected" \
    "DO DAYOFWEEK('2007-02-03'), DAYOFYEAR(NULL), LAST_DAY('2007-02-03'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "DAYOFWEEK zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT DAYOFWEEK();" \
    "$DATABASE"

expect_error \
    "DAYOFYEAR two arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT DAYOFYEAR('2007-02-03', 'x');" \
    "$DATABASE"

expect_error \
    "LAST_DAY zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT LAST_DAY();" \
    "$DATABASE"

expect_upstream_accepts \
    "numeric calendar date arguments deferred by MyLite" \
    "SELECT DAYOFWEEK(20070203), DAYOFYEAR(20070203), LAST_DAY(20070203);" \
    "$DATABASE"

expect_upstream_accepts \
    "warning-producing ISO-like calendar strings deferred by MyLite" \
    "SELECT DAYOFWEEK('2007-02-03T00:00:00Z'), DAYOFYEAR('2007-02-03T00:00:00Z'), "\
"LAST_DAY('2007-02-03T00:00:00Z');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_calendar_date_functions_expectations: ok"
