#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_timestamp_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_timestamp_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; SET SESSION sql_mode = ''; SET time_zone = '+00:00';" >/dev/null

core_expected=$(cat <<EXPECTED
2003-12-31 00:00:00	2003-12-31 12:34:56	2003-12-31 12:00:00	2004-01-01 02:03:04	2003-12-30 22:57:57	NULL	NULL
-1	0
EXPECTED
)
expect_output \
    "core TIMESTAMP values" \
    "$core_expected" \
    "DO 0; SELECT "\
"TIMESTAMP('2003-12-31'), "\
"TIMESTAMP('2003-12-31 12:34:56'), "\
"TIMESTAMP('2003-12-31','12:00:00'), "\
"TIMESTAMP('2003-12-31','1 02:03:04'), "\
"TIMESTAMP('2003-12-31','-01:02:03'), "\
"TIMESTAMP(NULL), "\
"TIMESTAMP('2003-12-31', NULL); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

invalid_datetime_expected=$(cat <<EXPECTED
NULL
Warning	1292	Incorrect datetime value: 'bad'
1
EXPECTED
)
expect_output \
    "TIMESTAMP invalid datetime warning" \
    "$invalid_datetime_expected" \
    "SELECT TIMESTAMP('bad'); SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"
expect_output \
    "TIMESTAMP invalid datetime before NULL time warning" \
    "$invalid_datetime_expected" \
    "SELECT TIMESTAMP('bad', NULL); SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"
expect_output \
    "TIMESTAMP invalid datetime before invalid time warning" \
    "$invalid_datetime_expected" \
    "SELECT TIMESTAMP('bad', 'bad'); SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"
expect_output \
    "TIMESTAMP invalid datetime before clipped time warning" \
    "$invalid_datetime_expected" \
    "SELECT TIMESTAMP('bad', '839:00:00'); SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

null_first_expected=$(cat <<EXPECTED
NULL
0
EXPECTED
)
expect_output \
    "TIMESTAMP NULL first argument before invalid time short-circuits" \
    "$null_first_expected" \
    "SELECT TIMESTAMP(NULL, 'bad'); SELECT @@warning_count;" \
    "$DATABASE"

invalid_time_expected=$(cat <<EXPECTED
NULL
Warning	1292	Truncated incorrect time value: 'bad'
1
EXPECTED
)
expect_output \
    "TIMESTAMP invalid time warning" \
    "$invalid_time_expected" \
    "SELECT TIMESTAMP('2003-12-31','bad'); SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

clipped_time_expected=$(cat <<EXPECTED
2004-02-03 22:59:59	2004-02-03 22:59:59
Warning	1292	Truncated incorrect time value: '839:00:00'
1
EXPECTED
)
expect_output \
    "TIMESTAMP clipped time warning" \
    "$clipped_time_expected" \
    "SELECT "\
"TIMESTAMP('2003-12-31','838:59:59'), "\
"TIMESTAMP('2003-12-31','839:00:00'); "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

overflow_time_expected=$(cat <<EXPECTED
NULL
Warning	1441	Datetime function: add_time field overflow
1
EXPECTED
)
expect_output \
    "TIMESTAMP add_time overflow warning" \
    "$overflow_time_expected" \
    "SELECT TIMESTAMP('9999-12-31 23:59:59','00:00:01'); "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t(id INT, d DATE, dt DATETIME, ts TIMESTAMP NULL, tm TIME, v VARCHAR(32)); "\
"INSERT INTO t VALUES "\
"(1,'2003-12-31','2003-12-31 12:34:56','2003-12-31 10:00:00','01:02:03','2003-12-31 06:00:00'), "\
"(2,NULL,NULL,NULL,NULL,NULL);" \
    "$DATABASE" >/dev/null

row_expected=$(cat <<EXPECTED
1	2003-12-31 00:00:00	2003-12-31 12:34:56	2003-12-31 10:00:00	2003-12-31 06:00:00.000000	2003-12-31 01:02:03	2003-12-31 13:36:59
2	NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "row-backed TIMESTAMP values" \
    "$row_expected" \
    "SELECT id, TIMESTAMP(d), TIMESTAMP(dt), TIMESTAMP(ts), TIMESTAMP(v), "\
"TIMESTAMP(d, tm), TIMESTAMP(dt, tm) FROM t ORDER BY id;" \
    "$DATABASE"

expect_error \
    "TIMESTAMP zero arguments" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT TIMESTAMP();" \
    "$DATABASE"
expect_error \
    "TIMESTAMP three arguments" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT TIMESTAMP('2001-01-01','00:00:00','x');" \
    "$DATABASE"

expect_upstream_accepts \
    "TIMESTAMP numeric first argument is deferred by MyLite" \
    "SELECT TIMESTAMP(1);" \
    "$DATABASE"
expect_upstream_accepts \
    "TIMESTAMP numeric time argument is deferred by MyLite" \
    "SELECT TIMESTAMP('2001-01-01', 1);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_timestamp_function_expectations: ok"
