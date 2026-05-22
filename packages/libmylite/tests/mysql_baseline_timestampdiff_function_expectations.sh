#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_timestampdiff_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_timestampdiff_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --default-character-set=utf8mb4 --batch --raw "$@"
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
1	4	13	56	398	9556	573365	34401906
-1	-4	-13	-56	-398	-9556	-573365	-34401906
-1	0
EXPECTED
)
expect_output \
    "core TIMESTAMPDIFF values" \
    "$core_expected" \
    "DO 0; SELECT "\
"TIMESTAMPDIFF(YEAR,'2003-01-01 00:00:00','2004-02-03 04:05:06'), "\
"TIMESTAMPDIFF(QUARTER,'2003-01-01 00:00:00','2004-02-03 04:05:06'), "\
"TIMESTAMPDIFF(MONTH,'2003-01-01 00:00:00','2004-02-03 04:05:06'), "\
"TIMESTAMPDIFF(WEEK,'2003-01-01 00:00:00','2004-02-03 04:05:06'), "\
"TIMESTAMPDIFF(DAY,'2003-01-01 00:00:00','2004-02-03 04:05:06'), "\
"TIMESTAMPDIFF(HOUR,'2003-01-01 00:00:00','2004-02-03 04:05:06'), "\
"TIMESTAMPDIFF(MINUTE,'2003-01-01 00:00:00','2004-02-03 04:05:06'), "\
"TIMESTAMPDIFF(SECOND,'2003-01-01 00:00:00','2004-02-03 04:05:06'); "\
"SELECT "\
"TIMESTAMPDIFF(YEAR,'2004-02-03 04:05:06','2003-01-01 00:00:00'), "\
"TIMESTAMPDIFF(QUARTER,'2004-02-03 04:05:06','2003-01-01 00:00:00'), "\
"TIMESTAMPDIFF(MONTH,'2004-02-03 04:05:06','2003-01-01 00:00:00'), "\
"TIMESTAMPDIFF(WEEK,'2004-02-03 04:05:06','2003-01-01 00:00:00'), "\
"TIMESTAMPDIFF(DAY,'2004-02-03 04:05:06','2003-01-01 00:00:00'), "\
"TIMESTAMPDIFF(HOUR,'2004-02-03 04:05:06','2003-01-01 00:00:00'), "\
"TIMESTAMPDIFF(MINUTE,'2004-02-03 04:05:06','2003-01-01 00:00:00'), "\
"TIMESTAMPDIFF(SECOND,'2004-02-03 04:05:06','2003-01-01 00:00:00'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

calendar_expected=$(cat <<EXPECTED
0	1	0	1	0	-1	-1	-1
0	0	0	1	-1	0	0	0
EXPECTED
)
expect_output \
    "TIMESTAMPDIFF calendar and partial-unit truncation" \
    "$calendar_expected" \
    "SELECT "\
"TIMESTAMPDIFF(YEAR,'2003-02-02','2004-02-01'), "\
"TIMESTAMPDIFF(YEAR,'2003-02-02','2004-02-02'), "\
"TIMESTAMPDIFF(MONTH,'2003-01-31','2003-02-28'), "\
"TIMESTAMPDIFF(MONTH,'2003-01-31','2003-03-01'), "\
"TIMESTAMPDIFF(QUARTER,'2003-01-01','2003-03-31'), "\
"TIMESTAMPDIFF(QUARTER,'2003-07-01','2003-04-01'), "\
"TIMESTAMPDIFF(MONTH,'2003-03-01','2003-01-31'), "\
"TIMESTAMPDIFF(YEAR,'2004-02-02','2003-02-02'); "\
"SELECT "\
"TIMESTAMPDIFF(DAY,'2003-02-01 23:59:59','2003-02-02 00:00:00'), "\
"TIMESTAMPDIFF(HOUR,'2003-02-01 23:59:59','2003-02-02 00:00:00'), "\
"TIMESTAMPDIFF(MINUTE,'2003-02-01 23:59:59','2003-02-02 00:00:00'), "\
"TIMESTAMPDIFF(SECOND,'2003-02-01 23:59:59','2003-02-02 00:00:00'), "\
"TIMESTAMPDIFF(SECOND,'2003-02-02 00:00:00','2003-02-01 23:59:59'), "\
"TIMESTAMPDIFF(MINUTE,'2003-02-02 00:00:00','2003-02-01 23:59:59'), "\
"TIMESTAMPDIFF(HOUR,'2003-02-02 00:00:00','2003-02-01 23:59:59'), "\
"TIMESTAMPDIFF(DAY,'2003-02-02 00:00:00','2003-02-01 23:59:59');" \
    "$DATABASE"

