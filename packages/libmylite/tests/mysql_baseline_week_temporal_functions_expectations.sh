#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_week_temporal_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_week_temporal_functions_expectations: $1" >&2
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
0
7	7	8	53	0	52	199952	198652	6	1	8
-1	0
EXPECTED
)
expect_output \
    "core week temporal values" \
    "$core_expected" \
    "DO 0; SELECT @@default_week_format; "\
"SELECT WEEK('2008-02-20'), WEEK('2008-02-20',0), WEEK('2008-02-20',1), "\
"WEEK('2008-12-31',1), WEEK('2000-01-01',0), WEEK('2000-01-01',2), "\
"YEARWEEK('2000-01-01'), YEARWEEK('1987-01-01'), "\
"WEEKDAY('2008-02-03 22:23:00'), WEEKDAY('2007-11-06'), "\
"WEEKOFYEAR('2008-02-20'); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

mode_expected=$(cat <<EXPECTED
0	0	52	52	0	0	52	52	199952	199952	199952	199952	199952	199952	199952	199952
52	53	52	1	53	52	53	52	200852	200901	200852	200901	200853	200852	200853	200852
0
EXPECTED
)
expect_output \
    "week modes at year boundaries" \
    "$mode_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT WEEK('2000-01-01',0), WEEK('2000-01-01',1), WEEK('2000-01-01',2), "\
"WEEK('2000-01-01',3), WEEK('2000-01-01',4), WEEK('2000-01-01',5), "\
"WEEK('2000-01-01',6), WEEK('2000-01-01',7), YEARWEEK('2000-01-01',0), "\
"YEARWEEK('2000-01-01',1), YEARWEEK('2000-01-01',2), YEARWEEK('2000-01-01',3), "\
"YEARWEEK('2000-01-01',4), YEARWEEK('2000-01-01',5), YEARWEEK('2000-01-01',6), "\
"YEARWEEK('2000-01-01',7); SELECT WEEK('2008-12-31',0), WEEK('2008-12-31',1), "\
"WEEK('2008-12-31',2), WEEK('2008-12-31',3), WEEK('2008-12-31',4), "\
"WEEK('2008-12-31',5), WEEK('2008-12-31',6), WEEK('2008-12-31',7), "\
"YEARWEEK('2008-12-31',0), YEARWEEK('2008-12-31',1), YEARWEEK('2008-12-31',2), "\
"YEARWEEK('2008-12-31',3), YEARWEEK('2008-12-31',4), YEARWEEK('2008-12-31',5), "\
"YEARWEEK('2008-12-31',6), YEARWEEK('2008-12-31',7); SELECT @@warning_count;" \
    "$DATABASE"

mode_coercion_expected=$(cat <<EXPECTED
7	7	8	7	8	7	8	7
Warning	1292	Truncated incorrect INTEGER value: 'abc'
1
200807	200807	200808	200807	200808	200807	200808	200807
Warning	1292	Truncated incorrect INTEGER value: 'abc'
1
EXPECTED
)
expect_output \
    "mode coercion and unsupported string mode evidence" \
    "$mode_coercion_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT WEEK('2008-02-20', -1), WEEK('2008-02-20', 8), WEEK('2008-02-20', 9), "\
"WEEK('2008-02-20', 15), WEEK('2008-02-20', TRUE), WEEK('2008-02-20', FALSE), "\
"WEEK('2008-02-20', '3'), WEEK('2008-02-20', 'abc'); SHOW WARNINGS; "\
"SELECT @@warning_count; SELECT YEARWEEK('2008-02-20', -1), YEARWEEK('2008-02-20', 8), "\
"YEARWEEK('2008-02-20', 9), YEARWEEK('2008-02-20', 15), "\
"YEARWEEK('2008-02-20', TRUE), YEARWEEK('2008-02-20', FALSE), "\
"YEARWEEK('2008-02-20', '3'), YEARWEEK('2008-02-20', 'abc'); SHOW WARNINGS; "\
"SELECT @@warning_count;" \
    "$DATABASE"

mode_boundary_expected=$(cat <<EXPECTED
7	7	200807	200807
EXPECTED
)
expect_output \
    "signed boundary week mode literals" \
    "$mode_boundary_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT WEEK('2008-02-20', 9223372036854775807), "\
"WEEK('2008-02-20', -9223372036854775808), "\
"YEARWEEK('2008-02-20', 9223372036854775807), "\
"YEARWEEK('2008-02-20', -9223372036854775808);" \
    "$DATABASE"

