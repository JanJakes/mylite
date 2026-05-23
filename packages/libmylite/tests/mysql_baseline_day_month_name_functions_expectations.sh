#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_day_month_name_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_day_month_name_functions_expectations: $1" >&2
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

run_mysql_type_info() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --column-type-info -vvv "$@"
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

expect_contains() {
    label=$1
    haystack=$2
    needle=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle], got [$haystack]" ;;
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
Saturday	February	Wednesday	December	Monday	December	NULL	NULL
-1	0
EXPECTED
)
expect_output \
    "core calendar name values" \
    "$core_expected" \
    "DO 0; SELECT DAYNAME('2007-02-03'), MONTHNAME('2008-02-03'), "\
"DAYNAME('2008-01-02 13:29:17'), MONTHNAME('2008-12-31 23:59:59'), "\
"DAYNAME('0001-01-01'), MONTHNAME('0999-12-31'), DAYNAME(NULL), MONTHNAME(NULL); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

zero_expected=$(cat <<EXPECTED
NULL	NULL	November	Monday	January	NULL
Warning	1292	Incorrect datetime value: '0000-00-00'
Warning	1292	Incorrect datetime value: '2001-11-00'
Warning	1292	Incorrect datetime value: '0000-02-29'
3
EXPECTED
)
expect_output \
    "zero and partial-zero calendar name values" \
    "$zero_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT DAYNAME('0000-00-00'), DAYNAME('2001-11-00'), MONTHNAME('2001-11-00'), "\
"DAYNAME('0000-01-02'), MONTHNAME('0000-01-02'), MONTHNAME('0000-02-29'); "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

invalid_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL	NULL	NULL	Saturday	February
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: '13:29:17'
Warning	1292	Incorrect datetime value: '13:29:17'
Warning	1292	Incorrect datetime value: '2008-01-02 24:00:00'
Warning	1292	Incorrect datetime value: '2008-01-02 99:00:00'
Warning	1292	Truncated incorrect datetime value: '2007-02-03T00:00:00Z'
Warning	1292	Truncated incorrect datetime value: '2007-02-03T00:00:00Z'
8
EXPECTED
)
expect_output \
    "invalid and truncating calendar name warnings" \
    "$invalid_expected" \
    "SET SESSION sql_mode = ''; "\
"SELECT DAYNAME('not-a-date'), MONTHNAME('not-a-date'), DAYNAME('13:29:17'), "\
"MONTHNAME('13:29:17'), DAYNAME('2008-01-02 24:00:00'), "\
"MONTHNAME('2008-01-02 99:00:00'), DAYNAME('2007-02-03T00:00:00Z'), "\
"MONTHNAME('2007-02-03T00:00:00Z'); SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
DAYNAME ('2007-02-03')	month_label
Saturday	February
EXPECTED
)
expect_output_with_headers \
    "calendar name labels" \
    "$labels_expected" \
    "SELECT DAYNAME ('2007-02-03'), MONTHNAME ('2008-02-03') AS month_label FROM DUAL;" \
    "$DATABASE"

run_mysql \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE t(id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, "\
"s VARCHAR(32), txt TEXT, tm TIME NULL); "\
"INSERT INTO t VALUES "\
"(1,'2007-02-03','2008-12-31 23:59:59','2008-02-03 13:29:17',"\
"'2008-02-03','2008-02-03','13:29:17'),"\
"(2,NULL,NULL,NULL,NULL,NULL,NULL),"\
"(3,'0000-00-00','2001-11-00 00:00:00',NULL,"\
"'not-a-date','2001-11-00','01:02:03');" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<EXPECTED
1	Saturday	December	Sunday	February	Sunday	February
2	NULL	NULL	NULL	NULL	NULL	NULL
3	NULL	November	NULL	NULL	NULL	November
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: '2001-11-00'
2
EXPECTED
)
expect_output \
    "table-backed calendar name values" \
    "$table_expected" \
    "SELECT id, DAYNAME(d), MONTHNAME(dt), DAYNAME(ts), MONTHNAME(s), "\
"DAYNAME(txt), MONTHNAME(txt) FROM t ORDER BY id; SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

metadata_output=$(run_mysql_type_info \
    "SET NAMES utf8mb4; SELECT DAYNAME('2020-01-01') AS dn, MONTHNAME('2020-01-01') AS mn;" \
    "$DATABASE")
expect_contains "calendar name metadata type" "$metadata_output" "Type:       VAR_STRING"
expect_contains "calendar name metadata collation" "$metadata_output" \
    "Collation:  utf8mb4_0900_ai_ci (255)"
expect_contains "calendar name metadata length" "$metadata_output" "Length:     36"

expect_error \
    "dayname zero argument arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'DAYNAME'" \
    "SELECT DAYNAME();" \
    "$DATABASE"
expect_error \
    "monthname extra argument arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'MONTHNAME'" \
    "SELECT MONTHNAME('2007-02-03', 'x');" \
    "$DATABASE"

expect_upstream_accepts \
    "numeric temporal argument coercion deferred by MyLite" \
    "SELECT DAYNAME(20070203), MONTHNAME(20080203);" \
    "$DATABASE"
expect_upstream_accepts \
    "TIME descriptor coercion deferred by MyLite" \
    "SELECT DAYNAME(tm), MONTHNAME(tm) FROM t WHERE id = 1;" \
    "$DATABASE"
expect_upstream_accepts \
    "locale-aware calendar names deferred by MyLite" \
    "SET lc_time_names='es_MX'; SELECT DAYNAME('2008-01-02'), MONTHNAME('2008-01-02');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_day_month_name_functions_expectations: ok"
