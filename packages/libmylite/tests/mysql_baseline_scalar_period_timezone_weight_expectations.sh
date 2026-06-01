#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_scalar_period_timezone_weight_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_scalar_period_timezone_weight_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; SET SESSION sql_mode = '';" >/dev/null

period_expected=$(cat <<EXPECTED
200803	200002	196912	200102	11	-1	NULL	NULL
0
EXPECTED
)
expect_output \
    "period values" \
    "$period_expected" \
    "SELECT PERIOD_ADD(200801,2), PERIOD_ADD(9912,2), PERIOD_ADD(7001,-1), "\
"PERIOD_ADD(101,1), PERIOD_DIFF(200802,200703), PERIOD_DIFF(9912,0001), "\
"PERIOD_ADD(NULL,1), PERIOD_DIFF(200801,NULL); SELECT @@warning_count;" \
    "$DATABASE"

expect_error \
    "period add zero period" \
    1210 \
    HY000 \
    "Incorrect arguments to period_add" \
    "SELECT PERIOD_ADD(0,1);" \
    "$DATABASE"
expect_error \
    "period add month out of range" \
    1210 \
    HY000 \
    "Incorrect arguments to period_add" \
    "SELECT PERIOD_ADD(200813,1);" \
    "$DATABASE"
expect_error \
    "period diff bad period" \
    1210 \
    HY000 \
    "Incorrect arguments to period_diff" \
    "SELECT PERIOD_DIFF(200801,200713);" \
    "$DATABASE"

period_coercion_expected=$(cat <<EXPECTED
200803	1	200804
Warning	1292	Truncated incorrect INTEGER value: '200801abc'
Warning	1292	Truncated incorrect INTEGER value: '2008-02'
Warning	1292	Truncated incorrect INTEGER value: '2007-03'
EXPECTED
)
expect_output \
    "period coercion deferred surface" \
    "$period_coercion_expected" \
    "SELECT PERIOD_ADD('200801abc',2), PERIOD_DIFF('2008-02','2007-03'), "\
"PERIOD_ADD(200801.9,2); SHOW WARNINGS;" \
    "$DATABASE"

convert_expected=$(cat <<EXPECTED
2004-01-01 14:30:00	2004-01-01 09:30:00	NULL	NULL	NULL
Warning	1292	Incorrect datetime value: 'bad'
EXPECTED
)
expect_output \
    "convert tz values and warnings" \
    "$convert_expected" \
    "SELECT CONVERT_TZ('2004-01-01 12:00:00','+00:00','+02:30'), "\
"CONVERT_TZ('2004-01-01 12:00:00','+02:30','+00:00'), "\
"CONVERT_TZ(NULL,'+00:00','+01:00'), "\
"CONVERT_TZ('bad','+00:00','+01:00'), "\
"CONVERT_TZ('2004-01-01 12:00:00','bad','+01:00'); SHOW WARNINGS;" \
    "$DATABASE"

weight_expected=$(cat <<EXPECTED
NULL	4142	61620000	616263	1C471C60
Warning	1292	Truncated incorrect BINARY(3) value: 'abcdef'
EXPECTED
)
expect_output \
    "weight string values and warnings" \
    "$weight_expected" \
    "SELECT HEX(WEIGHT_STRING(NULL)), HEX(WEIGHT_STRING(CAST('AB' AS BINARY))), "\
"HEX(WEIGHT_STRING('ab' AS BINARY(4))), HEX(WEIGHT_STRING('abcdef' AS BINARY(3))), "\
"HEX(WEIGHT_STRING(_utf8mb4'AB' COLLATE utf8mb4_0900_ai_ci)); SHOW WARNINGS;" \
    "$DATABASE"

expect_error \
    "period add argument count" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'PERIOD_ADD'" \
    "SELECT PERIOD_ADD(1);" \
    "$DATABASE"
expect_error \
    "convert_tz argument count" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CONVERT_TZ'" \
    "SELECT CONVERT_TZ('2004-01-01 12:00:00','+00:00');" \
    "$DATABASE"
expect_error \
    "weight string empty parse error" \
    1064 \
    42000 \
    "near ')' at line 1" \
    "SELECT WEIGHT_STRING();" \
    "$DATABASE"

expect_upstream_accepts \
    "named timezone deferred by MyLite" \
    "SELECT CONVERT_TZ('2004-01-01 12:00:00','UTC','Europe/Berlin');" \
    "$DATABASE"
expect_upstream_accepts \
    "weight string char form deferred by MyLite" \
    "SELECT HEX(WEIGHT_STRING('ab' AS CHAR(4)));" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_scalar_period_timezone_weight_expectations: ok"
