#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_temporal_extract_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_temporal_extract_functions_expectations: $1" >&2
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
2008-01-02	2008-01-02	NULL	2008	1	1	2	3	4	2	2	13	29	17	13	29	17	13	272	29	17
-1	0
EXPECTED
)
expect_output \
    "core temporal extract values" \
    "$core_expected" \
    "DO 0; SELECT DATE('2008-01-02 13:29:17'), DATE('2008-01-02'), "\
"DATE(NULL), YEAR('2008-01-02 13:29:17'), MONTH('2008-01-02 13:29:17'), "\
"QUARTER('2008-01-02'), QUARTER('2008-04-01'), QUARTER('2008-07-01'), "\
"QUARTER('2008-10-01'), "\
"DAY('2008-01-02 13:29:17'), DAYOFMONTH('2008-01-02 13:29:17'), "\
"HOUR('2008-01-02 13:29:17'), MINUTE('2008-01-02 13:29:17'), "\
"SECOND('2008-01-02 13:29:17'), HOUR('13:29:17'), MINUTE('13:29:17'), "\
"SECOND('13:29:17'), HOUR('-13:29:17'), HOUR('272:59:59'), "\
"MINUTE('-13:29:17'), SECOND('-13:29:17'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

zero_expected=$(cat <<EXPECTED
0000-00-00	0	0	0	0	0	1	0	4	1	2	3	2001-11-00	2001	11	4	0	2005-00-00	2005	0	0	0	0000-01-02	0	1	1	2
EXPECTED
)
expect_output \
    "zero and partial-zero temporal extract values" \
    "$zero_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT DATE('0000-00-00 00:00:00'), YEAR('0000-00-00'), "\
"MONTH('2005-00-00'), DAY('2001-11-00'), DAYOFMONTH('2001-11-00'), "\
"QUARTER('0000-00-00'), QUARTER('0000-01-02'), QUARTER('2005-00-00'), "\
"QUARTER('2001-11-00'), "\
"HOUR('0000-00-00 01:02:03'), MINUTE('0000-00-00 01:02:03'), "\
"SECOND('0000-00-00 01:02:03'), DATE('2001-11-00'), YEAR('2001-11-00'), "\
"MONTH('2001-11-00'), QUARTER('2001-11-00'), DAY('2001-11-00'), DATE('2005-00-00'), "\
"YEAR('2005-00-00'), MONTH('2005-00-00'), DAY('2005-00-00'), "\
"QUARTER('2005-00-00'), DATE('0000-01-02'), YEAR('0000-01-02'), "\
"MONTH('0000-01-02'), QUARTER('0000-01-02'), DAY('0000-01-02');" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
DATE ('2008-01-02 13:29:17')	yr	qtr	hr
2008-01-02	2008	2	13
EXPECTED
)
expect_output_with_headers \
    "temporal extract labels and whitespace" \
    "$labels_expected" \
"SET SESSION sql_mode = ''; "\
"SELECT DATE ('2008-01-02 13:29:17'), YEAR ('2008-01-02') AS yr, "\
"QUARTER ('2008-04-01') AS qtr, HOUR ('13:29:17') AS hr FROM DUAL;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	2008-01-02	2008	1	2	2	1	1	13	29	17	1	4	3	13	29	17
2	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
3	0000-00-00	0	0	0	0	0	NULL	13	29	17	NULL	0	4	NULL	NULL	NULL
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Truncated incorrect time value: 'not-a-date'
Warning	1292	Truncated incorrect time value: 'not-a-date'
Warning	1292	Truncated incorrect time value: 'not-a-date'
4
EXPECTED
)
expect_output \
    "table-backed temporal extract values and warnings" \
    "$table_expected" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE t(id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, "\
"tm TIME NULL, s VARCHAR(32), c CHAR(19), x TEXT); "\
"INSERT INTO t VALUES "\
"(1,'2008-01-02','2008-01-02 13:29:17','2008-01-02 13:29:17','13:29:17','2008-01-02 13:29:17','2008-10-01','2008-07-01'),"\
"(2,NULL,NULL,NULL,NULL,NULL,NULL,NULL),"\
"(3,'0000-00-00','0000-00-00 01:02:03',NULL,'-13:29:17','not-a-date','2005-00-00','2001-11-00'); "\
"SELECT id, DATE(dt), YEAR(d), MONTH(dt), DAY(d), DAYOFMONTH(dt), "\
"QUARTER(d), QUARTER(ts), HOUR(tm), MINUTE(tm), SECOND(tm), "\
"QUARTER(s), QUARTER(c), QUARTER(x), HOUR(s), MINUTE(s), SECOND(s) "\
"FROM t ORDER BY id; SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

timestamp_expected=$(cat <<EXPECTED
2008-01-02	2008	1	13	29	17
EXPECTED
)
expect_output \
    "timestamp-backed temporal extract values" \
    "$timestamp_expected" \
    "SELECT DATE(ts), YEAR(ts), QUARTER(ts), HOUR(ts), MINUTE(ts), SECOND(ts) "\
"FROM t WHERE id = 1;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "temporal extract do status" \
    "$do_expected" \
    "DO DATE('2008-01-02 13:29:17'), YEAR(NULL), QUARTER('2008-04-01'), "\
"HOUR('13:29:17'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

invalid_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Truncated incorrect time value: 'not-a-date'
Warning	1292	Truncated incorrect time value: 'not-a-date'
Warning	1292	Truncated incorrect time value: 'not-a-date'
8
EXPECTED
)
expect_output \
    "invalid temporal extract warnings" \
    "$invalid_expected" \
    "SELECT DATE('not-a-date'), YEAR('not-a-date'), MONTH('not-a-date'), "\
"QUARTER('not-a-date'), DAY('not-a-date'), HOUR('not-a-date'), MINUTE('not-a-date'), "\
"SECOND('not-a-date'); SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

expect_error \
    "DATE empty argument syntax" \
    1064 \
    "42000" \
    "syntax" \
    "SELECT DATE() AS x;" \
    "$DATABASE"

expect_error \
    "HOUR empty argument syntax" \
    1064 \
    "42000" \
    "syntax" \
    "SELECT HOUR() AS x;" \
    "$DATABASE"

expect_error \
    "QUARTER empty argument syntax" \
    1064 \
    "42000" \
    "syntax" \
    "SELECT QUARTER() AS x;" \
    "$DATABASE"

expect_error \
    "QUARTER extra argument syntax" \
    1064 \
    "42000" \
    "syntax" \
    "SELECT QUARTER('2008-01-02', 'x') AS x;" \
    "$DATABASE"

expect_error \
    "DAYOFMONTH empty argument count" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT DAYOFMONTH() AS x;" \
    "$DATABASE"

expect_upstream_accepts \
    "numeric temporal literals deferred by MyLite" \
    "SELECT DATE(20080102132917), YEAR(20080102132917), QUARTER(20080102132917), "\
"HOUR(20080102132917);" \
    "$DATABASE"

expect_upstream_accepts \
    "boolean and hex quarter coercions deferred by MyLite" \
    "SELECT QUARTER(TRUE), QUARTER(FALSE), QUARTER(X'323030382D30342D3031');" \
    "$DATABASE"

expect_upstream_accepts \
    "fractional temporal literals deferred by MyLite" \
    "SELECT HOUR('2008-01-02 13:29:17.999999'), SECOND('13:29:17.999999');" \
    "$DATABASE"

expect_upstream_accepts \
    "date-only time coercion deferred by MyLite" \
    "SELECT HOUR('2008-01-02'), MINUTE('2008-01-02'), SECOND('2008-01-02');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_temporal_extract_functions_expectations: ok"
