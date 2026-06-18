#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_string_bitmask_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_string_bitmask_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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
run_mysql \
    "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"USE ${DATABASE}; SET NAMES utf8mb4; SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION';" \
    >/dev/null

scalar_expected=$(cat <<EXPECTED
Y,N,Y,N	N,N,N,N	[]	NULL	NULL	Y	N	1,0,1,0	a	a,b	[]	NULL	a,c	a,b,c	a	[]	1,1	0
0	0
EXPECTED
)
expect_output \
    "scalar bitmask values" \
    "$scalar_expected" \
    "DO 0; SELECT EXPORT_SET(5,'Y','N',',',4), EXPORT_SET(0,'Y','N',',',4), "\
"CONCAT('[',EXPORT_SET(1,'Y','N',',',0),']'), EXPORT_SET(NULL,'Y','N',',',4), "\
"EXPORT_SET(1,NULL,'N',',',4), EXPORT_SET(TRUE,'Y','N',',',TRUE), "\
"EXPORT_SET(FALSE,'Y','N',',',TRUE), EXPORT_SET(5,1,0,',',4), "\
"MAKE_SET(1,'a','b','c'), MAKE_SET(3,'a','b','c'), "\
"CONCAT('[',MAKE_SET(0,'a','b'),']'), MAKE_SET(NULL,'a'), MAKE_SET(7,'a',NULL,'c'), "\
"MAKE_SET(-1,'a','b','c'), MAKE_SET(TRUE,'a','b'), "\
"CONCAT('[',MAKE_SET(FALSE,'a','b'),']'), MAKE_SET(3,1,TRUE,NULL), @@warning_count; "\
"DO EXPORT_SET(5,'Y','N',',',4), MAKE_SET(3,'a','b'); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

default_export_expected=$(cat <<EXPECTED
Y,N,Y,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N	Y,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N
EXPECTED
)
expect_output \
    "default and clamped export set count" \
    "$default_export_expected" \
    "SELECT EXPORT_SET(5,'Y','N'), EXPORT_SET(1,'Y','N',',',-1);" \
    "$DATABASE"

expect_output \
    "scalar integer function bitmask arguments" \
    "Y:N	a,b" \
    "SELECT EXPORT_SET(ABS(-5),'Y','N',':',BIT_COUNT(3)), MAKE_SET(ABS(-3),'a','b','c');" \
    "$DATABASE"

expect_output \
    "scalar integer arithmetic bitmask arguments" \
    "Y:N	Y	a" \
    "SELECT EXPORT_SET(1 + 0,'Y','N',':',2), "\
"EXPORT_SET(1,'Y','N',':',1 + 0), MAKE_SET(1 + 0,'a','b');" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t("\
"id INT, bits INT, on_label VARCHAR(20), off_label VARCHAR(20), sep CHAR(1), txt TEXT, i INT"\
"); "\
"INSERT INTO t VALUES "\
"(1, 5, 'Y', 'N', ':', 'first', 10), "\
"(2, 2, 'on', 'off', '|', 'second', -7), "\
"(3, NULL, 'Y', 'N', ',', NULL, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<EXPECTED
1	Y:N:Y:N	first,10
2	off|on|off|off	on
3	NULL	NULL
EXPECTED
)
expect_output \
    "table bitmask projection" \
    "$table_expected" \
    "SELECT id, EXPORT_SET(bits, on_label, off_label, sep, 4), "\
"MAKE_SET(bits, txt, on_label, i) FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table bitmask row envelope" \
    "3	NULL
2	on" \
    "SELECT id, MAKE_SET(bits, off_label, on_label) FROM t "\
"WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

expect_output \
    "table integer expression bitmask arguments" \
    "1	N:Y:Y	Y
2	off|off|on|off	second,on,off
3	NULL	NULL" \
    "SELECT id, EXPORT_SET(bits + id, on_label, off_label, sep, id + 2), "\
"MAKE_SET(ABS(i), txt, on_label, off_label) FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table predicate and order expression" \
    "1
1
3	NULL
1	N
2	on" \
    "SELECT COUNT(*) FROM t WHERE MAKE_SET(bits, 'a') = 'a'; "\
"SELECT id FROM t WHERE EXPORT_SET(bits, on_label, off_label, sep, 1) = on_label ORDER BY id; "\
"SELECT id, MAKE_SET(bits, off_label, on_label) FROM t "\
"ORDER BY MAKE_SET(bits, off_label, on_label), id;" \
    "$DATABASE"

expect_error \
    "export_set rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'EXPORT_SET'" \
    "SELECT EXPORT_SET();" \
    "$DATABASE"

expect_error \
    "export_set rejects too few arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'EXPORT_SET'" \
    "SELECT EXPORT_SET(1,'Y');" \
    "$DATABASE"

expect_error \
    "export_set rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'EXPORT_SET'" \
    "SELECT EXPORT_SET(1,'Y','N',',',4,5);" \
    "$DATABASE"

expect_error \
    "make_set rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'MAKE_SET'" \
    "SELECT MAKE_SET();" \
    "$DATABASE"

expect_error \
    "make_set rejects one argument" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'MAKE_SET'" \
    "SELECT MAKE_SET(1);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_string_bitmask_functions_expectations: ok"
