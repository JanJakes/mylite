#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_time_second_conversion_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_time_second_conversion_functions_expectations: $1" >&2
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
80580	2378	-2378	360000	NULL	00:39:38	00:39:38	-00:39:38	00:00:01	00:00:00	NULL	0
-1	0
EXPECTED
)
expect_output \
    "core time second conversion values" \
    "$core_expected" \
    "DO 0; SELECT TIME_TO_SEC('22:23:00'), TIME_TO_SEC('00:39:38'), "\
"TIME_TO_SEC('-00:39:38'), TIME_TO_SEC('100:00:00'), TIME_TO_SEC(NULL), "\
"SEC_TO_TIME(2378), SEC_TO_TIME(+2378), SEC_TO_TIME(-2378), "\
"SEC_TO_TIME(TRUE), SEC_TO_TIME(FALSE), SEC_TO_TIME(NULL), @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
secs	tm
2378	00:39:38
EXPECTED
)
expect_output_with_headers \
    "time second conversion labels and whitespace" \
    "$labels_expected" \
    "SELECT TIME_TO_SEC ('00:39:38') AS secs, SEC_TO_TIME (2378) AS tm FROM DUAL;" \
    "$DATABASE"

clipping_expected=$(cat <<EXPECTED
838:59:59
838:59:59	-838:59:59
Warning	1292	Truncated incorrect time value: '3020400'
Warning	1292	Truncated incorrect time value: '-3020400'
EXPECTED
)
expect_output \
    "SEC_TO_TIME clipping warnings" \
    "$clipping_expected" \
    "SELECT SEC_TO_TIME(3020399); "\
"SELECT SEC_TO_TIME(3020400), SEC_TO_TIME(-3020400); SHOW WARNINGS;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	0	2378	3723	3723	2378	00:39:38
2	NULL	-360000	0	NULL	NULL	-100:00:00
3	0	NULL	NULL	NULL	360000	838:59:59
Warning	1292	Truncated incorrect time value: 'not-a-time'
Warning	1292	Truncated incorrect time value: '3020400'
EXPECTED
)
expect_output \
    "descriptor-backed time second conversion values and warnings" \
    "$table_expected" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE t(id INT, d DATE NULL, tm TIME NULL, dt DATETIME NULL, "\
"ts TIMESTAMP NULL, v VARCHAR(32), secs INT NULL); "\
"INSERT INTO t VALUES "\
"(1,'2003-12-31','00:39:38','2003-12-31 01:02:03','2003-12-31 01:02:03','00:39:38',2378),"\
"(2,NULL,'-100:00:00','2003-12-31 00:00:00',NULL,'not-a-time',-360000),"\
"(3,'2003-12-31',NULL,NULL,NULL,'100:00:00',3020400); "\
"SELECT id, TIME_TO_SEC(d), TIME_TO_SEC(tm), TIME_TO_SEC(dt), TIME_TO_SEC(ts), "\
"TIME_TO_SEC(v), SEC_TO_TIME(secs) FROM t ORDER BY id; SHOW WARNINGS;" \
    "$DATABASE"

envelope_expected=$(cat <<EXPECTED
2	-360000	-100:00:00
1	2378	00:39:38
EXPECTED
)
expect_output \
    "row-scalar filtered ordered limited time second conversion values" \
    "$envelope_expected" \
    "SELECT id, TIME_TO_SEC(tm), SEC_TO_TIME(secs) FROM t "\
"WHERE id <= 2 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "time second conversion do status" \
    "$do_expected" \
    "DO TIME_TO_SEC('00:00:01'), SEC_TO_TIME(1); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

invalid_expected=$(cat <<EXPECTED
NULL
Warning	1292	Truncated incorrect time value: 'not-a-time'
EXPECTED
)
expect_output \
    "invalid TIME_TO_SEC warning" \
    "$invalid_expected" \
    "SELECT TIME_TO_SEC('not-a-time'); SHOW WARNINGS;" \
    "$DATABASE"

invalid_datetime_expected=$(cat <<EXPECTED
NULL
Warning	1292	Truncated incorrect time value: '2003-12-31 24:00:00'
NULL
Warning	1292	Truncated incorrect time value: '2003-12-31 24:00:00'
EXPECTED
)
expect_output \
    "invalid datetime TIME_TO_SEC warning" \
    "$invalid_datetime_expected" \
    "SELECT TIME_TO_SEC('2003-12-31 24:00:00'); SHOW WARNINGS; "\
"INSERT INTO t VALUES (4,NULL,NULL,NULL,NULL,'2003-12-31 24:00:00',0); "\
"SELECT TIME_TO_SEC(v) FROM t WHERE id = 4; SHOW WARNINGS;" \
    "$DATABASE"

expect_error \
    "TIME_TO_SEC empty argument count" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'TIME_TO_SEC'" \
    "SELECT TIME_TO_SEC() AS x;" \
    "$DATABASE"

expect_error \
    "TIME_TO_SEC extra argument count" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'TIME_TO_SEC'" \
    "SELECT TIME_TO_SEC('00:00:01', '00:00:02') AS x;" \
    "$DATABASE"

expect_error \
    "SEC_TO_TIME empty argument count" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'SEC_TO_TIME'" \
    "SELECT SEC_TO_TIME() AS x;" \
    "$DATABASE"

expect_error \
    "SEC_TO_TIME extra argument count" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'SEC_TO_TIME'" \
    "SELECT SEC_TO_TIME(1, 2) AS x;" \
    "$DATABASE"

expect_upstream_accepts \
    "numeric TIME_TO_SEC input deferred by MyLite" \
    "SELECT TIME_TO_SEC(123456);" \
    "$DATABASE"

expect_upstream_accepts \
    "date-only TIME_TO_SEC string coercion deferred by MyLite" \
    "SELECT TIME_TO_SEC('2003-12-31');" \
    "$DATABASE"

expect_upstream_accepts \
    "fractional TIME_TO_SEC input deferred by MyLite" \
    "SELECT TIME_TO_SEC('12:34:56.123456');" \
    "$DATABASE"

expect_upstream_accepts \
    "string SEC_TO_TIME input deferred by MyLite" \
    "SELECT SEC_TO_TIME('2378');" \
    "$DATABASE"

expect_upstream_accepts \
    "fractional SEC_TO_TIME input deferred by MyLite" \
    "SELECT SEC_TO_TIME(1.5);" \
    "$DATABASE"
