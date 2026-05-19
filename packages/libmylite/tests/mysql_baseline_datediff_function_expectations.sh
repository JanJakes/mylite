#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_datediff_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_datediff_function_expectations: $1" >&2
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
1	-31	0	NULL	NULL	-3287181	3287181	11	2	1	1	1
-1	0
EXPECTED
)
expect_output \
    "core DATEDIFF values" \
    "$core_expected" \
    "DO 0; SELECT "\
"DATEDIFF('2007-12-31 23:59:59','2007-12-30'), "\
"DATEDIFF('2010-11-30 23:59:59','2010-12-31'), "\
"DATEDIFF('2008-01-02 13:29:17','2008-01-02 23:59:59'), "\
"DATEDIFF(NULL,'2008-01-01'), DATEDIFF('2008-01-01',NULL), "\
"DATEDIFF('1000-01-01','9999-12-31'), "\
"DATEDIFF('9999-12-31','1000-01-01'), "\
"DATEDIFF('1582-10-15','1582-10-04'), "\
"DATEDIFF('2000-03-01','2000-02-28'), "\
"DATEDIFF('1900-03-01','1900-02-28'), "\
"DATEDIFF('0001-01-01','0000-12-31'), "\
"DATEDIFF('0000-03-01','0000-02-28'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

zero_expected=$(cat <<EXPECTED
NULL	NULL	1	NULL
Warning	1292	Incorrect datetime value: '0000-00-00'
Warning	1292	Incorrect datetime value: '0000-00-00'
Warning	1292	Incorrect datetime value: '2001-11-00'
Warning	1292	Incorrect datetime value: '0000-00-00'
4
EXPECTED
)
expect_output \
    "zero and partial-zero DATEDIFF values" \
    "$zero_expected" \
    "SELECT DATEDIFF('0000-00-00','0000-00-00'), "\
"DATEDIFF('2001-11-00','2001-10-31'), "\
"DATEDIFF('0000-01-02','0000-01-01'), "\
"DATEDIFF('2008-01-02','0000-00-00'); SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

invalid_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL	NULL
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: '13:29:17'
Warning	1292	Incorrect datetime value: 'bad'
Warning	1292	Incorrect datetime value: 'bad'
5
EXPECTED
)
expect_output \
    "invalid and NULL DATEDIFF warnings" \
    "$invalid_expected" \
    "SELECT DATEDIFF('not-a-date','2008-01-01'), "\
"DATEDIFF('2008-01-01','not-a-date'), DATEDIFF('13:29:17','2008-01-01'), "\
"DATEDIFF(NULL,'bad'), DATEDIFF('bad',NULL); SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
DATEDIFF ('2008-01-02','2008-01-01')	diff
1	1
EXPECTED
)
expect_output_with_headers \
    "DATEDIFF labels and whitespace" \
    "$labels_expected" \
    "SELECT DATEDIFF ('2008-01-02','2008-01-01'), "\
"DATEDIFF('2008-01-02','2008-01-01') AS diff FROM DUAL;" \
    "$DATABASE"

expect_error \
    "DATEDIFF ANSI_QUOTES double quotes become identifiers" \
    1054 \
    "42S22" \
    "Unknown column" \
    "SET SESSION sql_mode = 'ANSI_QUOTES'; SELECT DATEDIFF(\"2008-01-02\",\"2008-01-01\");" \
    "$DATABASE"

expect_output \
    "DATEDIFF accepts ordinary strings after NO_BACKSLASH_ESCAPES" \
    "1" \
    "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'; "\
"SELECT DATEDIFF('2008-01-02','2008-01-01'); SET SESSION sql_mode = '';" \
    "$DATABASE"

identifier_expected=$(cat <<EXPECTED
datediff
EXPECTED
)
expect_output \
    "DATEDIFF identifier remains usable" \
    "$identifier_expected" \
    "DROP TABLE IF EXISTS datediff; CREATE TABLE datediff(id INT); "\
"SHOW TABLES LIKE 'datediff'; DROP TABLE datediff;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	1	1	3	1
2	NULL	NULL	NULL	NULL
3	NULL	NULL	NULL	NULL
Warning	1292	Incorrect datetime value: 'not-a-date'
1
EXPECTED
)
expect_output \
    "table-backed DATEDIFF projection" \
    "$table_expected" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE t(id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, s VARCHAR(32)); "\
"INSERT INTO t VALUES "\
"(1,'2008-01-02','2008-01-03 13:29:17','2008-01-04 23:59:59','2008-01-05 01:02:03'),"\
"(2,NULL,NULL,NULL,NULL),"\
"(3,'0000-00-00','2001-11-00 00:00:00',NULL,'not-a-date'); "\
"SELECT id, DATEDIFF(dt,d), DATEDIFF(ts,dt), DATEDIFF(s,d), "\
"DATEDIFF(d,'2008-01-01') FROM t ORDER BY id; "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DATEDIFF DO status" \
    "$do_expected" \
    "DO DATEDIFF('2008-01-02','2008-01-01'), DATEDIFF(NULL,'2008-01-01'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "DATEDIFF zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT DATEDIFF();" \
    "$DATABASE"

expect_error \
    "DATEDIFF one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT DATEDIFF('2008-01-02');" \
    "$DATABASE"

expect_error \
    "DATEDIFF three arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT DATEDIFF('2008-01-03','2008-01-02','2008-01-01');" \
    "$DATABASE"

expect_upstream_accepts \
    "numeric temporal DATEDIFF arguments deferred by MyLite" \
    "SELECT DATEDIFF(20080102, 20080101), DATEDIFF(20080102132917, 20080101);" \
    "$DATABASE"

expect_upstream_accepts \
    "boolean DATEDIFF arguments deferred by MyLite" \
    "SELECT DATEDIFF(TRUE, FALSE);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_datediff_function_expectations: ok"