default_mode_expected=$(cat <<EXPECTED
1	8	7	200807	200807
7	52	0	199952	199952
0
EXPECTED
)
expect_output \
    "default week format behavior" \
    "$default_mode_expected" \
    "SET SESSION sql_mode = ''; SET SESSION default_week_format = 1; "\
"SELECT @@default_week_format, WEEK('2008-02-20'), WEEK('2008-02-20', NULL), "\
"YEARWEEK('2008-02-20'), YEARWEEK('2008-02-20', NULL); "\
"SET SESSION default_week_format = 7; SELECT @@default_week_format, WEEK('2000-01-01'), "\
"WEEK('2000-01-01', NULL), YEARWEEK('2000-01-01'), YEARWEEK('2000-01-01', NULL); "\
"SELECT @@warning_count; SET SESSION default_week_format = 0;" \
    "$DATABASE"

zero_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL
NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	1	0	1	1
Warning	1292	Incorrect datetime value: '0000-00-00'
Warning	1292	Incorrect datetime value: '0000-00-00'
Warning	1292	Incorrect datetime value: '0000-00-00'
Warning	1292	Incorrect datetime value: '0000-00-00'
Warning	1292	Incorrect datetime value: '2001-11-00'
Warning	1292	Incorrect datetime value: '2001-11-00'
Warning	1292	Incorrect datetime value: '2001-11-00'
Warning	1292	Incorrect datetime value: '2001-11-00'
8
EXPECTED
)
expect_output \
    "zero and partial-zero week temporal values" \
    "$zero_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT WEEK(NULL), WEEKDAY(NULL), WEEKOFYEAR(NULL), YEARWEEK(NULL); "\
"SELECT WEEK('0000-00-00'), WEEKDAY('0000-00-00'), WEEKOFYEAR('0000-00-00'), "\
"YEARWEEK('0000-00-00'), WEEK('2001-11-00'), WEEKDAY('2001-11-00'), "\
"WEEKOFYEAR('2001-11-00'), YEARWEEK('2001-11-00'), WEEK('0000-01-02'), "\
"WEEKDAY('0000-01-02'), WEEKOFYEAR('0000-01-02'), YEARWEEK('0000-01-02'); "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

invalid_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: '2008-01-02 24:00:00'
Warning	1292	Incorrect datetime value: '2008-01-02 24:00:00'
Warning	1292	Incorrect datetime value: '2008-01-02 24:00:00'
Warning	1292	Incorrect datetime value: '2008-01-02 24:00:00'
8
EXPECTED
)
expect_output \
    "invalid week temporal warnings" \
    "$invalid_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT WEEK('not-a-date'), WEEKDAY('not-a-date'), WEEKOFYEAR('not-a-date'), "\
"YEARWEEK('not-a-date'), WEEK('2008-01-02 24:00:00'), "\
"WEEKDAY('2008-01-02 24:00:00'), WEEKOFYEAR('2008-01-02 24:00:00'), "\
"YEARWEEK('2008-01-02 24:00:00'); SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
WEEK ('2007-02-03')	weekday_value	calendar_week	year_week
4	5	8	200807
EXPECTED
)
expect_output_with_headers \
    "week temporal labels and whitespace" \
    "$labels_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT WEEK ('2007-02-03'), WEEKDAY ('2007-02-03') AS weekday_value, "\
"WEEKOFYEAR ('2008-02-20') AS calendar_week, "\
"YEARWEEK ('2008-02-20') AS year_week FROM DUAL;" \
    "$DATABASE"

identifier_expected=$(cat <<EXPECTED
week
weekday
weekofyear
yearweek
EXPECTED
)
expect_output \
    "week function identifiers remain usable" \
    "$identifier_expected" \
    "DROP TABLE IF EXISTS week; DROP TABLE IF EXISTS weekday; DROP TABLE IF EXISTS weekofyear; "\
"DROP TABLE IF EXISTS yearweek; CREATE TABLE week(id INT); CREATE TABLE weekday(id INT); "\
"CREATE TABLE weekofyear(id INT); CREATE TABLE yearweek(id INT); SHOW TABLES LIKE 'week'; "\
"SHOW TABLES LIKE 'weekday'; SHOW TABLES LIKE 'weekofyear'; SHOW TABLES LIKE 'yearweek'; "\
"DROP TABLE week, weekday, weekofyear, yearweek;" \
    "$DATABASE"

