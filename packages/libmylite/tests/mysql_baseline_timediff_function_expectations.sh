#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_timediff_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_timediff_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "${MYSQL_BIN:-mysql}" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
        return
    fi
    if [ -n "$MYSQL_BIN" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot --default-character-set=utf8mb4 \
                --batch --raw --skip-column-names "$@"
        return
    fi
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "${MYSQL_BIN:-mysql}" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --default-character-set=utf8mb4 --batch --raw "$@"
        return
    fi
    if [ -n "$MYSQL_BIN" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot --default-character-set=utf8mb4 \
                --batch --raw "$@"
        return
    fi
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
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
00:00:01	-00:00:01	01:01:59	-01:01:59	-01:02:07	100:00:00	NULL	NULL	0
-1	0
EXPECTED
)
expect_output \
    "core timediff scalar values" \
    "$core_expected" \
    "USE ${DATABASE}; DO 0; "\
"SELECT TIMEDIFF('2008-01-02 13:29:17','2008-01-02 13:29:16'), "\
"TIMEDIFF('2008-01-02 13:29:16','2008-01-02 13:29:17'), "\
"TIMEDIFF('01:02:03','00:00:04'), TIMEDIFF('00:00:04','01:02:03'), "\
"TIMEDIFF('-01:02:03','00:00:04'), TIMEDIFF('101:02:03','01:02:03'), "\
"TIMEDIFF(NULL,'bad'), TIMEDIFF('01:02:03', NULL), @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;"

mixed_expected=$(cat <<EXPECTED
NULL	NULL	0
0
EXPECTED
)
expect_output \
    "mixed scalar timediff domains" \
    "$mixed_expected" \
    "USE ${DATABASE}; DO 0; "\
"SELECT TIMEDIFF('2008-01-02 13:29:17','01:02:03'), "\
"TIMEDIFF('01:02:03','2008-01-02 13:29:17'), @@warning_count; "\
"SELECT @@warning_count;"

clamp_expected=$(cat <<EXPECTED
838:59:59	-838:59:59
2
Warning	1292	Truncated incorrect time value: '839:00:00'
Warning	1292	Truncated incorrect time value: '-839:00:00'
EXPECTED
)
expect_output \
    "timediff clamps out of range time results" \
    "$clamp_expected" \
"USE ${DATABASE}; DO 0; "\
"SELECT TIMEDIFF('838:59:59','-00:00:01'), "\
"TIMEDIFF('-838:59:59','00:00:01'); "\
"SHOW COUNT(*) WARNINGS; SHOW WARNINGS;"

invalid_expected=$(cat <<EXPECTED
NULL	NULL
2
Warning	1292	Truncated incorrect time value: 'bad'
Warning	1292	Truncated incorrect time value: 'bad'
EXPECTED
)
expect_output \
    "timediff invalid strings warn" \
    "$invalid_expected" \
"USE ${DATABASE}; DO 0; "\
"SELECT TIMEDIFF('bad','00:00:01'), TIMEDIFF('00:00:01','bad'); "\
"SHOW COUNT(*) WARNINGS; SHOW WARNINGS;"

label_expected=$(cat <<EXPECTED
TIMEDIFF('01:02:03','00:00:01')	shifted
01:02:02	00:00:01
EXPECTED
)
expect_output_with_headers \
    "timediff labels" \
    "$label_expected" \
    "USE ${DATABASE}; "\
"SELECT TIMEDIFF('01:02:03','00:00:01'), "\
"TIMEDIFF('2008-01-02 13:29:17','2008-01-02 13:29:16') AS shifted FROM DUAL;"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "timediff do status" \
    "$do_expected" \
    "USE ${DATABASE}; "\
"DO TIMEDIFF('01:02:03','00:00:01'), TIMEDIFF(NULL,'bad'); "\
"SELECT ROW_COUNT(), @@warning_count;"

expect_output \
    "default mode accepts whitespace before paren" \
    "01:02:02" \
    "USE ${DATABASE}; SET SESSION sql_mode = ''; "\
"SELECT TIMEDIFF ('01:02:03','00:00:01');"

expect_output \
    "ignore_space accepts whitespace before paren" \
    "01:02:02" \
    "USE ${DATABASE}; SET SESSION sql_mode = 'IGNORE_SPACE'; "\
"SELECT TIMEDIFF ('01:02:03','00:00:01');"

expect_output \
    "default and ignore_space allow function name as table identifier" \
    "$(cat <<EXPECTED
timediff
timediff
EXPECTED
)" \
    "USE ${DATABASE}; SET SESSION sql_mode = ''; "\
"DROP TABLE IF EXISTS timediff; CREATE TABLE timediff(id INT); "\
"SELECT table_name FROM information_schema.tables "\
"WHERE table_schema = '${DATABASE}' AND table_name = 'timediff'; "\
"DROP TABLE timediff; SET SESSION sql_mode = 'IGNORE_SPACE'; "\
"CREATE TABLE timediff(id INT); "\
"SELECT table_name FROM information_schema.tables "\
"WHERE table_schema = '${DATABASE}' AND table_name = 'timediff'; "\
"DROP TABLE timediff;"

