#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_addtime_subtime_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_addtime_subtime_functions_expectations: $1" >&2
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
2008-01-02 13:29:18	2008-01-02 13:29:16	2008-01-02 13:29:16	2008-01-02 13:29:18	NULL	NULL	0
-1	0
EXPECTED
)
expect_output \
    "core addtime subtime datetime values" \
    "$core_expected" \
    "USE ${DATABASE}; DO 0; "\
"SELECT ADDTIME('2008-01-02 13:29:17','00:00:01'), "\
"ADDTIME('2008-01-02 13:29:17','-00:00:01'), "\
"SUBTIME('2008-01-02 13:29:17','00:00:01'), "\
"SUBTIME('2008-01-02 13:29:17','-00:00:01'), "\
"ADDTIME(NULL,'bad'), ADDTIME('01:02:03', NULL), @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;"

time_expected=$(cat <<EXPECTED
01:02:07	01:01:59	-01:01:59	-01:02:07	101:02:03	838:59:59	-838:59:59	0
EXPECTED
)
expect_output \
    "core addtime subtime time values" \
    "$time_expected" \
    "USE ${DATABASE}; DO 0; "\
"SELECT ADDTIME('01:02:03','00:00:04'), SUBTIME('01:02:03','00:00:04'), "\
"ADDTIME('-01:02:03','00:00:04'), SUBTIME('-01:02:03','00:00:04'), "\
"ADDTIME('01:02:03','100:00:00'), ADDTIME('838:59:58','00:00:01'), "\
"SUBTIME('-838:59:58','00:00:01'), @@warning_count;"

label_expected=$(cat <<EXPECTED
ADDTIME('01:02:03','00:00:01')	shifted
01:02:04	2008-01-02 13:29:16
EXPECTED
)
expect_output_with_headers \
    "addtime subtime labels" \
    "$label_expected" \
    "USE ${DATABASE}; "\
"SELECT ADDTIME('01:02:03','00:00:01'), "\
"SUBTIME('2008-01-02 13:29:17','00:00:01') AS shifted FROM DUAL;"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "addtime subtime do status" \
    "$do_expected" \
    "USE ${DATABASE}; "\
"DO ADDTIME('01:02:03','00:00:01'), SUBTIME(NULL,'bad'); "\
"SELECT ROW_COUNT(), @@warning_count;"

expect_output \
    "default mode accepts whitespace before paren" \
    "01:02:04	01:02:02" \
    "USE ${DATABASE}; SET SESSION sql_mode = ''; "\
"SELECT ADDTIME ('01:02:03','00:00:01'), SUBTIME ('01:02:03','00:00:01');"

expect_output \
    "ignore_space accepts whitespace before paren" \
    "01:02:04	01:02:02" \
    "USE ${DATABASE}; SET SESSION sql_mode = 'IGNORE_SPACE'; "\
"SELECT ADDTIME ('01:02:03','00:00:01'), SUBTIME ('01:02:03','00:00:01');"

expect_output \
    "default and ignore_space allow function names as table identifiers" \
    "$(cat <<EXPECTED
addtime
subtime
addtime
subtime
EXPECTED
)" \
    "USE ${DATABASE}; SET SESSION sql_mode = ''; "\
"DROP TABLE IF EXISTS addtime; DROP TABLE IF EXISTS subtime; "\
"CREATE TABLE addtime(id INT); CREATE TABLE subtime(id INT); "\
"SELECT table_name FROM information_schema.tables "\
"WHERE table_schema = '${DATABASE}' AND table_name IN ('addtime', 'subtime') "\
"ORDER BY table_name; "\
"DROP TABLE addtime; DROP TABLE subtime; "\
"SET SESSION sql_mode = 'IGNORE_SPACE'; "\
"CREATE TABLE addtime(id INT); CREATE TABLE subtime(id INT); "\
"SELECT table_name FROM information_schema.tables "\
"WHERE table_schema = '${DATABASE}' AND table_name IN ('addtime', 'subtime') "\
"ORDER BY table_name; "\
"DROP TABLE addtime; DROP TABLE subtime;"

expect_error \
    "addtime no args parameter count" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "USE ${DATABASE}; SELECT ADDTIME();"

expect_error \
    "addtime one arg parameter count" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "USE ${DATABASE}; SELECT ADDTIME('01:02:03');"

expect_error \
    "addtime three args parameter count" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "USE ${DATABASE}; SELECT ADDTIME('01:02:03','00:00:01','x');"

expect_error \
    "subtime no args parameter count" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "USE ${DATABASE}; SELECT SUBTIME();"

expect_error \
    "subtime one arg parameter count" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "USE ${DATABASE}; SELECT SUBTIME('01:02:03');"

expect_error \
    "subtime three args parameter count" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "USE ${DATABASE}; SELECT SUBTIME('01:02:03','00:00:01','x');"

expect_error \
    "ansi quotes makes double quoted operands identifiers" \
    1054 \
    "42S22" \
    "Unknown column" \
    "USE ${DATABASE}; SET SESSION sql_mode = 'ANSI_QUOTES'; "\
"SELECT ADDTIME(\"01:02:03\", \"00:00:01\");"

row_context_expected=$(cat <<EXPECTED
1	2008-01-02 13:29:21	2008-01-02 13:29:13	01:02:07	01:01:59
2	2008-01-02 00:30:00	2008-01-01 23:30:00	-00:30:00	-01:30:00
1
1
1	2008-01-02 13:29:13	01:02:07
2	2008-01-01 23:30:00	-00:30:00
EXPECTED
)
expect_output \
    "row-backed addtime subtime contexts" \
    "$row_context_expected" \
    "USE ${DATABASE}; SET SESSION sql_mode = ''; "\
"DROP TABLE IF EXISTS shifts; "\
"CREATE TABLE shifts("\
"id INT, dt DATETIME, tm TIME, delta TIME, out_dt VARCHAR(32), out_tm VARCHAR(32)); "\
"INSERT INTO shifts VALUES "\
"(1, '2008-01-02 13:29:17', '01:02:03', '00:00:04', NULL, NULL), "\
"(2, '2008-01-02 00:00:00', '-01:00:00', '00:30:00', NULL, NULL); "\
"SELECT id, ADDTIME(dt, delta), SUBTIME(dt, delta), ADDTIME(tm, delta), SUBTIME(tm, delta) "\
"FROM shifts ORDER BY id; "\
"SELECT id FROM shifts WHERE ADDTIME(tm, delta) = '01:02:07'; "\
"SELECT id FROM shifts ORDER BY ADDTIME(tm, delta) DESC LIMIT 1; "\
"UPDATE shifts SET out_dt = SUBTIME(dt, delta), out_tm = ADDTIME(tm, delta); "\
"SELECT id, out_dt, out_tm FROM shifts ORDER BY id;"

deferred_expected=$(cat <<EXPECTED
00:00:02	01:02:04	27:05:07	838:59:59	-838:59:59
EXPECTED
)
expect_output \
    "mysql accepts broader deferred operand and clamp behavior" \
    "$deferred_expected" \
    "USE ${DATABASE}; SET SESSION sql_mode = ''; "\
"SELECT ADDTIME(1,'00:00:01'), ADDTIME('01:02:03', 1), "\
"ADDTIME('01:02:03', '1 02:03:04'), "\
"ADDTIME('838:59:59','00:00:01'), SUBTIME('-838:59:59','00:00:01');"

printf '%s\n' "mysql_baseline_addtime_subtime_functions_expectations: ok"
