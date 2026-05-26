#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_extract_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_extract_function_expectations: $1" >&2
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
2019	7	2	1	2	3	3	201907	201	20102	2010203	102	10203	203	123456	100000	123457	0	10	-6	0	NULL	-13	-29	-17	-1329	-132917	-2917
-1	0
EXPECTED
)
expect_output \
    "core extract values" \
    "$core_expected" \
    "DO 0; SELECT EXTRACT(YEAR FROM '2019-07-02 01:02:03'), "\
"EXTRACT(MONTH FROM '2019-07-02 01:02:03'), "\
"EXTRACT(DAY FROM '2019-07-02 01:02:03'), "\
"EXTRACT(HOUR FROM '2019-07-02 01:02:03'), "\
"EXTRACT(MINUTE FROM '2019-07-02 01:02:03'), "\
"EXTRACT(SECOND FROM '2019-07-02 01:02:03'), "\
"EXTRACT(QUARTER FROM '2019-07-02 01:02:03'), "\
"EXTRACT(YEAR_MONTH FROM '2019-07-02 01:02:03'), "\
"EXTRACT(DAY_HOUR FROM '2019-07-02 01:02:03'), "\
"EXTRACT(DAY_MINUTE FROM '2019-07-02 01:02:03'), "\
"EXTRACT(DAY_SECOND FROM '2019-07-02 01:02:03'), "\
"EXTRACT(HOUR_MINUTE FROM '2019-07-02 01:02:03'), "\
"EXTRACT(HOUR_SECOND FROM '2019-07-02 01:02:03'), "\
"EXTRACT(MINUTE_SECOND FROM '2019-07-02 01:02:03'), "\
"EXTRACT(MICROSECOND FROM '12:00:00.123456'), "\
"EXTRACT(MICROSECOND FROM '12:00:00.1'), "\
"EXTRACT(MICROSECOND FROM '12:00:00.1234567'), "\
"EXTRACT(MICROSECOND FROM '12:00:00.9999995'), "\
"EXTRACT(MICROSECOND FROM '2019-12-31 23:59:59.000010'), "\
"EXTRACT(MICROSECOND FROM '-13:29:17.000006'), "\
"EXTRACT(MICROSECOND FROM '12:00:00'), "\
"EXTRACT(YEAR FROM NULL), EXTRACT(HOUR FROM '-13:29:17'), "\
"EXTRACT(MINUTE FROM '-13:29:17'), EXTRACT(SECOND FROM '-13:29:17'), "\
"EXTRACT(HOUR_MINUTE FROM '-13:29:17'), EXTRACT(DAY_SECOND FROM '-13:29:17'), "\
"EXTRACT(MINUTE_SECOND FROM '-13:29:17'); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

zero_expected=$(cat <<EXPECTED
0	0	0	0	1	1	1	10203	4	0
EXPECTED
)
expect_output \
    "zero and partial-zero extract values" \
    "$zero_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT EXTRACT(YEAR FROM '0000-00-00'), EXTRACT(MONTH FROM '2005-00-00'), "\
"EXTRACT(DAY FROM '2001-11-00'), EXTRACT(QUARTER FROM '0000-00-00'), "\
"EXTRACT(YEAR_MONTH FROM '0000-01-02'), "\
"EXTRACT(HOUR FROM '0000-00-00 01:02:03'), "\
"EXTRACT(YEAR_MONTH FROM '0000-01-02'), "\
"EXTRACT(DAY_SECOND FROM '2001-11-00 01:02:03'), "\
"EXTRACT(MICROSECOND FROM '0000-00-00 01:02:03.000004'), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
EXTRACT(YEAR FROM '2019-07-02')	qtr	hm
2019	3	102
EXPECTED
)
expect_output_with_headers \
    "extract labels and aliases" \
    "$labels_expected" \
    "SELECT EXTRACT(YEAR FROM '2019-07-02'), "\