aliases_expected=$(cat <<EXPECTED
1	1	1	2	24	2	3	2
EXPECTED
)
expect_output \
    "TIMESTAMPDIFF SQL_TSI aliases" \
    "$aliases_expected" \
    "SELECT "\
"TIMESTAMPDIFF(SQL_TSI_YEAR,'2003-02-02','2004-02-02'), "\
"TIMESTAMPDIFF(SQL_TSI_QUARTER,'2003-01-01','2003-04-01'), "\
"TIMESTAMPDIFF(SQL_TSI_MONTH,'2003-01-31','2003-03-01'), "\
"TIMESTAMPDIFF(SQL_TSI_WEEK,'2003-02-01','2003-02-15'), "\
"TIMESTAMPDIFF(SQL_TSI_HOUR,'2003-02-01','2003-02-02'), "\
"TIMESTAMPDIFF(SQL_TSI_MINUTE,'2003-02-01','2003-02-01 00:02:03'), "\
"TIMESTAMPDIFF(SQL_TSI_SECOND,'2003-02-01','2003-02-01 00:00:03'), "\
"TIMESTAMPDIFF(DAY,'0000-01-02','0000-01-04');" \
    "$DATABASE"

null_invalid_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: 'bad-left'
Warning	1292	Incorrect datetime value: 'bad-right'
3
EXPECTED
)
expect_output \
    "TIMESTAMPDIFF NULL short-circuit and invalid warnings" \
    "$null_invalid_expected" \
    "SELECT "\
"TIMESTAMPDIFF(DAY,NULL,'not-a-date'), "\
"TIMESTAMPDIFF(DAY,'not-a-date',NULL), "\
"TIMESTAMPDIFF(DAY,'bad-left','bad-right'), "\
"TIMESTAMPDIFF(DAY,'2003-01-01','bad-right'); "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

zero_expected=$(cat <<EXPECTED
NULL	NULL	731610	NULL
Warning	1292	Incorrect datetime value: '0000-00-00'
Warning	1292	Incorrect datetime value: '2001-11-00'
Warning	1292	Incorrect datetime value: '0000-00-00'
3
EXPECTED
)
expect_output \
    "TIMESTAMPDIFF zero and partial-zero dates" \
    "$zero_expected" \
    "SELECT "\
"TIMESTAMPDIFF(DAY,'0000-00-00','2003-02-01'), "\
"TIMESTAMPDIFF(DAY,'2001-11-00','2003-02-01'), "\
"TIMESTAMPDIFF(DAY,'0000-01-02','2003-02-01'), "\
"TIMESTAMPDIFF(DAY,'2003-02-01','0000-00-00'); "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
TIMESTAMPDIFF (DAY,'2003-01-01','2003-01-02')	tsd
1	1
EXPECTED
)
expect_output_with_headers \
    "TIMESTAMPDIFF labels and whitespace" \
    "$labels_expected" \
    "SELECT TIMESTAMPDIFF (DAY,'2003-01-01','2003-01-02'), "\
