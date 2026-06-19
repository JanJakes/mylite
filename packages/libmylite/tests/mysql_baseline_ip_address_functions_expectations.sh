#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_ip_address_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_ip_address_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 --skip-column-names \
            --default-character-set=utf8mb4 "$@"
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; USE ${DATABASE}; SET NAMES utf8mb4; SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION';" >/dev/null

aton_expected=$(cat <<EXPECTED
2130706433	2130706433	167772161	1	0	4294967295	NULL	123	1	0	16777221	2130706433	0
EXPECTED
)
expect_output \
    "inet_aton scalar values" \
    "$aton_expected" \
    "DO 0; SELECT INET_ATON('127.0.0.1'), INET_ATON('127.1'), "\
"INET_ATON('10.0.1'), INET_ATON('1'), INET_ATON('0.0.0.0'), "\
"INET_ATON('255.255.255.255'), INET_ATON(NULL), INET_ATON(123), "\
"INET_ATON(TRUE), INET_ATON(FALSE), INET_ATON(1.5), "\
"INET_ATON(X'3132372E302E302E31'), @@warning_count;" \
    "$DATABASE"

aton_short_expected=$(cat <<EXPECTED
16909060	1	1	16777218	16908292	0	0
EXPECTED
)
expect_output \
    "inet_aton accepted short forms" \
    "$aton_short_expected" \
    "DO 0; SELECT INET_ATON('01.002.003.004'), INET_ATON('.1'), "\
"INET_ATON('..1'), INET_ATON('1..2'), INET_ATON('1.2..4'), "\
"INET_ATON('0..0'), @@warning_count;" \
    "$DATABASE"

aton_invalid_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
Warning	1411	Incorrect string value: ''256.1.1.1'' for function inet_aton
Warning	1411	Incorrect string value: ''1.2.3.4.5'' for function inet_aton
Warning	1411	Incorrect string value: ''1.2.3.'' for function inet_aton
Warning	1411	Incorrect string value: ''abc'' for function inet_aton
Warning	1411	Incorrect string value: '' 1.2.3.4'' for function inet_aton
Warning	1411	Incorrect string value: ''1.2.3.4 '' for function inet_aton
Warning	1411	Incorrect string value: ''1.256'' for function inet_aton
Warning	1411	Incorrect string value: ''4294967295'' for function inet_aton
EXPECTED
)
expect_output \
    "inet_aton invalid warnings" \
    "$aton_invalid_expected" \
"DO 0; SELECT INET_ATON('256.1.1.1'), INET_ATON('1.2.3.4.5'), "\
"INET_ATON('1.2.3.'), INET_ATON('abc'), INET_ATON(' 1.2.3.4'), "\
"INET_ATON('1.2.3.4 '), INET_ATON('1.256'), INET_ATON('4294967295'); "\
"SHOW WARNINGS;" \
    "$DATABASE"

ntoa_expected=$(cat <<EXPECTED
127.0.0.1	10.0.0.1	0.0.0.1	0.0.0.0	255.255.255.255	NULL	127.0.0.1	0.0.0.0	0.0.0.2	0
EXPECTED
)
expect_output \
    "inet_ntoa scalar values" \
    "$ntoa_expected" \
    "DO 0; SELECT INET_NTOA(2130706433), INET_NTOA(167772161), "\
"INET_NTOA(1), INET_NTOA(0), INET_NTOA(4294967295), INET_NTOA(NULL), "\
"INET_NTOA('2130706433'), INET_NTOA(FALSE), INET_NTOA(1.5), @@warning_count;" \
    "$DATABASE"

ntoa_warning_expected=$(cat <<EXPECTED
NULL	NULL	NULL	0.0.0.0	0.0.0.123	0.0.0.1	0.0.0.1	0.0.0.0
Warning	1411	Incorrect integer value: '-(1)' for function inet_ntoa
Warning	1411	Incorrect integer value: '4294967296' for function inet_ntoa
Warning	1292	Truncated incorrect INTEGER value: '4294967296abc'
Warning	1411	Incorrect integer value: ''4294967296abc'' for function inet_ntoa
Warning	1292	Truncated incorrect INTEGER value: 'abc'
Warning	1292	Truncated incorrect INTEGER value: '123abc'
Warning	1292	Truncated incorrect INTEGER value: '1.5'
Warning	1292	Truncated incorrect INTEGER value: '1.4'
Warning	1292	Truncated incorrect BINARY value: 'x'32313330373036343333''
EXPECTED
)
expect_output \
    "inet_ntoa warnings" \
    "$ntoa_warning_expected" \
    "DO 0; SELECT INET_NTOA(-1), INET_NTOA(4294967296), "\
"INET_NTOA('4294967296abc'), INET_NTOA('abc'), "\
"INET_NTOA('123abc'), INET_NTOA('1.5'), INET_NTOA('1.4'), "\
"INET_NTOA(X'32313330373036343333'); SHOW WARNINGS;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t(id INT PRIMARY KEY, ip VARCHAR(32), n BIGINT UNSIGNED); "\
"INSERT INTO t VALUES "\
"(1, '127.0.0.1', 2130706433), "\
"(2, '10.0.1', 167772161), "\
"(3, 'bad', 4294967295), "\
"(4, NULL, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<EXPECTED
1	2130706433	127.0.0.1
2	167772161	10.0.0.1
3	NULL	255.255.255.255
4	NULL	NULL
EXPECTED
)
expect_output \
    "table-backed ip address functions" \
    "$table_expected" \
    "DO 0; SELECT id, INET_ATON(ip), INET_NTOA(n) FROM t ORDER BY id;" \
    "$DATABASE"

predicate_expected=$(cat <<EXPECTED
3
1
EXPECTED
)
expect_output \
    "table-backed predicate and order" \
    "$predicate_expected" \
    "DO 0; SELECT id FROM t WHERE INET_ATON(ip) = 2130706433 "\
"OR INET_NTOA(n) = '255.255.255.255' ORDER BY INET_ATON(ip), id;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "do statement" \
    "$do_expected" \
    "DO INET_ATON('127.0.0.1'), INET_NTOA(1); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

assignment_dml_expected=$(cat <<EXPECTED
2130706433	0.0.0.1
2130706433	0.0.0.1
EXPECTED
)
expect_output \
    "assignment and source-free dml contexts" \
    "$assignment_dml_expected" \
    "SET @aton = INET_ATON('127.0.0.1'); SET @ntoa = INET_NTOA(1); "\
"SELECT @aton, @ntoa; CREATE TABLE values_t(a BIGINT UNSIGNED, n VARCHAR(32)); "\
"INSERT INTO values_t VALUES (INET_ATON('127.0.0.1'), INET_NTOA(1)); "\
"SELECT a, n FROM values_t;" \
    "$DATABASE"

expect_error \
    "inet_aton rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'INET_ATON'" \
    "SELECT INET_ATON();" \
    "$DATABASE"

expect_error \
    "inet_aton rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'INET_ATON'" \
    "SELECT INET_ATON('1.2.3.4', 'x');" \
    "$DATABASE"

expect_error \
    "inet_ntoa rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'INET_NTOA'" \
    "SELECT INET_NTOA();" \
    "$DATABASE"

expect_error \
    "inet_ntoa rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'INET_NTOA'" \
    "SELECT INET_NTOA(1, 2);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_ip_address_functions_expectations: ok"