"EXTRACT(QUARTER FROM '2019-07-02') AS qtr, "\
"EXTRACT(HOUR_MINUTE FROM '01:02:03') AS hm FROM DUAL;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	2019	0	3	201907	2132917	0	2019	2132917	-13	13	-132917	132917	-12
2	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
3	0	0	NULL	NULL	NULL	NULL	NULL	NULL	13	NULL	132917	NULL	NULL
Warning	1292	Truncated incorrect time value: 'not-a-date'
Warning	1292	Truncated incorrect time value: 'not-a-date'
Warning	1292	Truncated incorrect time value: 'not-a-date'
3
EXPECTED
)
expect_output \
    "table-backed extract values and warnings" \
    "$table_expected" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE t(id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, "\
"tm TIME NULL, s VARCHAR(32), sf VARCHAR(32)); "\
"INSERT INTO t VALUES "\
"(1,'2019-07-02','2019-07-02 13:29:17','2019-07-02 13:29:17','-13:29:17','13:29:17','-13:29:17.000012'),"\
"(2,NULL,NULL,NULL,NULL,NULL,NULL),"\
"(3,'0000-00-00',NULL,NULL,'13:29:17','not-a-date','not-a-date'); "\
"SELECT id, EXTRACT(YEAR FROM d), EXTRACT(MICROSECOND FROM d), "\
"EXTRACT(QUARTER FROM dt), "\
"EXTRACT(YEAR_MONTH FROM dt), EXTRACT(DAY_SECOND FROM dt), "\
"EXTRACT(MICROSECOND FROM dt), EXTRACT(YEAR FROM ts), EXTRACT(DAY_SECOND FROM ts), "\
"EXTRACT(HOUR FROM tm), EXTRACT(HOUR FROM s), EXTRACT(DAY_SECOND FROM tm), "\
"EXTRACT(DAY_SECOND FROM s), EXTRACT(MICROSECOND FROM sf) "\
"FROM t ORDER BY id; SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

filtered_expected=$(cat <<EXPECTED
3	132917
2	NULL
EXPECTED
)
expect_output \
    "table-backed extract ordered limit envelope" \
    "$filtered_expected" \
    "SELECT id, EXTRACT(DAY_SECOND FROM tm) FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "extract do status" \
    "$do_expected" \
    "DO EXTRACT(YEAR FROM '2019-07-02'), EXTRACT(HOUR FROM '-13:29:17'), "\
"EXTRACT(MICROSECOND FROM '12:00:00.000123'), EXTRACT(YEAR FROM NULL); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

invalid_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL	NULL	NULL	0
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Truncated incorrect time value: 'not-a-date'
Warning	1292	Truncated incorrect time value: 'not-a-date'
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Truncated incorrect time value: 'not-a-date'
Warning	1292	Truncated incorrect time value: '2019-01-02 24:00:00.123456'
Warning	1292	Truncated incorrect time value: '2024-01-02'
7
EXPECTED
)
expect_output \
    "invalid extract warnings" \
    "$invalid_expected" \
    "SELECT EXTRACT(YEAR FROM 'not-a-date'), EXTRACT(HOUR FROM 'not-a-date'), "\
"EXTRACT(DAY_SECOND FROM 'not-a-date'), EXTRACT(QUARTER FROM 'not-a-date'), "\
"EXTRACT(MICROSECOND FROM 'not-a-date'), "\
"EXTRACT(MICROSECOND FROM '2019-01-02 24:00:00.123456'), "\
"EXTRACT(MICROSECOND FROM '2024-01-02'); "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

expect_error \
    "EXTRACT empty syntax" \
    1064 \
    "42000" \
    "syntax" \
    "SELECT EXTRACT();" \
    "$DATABASE"

expect_error \
    "EXTRACT missing FROM syntax" \
    1064 \
    "42000" \
    "syntax" \
    "SELECT EXTRACT(YEAR);" \
    "$DATABASE"

expect_upstream_accepts \
    "week extract deferred by MyLite" \
    "SELECT EXTRACT(WEEK FROM '2019-07-02');" \
    "$DATABASE"

expect_upstream_accepts \
    "date-only time extract coercion deferred by MyLite" \
    "SELECT EXTRACT(HOUR FROM '2008-01-02'), EXTRACT(HOUR_SECOND FROM '2008-01-02');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_extract_function_expectations: ok"