run_mysql \
    "USE ${DATABASE}; "\
"CREATE TABLE t(id INT, d DATE NULL, d2 DATE NULL, tm TIME NULL, tm2 TIME NULL, "\
"dt DATETIME NULL, dt2 DATETIME NULL, ts TIMESTAMP NULL DEFAULT NULL); "\
"INSERT INTO t VALUES "\
"(1,'2008-01-03','2008-01-01','01:02:03','00:00:04',"\
"'2008-01-02 13:29:17','2008-01-02 13:29:16','2008-01-02 13:29:15'), "\
"(2,NULL,'2008-01-01',NULL,'00:00:04',NULL,'2008-01-02 13:29:16',NULL);" \
    >/dev/null

table_expected=$(cat <<EXPECTED
1	48:00:00	01:01:59	00:00:01	00:00:02	NULL	NULL	0
2	NULL	NULL	NULL	NULL	NULL	NULL	0
EXPECTED
)
expect_output \
    "table backed timediff temporal descriptors" \
    "$table_expected" \
    "USE ${DATABASE}; "\
"SELECT id, TIMEDIFF(d,d2), TIMEDIFF(tm,tm2), TIMEDIFF(dt,dt2), "\
"TIMEDIFF(dt,ts), TIMEDIFF(d,dt), TIMEDIFF(tm,dt), @@warning_count FROM t ORDER BY id;"

expect_error \
    "timediff no args parameter count" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "USE ${DATABASE}; SELECT TIMEDIFF();"

expect_error \
    "timediff one arg parameter count" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "USE ${DATABASE}; SELECT TIMEDIFF('01:02:03');"

expect_error \
    "timediff three args parameter count" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "USE ${DATABASE}; SELECT TIMEDIFF('01:02:03','00:00:01','x');"

expect_error \
    "ansi quotes makes double quoted operands identifiers" \
    1054 \
    "42S22" \
    "Unknown column" \
    "USE ${DATABASE}; SET SESSION sql_mode = 'ANSI_QUOTES'; "\
"SELECT TIMEDIFF(\"01:02:03\", \"00:00:01\");"

deferred_expected=$(cat <<EXPECTED
-00:00:01	101:01:59	01:02:00.123455	0
EXPECTED
)
expect_output \
    "mysql accepts broader deferred operands" \
    "$deferred_expected" \
    "USE ${DATABASE}; SET SESSION sql_mode = ''; "\
"SELECT TIMEDIFF(1,2), TIMEDIFF(1010203,4), "\
"TIMEDIFF('01:02:03.123456','00:00:03.000001'), @@warning_count;"

printf '%s\n' "mysql_baseline_timediff_function_expectations: ok"
