#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_microsecond_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_microsecond_function_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; SET time_zone = '+00:00';" >/dev/null

core_expected=$(cat <<EXPECTED
123456	100000	1	999999	123457	0	6	7	0	NULL
0
EXPECTED
)
expect_output \
    "core fractional microsecond values" \
    "$core_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT MICROSECOND('12:00:00.123456'), MICROSECOND('12:00:00.1'), "\
"MICROSECOND('12:00:00.000001'), MICROSECOND('12:00:00.999999'), "\
"MICROSECOND('12:00:00.1234567'), MICROSECOND('12:00:00.9999995'), "\
"MICROSECOND('-13:29:17.000006'), MICROSECOND('272:59:59.000007'), "\
"MICROSECOND('12:00:00'), MICROSECOND(NULL); SELECT @@warning_count;" \
    "$DATABASE"

datetime_expected=$(cat <<EXPECTED
10	100000	0	4	5
0
EXPECTED
)
expect_output \
    "datetime microsecond values" \
    "$datetime_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT MICROSECOND('2019-12-31 23:59:59.000010'), "\
"MICROSECOND('2019-12-31 23:59:59.1'), "\
"MICROSECOND('2019-12-31 23:59:59'), "\
"MICROSECOND('0000-00-00 01:02:03.000004'), "\
"MICROSECOND('2001-11-00 01:02:03.000005'); SELECT @@warning_count;" \
    "$DATABASE"

datetime_invalid_time_expected=$(cat <<EXPECTED
NULL	NULL	NULL
Warning	1292	Truncated incorrect time value: '2019-01-02 24:00:00'
Warning	1292	Truncated incorrect time value: '2019-01-02 99:00:00'
Warning	1292	Truncated incorrect time value: '2019-01-02 24:00:00.123456'
3
EXPECTED
)
expect_output \
    "datetime invalid time-part microsecond values" \
    "$datetime_invalid_time_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT MICROSECOND('2019-01-02 24:00:00'), "\
"MICROSECOND('2019-01-02 99:00:00'), "\
"MICROSECOND('2019-01-02 24:00:00.123456'); "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

date_invalid_expected=$(cat <<EXPECTED
0	0	0	NULL
Warning	1292	Truncated incorrect time value: '2024-01-02'
Warning	1292	Truncated incorrect time value: '0000-00-00'
Warning	1292	Truncated incorrect time value: '2001-11-00'
Warning	1292	Truncated incorrect time value: 'not-a-date'
4
EXPECTED
)
expect_output \
    "date-only and invalid microsecond values" \
    "$date_invalid_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT MICROSECOND('2024-01-02'), MICROSECOND('0000-00-00'), "\
"MICROSECOND('2001-11-00'), MICROSECOND('not-a-date'); "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
MICROSECOND ('12:00:00.000006')	us
6	7
EXPECTED
)
expect_output_with_headers \
    "microsecond labels and whitespace" \
    "$labels_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT MICROSECOND ('12:00:00.000006'), MICROSECOND ('2019-12-31 23:59:59.000007') AS us FROM DUAL;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	0	0	0	0	6	7	8
2	NULL	NULL	NULL	NULL	NULL	NULL	NULL
3	0	0	0	0	NULL	123457	0
Warning	1292	Truncated incorrect time value: 'bad'
Warning	1292	Truncated incorrect time value: '0000-00-00'
2
EXPECTED
)
expect_output \
    "table-backed microsecond values and warnings" \
    "$table_expected" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE t(id INT, d DATE, tm TIME, dt DATETIME, ts TIMESTAMP NULL, "\
"s VARCHAR(64), c CHAR(32), x TEXT); "\
"INSERT INTO t VALUES "\
"(1,'2024-01-02','13:29:17','2024-01-02 13:29:17','2024-01-02 13:29:17',"\
"'12:00:00.000006','2019-12-31 23:59:59.000007','2001-11-00 01:02:03.000008'),"\
"(2,NULL,NULL,NULL,NULL,NULL,NULL,NULL),"\
"(3,'0000-00-00','13:29:17','0000-00-00 01:02:03','0000-00-00 01:02:03',"\
"'bad','12:00:00.1234567','0000-00-00'); "\
"SELECT id, MICROSECOND(d), MICROSECOND(tm), MICROSECOND(dt), MICROSECOND(ts), "\
"MICROSECOND(s), MICROSECOND(c), MICROSECOND(x) FROM t ORDER BY id; "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "microsecond do status" \
    "$do_expected" \
    "DO MICROSECOND('12:00:00.000001'), MICROSECOND(NULL); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "MICROSECOND empty argument syntax" \
    1064 \
    "42000" \
    "syntax" \
    "SELECT MICROSECOND() AS x;" \
    "$DATABASE"

expect_error \
    "MICROSECOND extra argument syntax" \
    1064 \
    "42000" \
    "syntax" \
    "SELECT MICROSECOND('12:00:00.000001', 1) AS x;" \
    "$DATABASE"

expect_upstream_accepts \
    "numeric temporal coercion deferred by MyLite" \
    "SELECT MICROSECOND(1), MICROSECOND(TRUE), MICROSECOND(123456.789);" \
    "$DATABASE"

expect_upstream_accepts \
    "EXTRACT MICROSECOND deferred by MyLite" \
    "SELECT EXTRACT(MICROSECOND FROM '2003-01-02 10:30:00.000123');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_microsecond_function_expectations: ok"
