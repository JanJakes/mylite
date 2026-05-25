#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_to_days_to_seconds_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_to_days_to_seconds_functions_expectations: $1" >&2
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
733321	733321	NULL	1	2	59	60	366	733466	365243	3652424	63358934400	63358938123	63371462400	63371548799
-1	0
EXPECTED
)
expect_output \
    "core TO_DAYS and TO_SECONDS values" \
    "$core_expected" \
    "DO 0; SELECT "\
"TO_DAYS('2007-10-07'), TO_DAYS('2007-10-07 23:59:59'), TO_DAYS(NULL), "\
"TO_DAYS('0000-01-01'), TO_DAYS('0000-01-02'), TO_DAYS('0000-02-28'), "\
"TO_DAYS('0000-03-01'), TO_DAYS('0001-01-01'), TO_DAYS('2008-02-29'), "\
"TO_DAYS('1000-01-01'), "\
"TO_DAYS('9999-12-31'), TO_SECONDS('2007-10-07'), "\
"TO_SECONDS('2007-10-07 01:02:03'), TO_SECONDS('2008-02-29'), "\
"TO_SECONDS('2008-02-29 23:59:59'); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

to_seconds_expected=$(cat <<EXPECTED
86400	172800	31622400	63426672000	63426721412	NULL
EXPECTED
)
expect_output \
    "TO_SECONDS date and datetime values" \
    "$to_seconds_expected" \
    "SELECT TO_SECONDS('0000-01-01'), TO_SECONDS('0000-01-02'), "\
"TO_SECONDS('0001-01-01'), TO_SECONDS('2009-11-29'), "\
"TO_SECONDS('2009-11-29 13:43:32'), TO_SECONDS(NULL);" \
    "$DATABASE"

invalid_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
Warning	1292	Incorrect datetime value: '0000-00-00'
Warning	1292	Incorrect datetime value: '2001-11-00'
Warning	1292	Incorrect datetime value: '2001-00-01'
Warning	1292	Incorrect datetime value: '0000-00-00'
Warning	1292	Incorrect datetime value: '2001-11-00'
Warning	1292	Incorrect datetime value: '2001-11-01 24:00:00'
Warning	1292	Incorrect datetime value: '2007-02-29'
Warning	1292	Incorrect datetime value: '0000-02-29'
Warning	1292	Incorrect datetime value: '2007-02-29'
9
EXPECTED
)
expect_output \
    "invalid zero and partial-zero values" \
    "$invalid_expected" \
"SELECT TO_DAYS('0000-00-00'), TO_DAYS('2001-11-00'), "\
"TO_DAYS('2001-00-01'), TO_SECONDS('0000-00-00'), "\
"TO_SECONDS('2001-11-00'), TO_SECONDS('2001-11-01 24:00:00'), "\
"TO_DAYS('2007-02-29'), TO_DAYS('0000-02-29'), TO_SECONDS('2007-02-29'); "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

invalid_string_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: '13:29:17'
Warning	1292	Incorrect datetime value: '13:29:17'
4
EXPECTED
)
expect_output \
    "invalid non-date strings" \
    "$invalid_string_expected" \
    "SELECT TO_DAYS('not-a-date'), TO_SECONDS('not-a-date'), "\
"TO_DAYS('13:29:17'), TO_SECONDS('13:29:17'); SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
TO_DAYS ('2007-10-07')	sec_no
733321	63358938123
EXPECTED
)
expect_output_with_headers \
    "labels and whitespace" \
    "$labels_expected" \
    "SELECT TO_DAYS ('2007-10-07'), TO_SECONDS('2007-10-07 01:02:03') AS sec_no FROM DUAL;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO row count" \
    "$do_expected" \
    "DO TO_DAYS('2007-10-07'), TO_SECONDS('2007-10-07 01:02:03'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

identifier_expected=$(cat <<EXPECTED
to_days
to_seconds
EXPECTED
)
expect_output \
    "function names remain identifiers" \
    "$identifier_expected" \
    "CREATE TABLE to_days(id INT); CREATE TABLE to_seconds(id INT); "\
"SHOW TABLES LIKE 'to_days'; SHOW TABLES LIKE 'to_seconds'; "\
"DROP TABLE to_days; DROP TABLE to_seconds;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	733321	733321	733321	63358934400	63359020799	63358938123	733321	63358949106
2	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
3	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
Warning	1292	Incorrect datetime value: 'not-a-date'
Warning	1292	Incorrect datetime value: 'not-a-date'
2
EXPECTED
)
expect_output \
    "table-backed projection" \
    "$table_expected" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE t(id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, s VARCHAR(32)); "\
"INSERT INTO t VALUES "\
"(1,'2007-10-07','2007-10-07 23:59:59','2007-10-07 01:02:03','2007-10-07 04:05:06'),"\
"(2,NULL,NULL,NULL,NULL),"\
"(3,'0000-00-00','2001-11-00 00:00:00','2001-11-00 00:00:00','not-a-date'); "\
"SELECT id, TO_DAYS(d), TO_DAYS(dt), TO_DAYS(ts), TO_SECONDS(d), "\
"TO_SECONDS(dt), TO_SECONDS(ts), TO_DAYS(s), TO_SECONDS(s) "\
"FROM t ORDER BY id; SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

filtered_expected=$(cat <<EXPECTED
1	733321	63358934400
EXPECTED
)
expect_output \
    "table-backed projection with WHERE ORDER LIMIT" \
    "$filtered_expected" \
    "SELECT id, TO_DAYS(dt), TO_SECONDS(d) FROM t "\
"WHERE id IN (1, 3) ORDER BY id LIMIT 1;" \
    "$DATABASE"

expect_error \
    "TO_DAYS ANSI_QUOTES double quotes become identifiers" \
    1054 \
    "42S22" \
    "Unknown column" \
    "SET SESSION sql_mode = 'ANSI_QUOTES'; SELECT TO_DAYS(\"2007-10-07\");" \
    "$DATABASE"

expect_output \
    "TO_DAYS accepts ordinary strings after NO_BACKSLASH_ESCAPES" \
    "733321" \
    "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'; "\
"SELECT TO_DAYS('2007-10-07'); SET SESSION sql_mode = '';" \
    "$DATABASE"

expect_error \
    "TO_DAYS empty arity" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'TO_DAYS'" \
    "SELECT TO_DAYS();" \
    "$DATABASE"

expect_error \
    "TO_DAYS too many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'TO_DAYS'" \
    "SELECT TO_DAYS('2007-10-07', 1);" \
    "$DATABASE"

expect_error \
    "TO_SECONDS empty arity" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'TO_SECONDS'" \
    "SELECT TO_SECONDS();" \
    "$DATABASE"

expect_error \
    "TO_SECONDS too many arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'TO_SECONDS'" \
    "SELECT TO_SECONDS('2007-10-07', 1);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts numeric temporal values deferred by MyLite" \
    "SELECT TO_DAYS(20071007), TO_SECONDS(20091129134332);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts fractional and T-separator strings deferred by MyLite" \
    "SELECT TO_DAYS('2007-10-07T01:02:03'), "\
"TO_SECONDS('2007-10-07 01:02:03.123456');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_to_days_to_seconds_functions_expectations: ok"