"TIMESTAMPDIFF(SQL_TSI_DAY,'2003-01-01','2003-01-02') AS tsd FROM DUAL;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	1	52	1	36
2	NULL	NULL	NULL	NULL
3	NULL	NULL	NULL	NULL
0
EXPECTED
)
expect_output \
    "TIMESTAMPDIFF table-backed projection" \
    "$table_expected" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE t (id int, d date, dt datetime, ts timestamp NULL, txt varchar(32)); "\
"INSERT INTO t VALUES "\
"(1,'2003-02-01','2003-02-02 01:02:03','2003-02-03 04:05:06','2003-03-01'), "\
"(2,NULL,NULL,NULL,NULL), "\
"(3,'0000-00-00','2001-11-00',NULL,'not-a-date'); "\
"SELECT id, TIMESTAMPDIFF(DAY,d,dt), TIMESTAMPDIFF(HOUR,d,ts), "\
"TIMESTAMPDIFF(MONTH,d,txt), TIMESTAMPDIFF(HOUR,d,'2003-02-02 12:00:00') "\
"FROM t ORDER BY id; SELECT @@warning_count;" \
    "$DATABASE"

identifier_expected=$(cat <<EXPECTED
1
EXPECTED
)
expect_output \
    "TIMESTAMPDIFF identifier remains nonreserved" \
    "$identifier_expected" \
    "CREATE TABLE timestampdiff (id int); INSERT INTO timestampdiff VALUES (1); "\
"SELECT id FROM timestampdiff;" \
    "$DATABASE"

expect_upstream_accepts \
    "TIMESTAMPDIFF bare MICROSECOND accepted upstream" \
    "SELECT TIMESTAMPDIFF(MICROSECOND,'2003-02-01 00:00:00.000001','2003-02-01 00:00:00.000003');" \
    "$DATABASE"

expect_error \
    "TIMESTAMPDIFF zero arguments" \
    1064 \
    "42000" \
    "near ')'" \
    "SELECT TIMESTAMPDIFF();" \
    "$DATABASE"
expect_error \
    "TIMESTAMPDIFF one argument" \
    1064 \
    "42000" \
    "near ')'" \
    "SELECT TIMESTAMPDIFF(DAY);" \
    "$DATABASE"
expect_error \
    "TIMESTAMPDIFF two arguments" \
    1064 \
    "42000" \
    "near ')'" \
    "SELECT TIMESTAMPDIFF(DAY,'2003-01-01');" \
    "$DATABASE"
expect_error \
    "TIMESTAMPDIFF extra argument" \
    1064 \
    "42000" \
    "near ','extra')'" \
    "SELECT TIMESTAMPDIFF(DAY,'2003-01-01','2003-01-02','extra');" \
    "$DATABASE"
expect_error \
    "TIMESTAMPDIFF quoted unit" \
    1064 \
    "42000" \
    "near ''DAY'" \
    "SELECT TIMESTAMPDIFF('DAY','2003-01-01','2003-01-02');" \
    "$DATABASE"
expect_error \
    "TIMESTAMPDIFF unknown unit" \
    1064 \
    "42000" \
    "near 'BOGUS" \
    "SELECT TIMESTAMPDIFF(BOGUS,'2003-01-01','2003-01-02');" \
    "$DATABASE"
expect_error \
    "TIMESTAMPDIFF composite unit" \
    1064 \
    "42000" \
    "near 'DAY_HOUR" \
    "SELECT TIMESTAMPDIFF(DAY_HOUR,'2003-01-01','2003-01-02');" \
    "$DATABASE"
expect_error \
    "TIMESTAMPDIFF SQL_TSI_MICROSECOND rejected upstream" \
    1064 \
    "42000" \
    "near 'SQL_TSI_MICROSECOND" \
    "SELECT TIMESTAMPDIFF(SQL_TSI_MICROSECOND,'2003-01-01','2003-01-02');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_timestampdiff_function_expectations: ok"