expect_error \
    "week ANSI_QUOTES double quotes become identifiers" \
    1054 \
    "42S22" \
    "Unknown column" \
    "SET SESSION sql_mode = 'ANSI_QUOTES'; SELECT WEEKDAY(\"2007-02-03\");" \
    "$DATABASE"

expect_output \
    "week functions accept ordinary strings after NO_BACKSLASH_ESCAPES" \
    "4	5	8	200807" \
    "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'; "\
"SELECT WEEK('2007-02-03'), WEEKDAY('2007-02-03'), WEEKOFYEAR('2008-02-20'), "\
"YEARWEEK('2008-02-20'); SET SESSION sql_mode = '';" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	4	2	5	200704	8
2	NULL	NULL	NULL	NULL	NULL
3	NULL	NULL	NULL	NULL	NULL
4	1	0	52	199952	1
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: '2001-11-00'
2
EXPECTED
)
expect_output \
    "table-backed week temporal projection" \
    "$table_expected" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE t(id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, "\
"s VARCHAR(32), txt TEXT, tm TIME); "\
"INSERT INTO t VALUES "\
"(1,'2007-02-03','2008-12-31 23:59:59','2008-02-03 13:29:17','2007-02-03','2008-02-20','13:29:17'),"\
"(2,NULL,NULL,NULL,NULL,NULL,NULL),"\
"(3,'0000-00-00','2001-11-00 00:00:00',NULL,'not-a-date','2001-11-00','01:02:03'),"\
"(4,'0000-01-02','0000-01-02 01:02:03','2000-01-01 00:00:00','2000-01-01','2008-12-31','04:05:06'); "\
"SELECT id, WEEK(d), WEEKDAY(dt), WEEKOFYEAR(ts), YEARWEEK(s), WEEK(txt, 3) "\
"FROM t ORDER BY id; SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

weekday_arithmetic_expected=$(cat <<EXPECTED
1
0
EXPECTED
)
expect_output \
    "week temporal weekday arithmetic predicate" \
    "$weekday_arithmetic_expected" \
    "SELECT id FROM t WHERE WEEKDAY(dt) + 1 = 3 ORDER BY id; SELECT @@warning_count;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "week temporal DO status" \
    "$do_expected" \
    "DO WEEK('2007-02-03'), WEEKDAY(NULL), WEEKOFYEAR('2008-02-20'), "\
"YEARWEEK('2008-02-20'); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "WEEK zero arguments syntax" \
    1064 \
    "42000" \
    "syntax" \
    "SELECT WEEK();" \
    "$DATABASE"

expect_error \
    "WEEK extra argument syntax" \
    1064 \
    "42000" \
    "syntax" \
    "SELECT WEEK('2008-01-01','x','y');" \
    "$DATABASE"

expect_error \
    "WEEKDAY zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT WEEKDAY();" \
    "$DATABASE"

expect_error \
    "WEEKOFYEAR extra argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT WEEKOFYEAR('2008-01-01','x');" \
    "$DATABASE"

expect_error \
    "YEARWEEK zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT YEARWEEK();" \
    "$DATABASE"

expect_error \
    "YEARWEEK extra argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT YEARWEEK('2008-01-01','x','y');" \
    "$DATABASE"

expect_upstream_accepts \
    "numeric temporal week arguments deferred by MyLite" \
    "SELECT WEEK(20070203), WEEKDAY(20070203), WEEKOFYEAR(20080220), YEARWEEK(20080220);" \
    "$DATABASE"

expect_upstream_accepts \
    "string mode coercion deferred by MyLite" \
    "SELECT WEEK('2008-02-20', '3'), YEARWEEK('2008-02-20', '3');" \
    "$DATABASE"

expect_upstream_accepts \
    "warning-producing ISO-like week strings deferred by MyLite" \
    "SELECT WEEK('2007-02-03T00:00:00Z'), WEEKDAY('2007-02-03T00:00:00Z'), "\
"WEEKOFYEAR('2007-02-03T00:00:00Z'), YEARWEEK('2007-02-03T00:00:00Z');" \
    "$DATABASE"

expect_upstream_accepts \
    "time value week coercion deferred by MyLite" \
    "SELECT WEEK(tm) FROM t ORDER BY id;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_week_temporal_functions_expectations: ok"
