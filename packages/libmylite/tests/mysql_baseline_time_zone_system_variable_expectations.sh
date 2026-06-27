#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names"
DATABASE="mylite_time_zone_system_variable_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_time_zone_system_variable_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    expect_value "$label" "$expected" "$output"
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
    run_mysql "SET GLOBAL time_zone = 'SYSTEM';" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

expect_output \
    "default time zone values" \
    "SYSTEM	SYSTEM	SYSTEM	UTC	-1
time_zone	SYSTEM
time_zone	SYSTEM
system_time_zone	UTC
system_time_zone	UTC" \
    "SELECT @@GLOBAL.time_zone, @@SESSION.time_zone, @@time_zone,
            @@system_time_zone, ROW_COUNT();
     SHOW VARIABLES LIKE 'time_zone';
     SHOW GLOBAL VARIABLES LIKE 'time_zone';
     SHOW VARIABLES LIKE 'system_time_zone';
     SHOW GLOBAL VARIABLES LIKE 'system_time_zone';" \
    "$DATABASE"

expect_output \
    "session time zone assignment forms and normalization" \
    "+00:00	+00:00	SYSTEM	0	0
+05:30	0	0
-06:00	0	0
+14:00	0	0
-13:59	0	0
+00:00	0	0
+00:00	0	0
SYSTEM	SYSTEM	0	0
SYSTEM	0	0
UTC	0	0
UTC	0	0
SYSTEM	0	0" \
    "SET time_zone = '+00:00';
     SELECT @@time_zone, @@SESSION.time_zone, @@GLOBAL.time_zone, @@warning_count, ROW_COUNT();
     SET @@time_zone = '+5:30';
     SELECT @@time_zone, @@warning_count, ROW_COUNT();
     SET SESSION time_zone = '-6:00';
     SELECT @@time_zone, @@warning_count, ROW_COUNT();
     SET LOCAL time_zone = '+14:00';
     SELECT @@time_zone, @@warning_count, ROW_COUNT();
     SET @@SESSION.time_zone = '-13:59';
     SELECT @@time_zone, @@warning_count, ROW_COUNT();
     SET @@LOCAL.time_zone = '-0:00';
     SELECT @@time_zone, @@warning_count, ROW_COUNT();
     SET @@session.Time_Zone = '+0:00';
     SELECT @@time_zone, @@warning_count, ROW_COUNT();
     SET time_zone = DEFAULT;
     SELECT @@time_zone, @@GLOBAL.time_zone, @@warning_count, ROW_COUNT();
     SET time_zone = 'system';
     SELECT @@time_zone, @@warning_count, ROW_COUNT();
     SET time_zone = 'utc';
     SELECT @@time_zone, @@warning_count, ROW_COUNT();
     SET time_zone = UTC;
     SELECT @@time_zone, @@warning_count, ROW_COUNT();
     SET time_zone = SYSTEM;
     SELECT @@time_zone, @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "current time functions follow session offset" \
    "2023-11-14 22:13:20	2023-11-14	22:13:20	2023-11-14 22:13:20
2023-11-15 00:43:20	2023-11-15	00:43:20	2023-11-15 00:43:20
2023-11-14 16:13:20	2023-11-14	16:13:20	2023-11-14 16:13:20
UTC	2023-11-14 22:13:20	2023-11-14	22:13:20" \
    "SET timestamp = 1700000000;
     SET time_zone = '+00:00';
     SELECT NOW(), CURDATE(), CURTIME(), CURRENT_TIMESTAMP;
     SET time_zone = '+02:30';
     SELECT NOW(), CURDATE(), CURTIME(), CURRENT_TIMESTAMP;
     SET time_zone = '-06:00';
     SELECT NOW(), CURDATE(), CURTIME(), CURRENT_TIMESTAMP;
     SET time_zone = 'UTC';
     SELECT @@time_zone, NOW(), CURDATE(), CURTIME();" \
    "$DATABASE"

expect_output \
    "global time zone behavior deferred by mylite" \
    "SYSTEM	+02:30	0	0	0
SYSTEM	+02:30	0	0	0
SYSTEM	+02:30	0	0	0
+03:00	+02:30	0	0
+03:00	+03:00	0	0
SYSTEM	SYSTEM" \
    "SET time_zone = '+02:30';
     SET GLOBAL time_zone = DEFAULT;
     SELECT @@GLOBAL.time_zone, @@SESSION.time_zone, @@warning_count, @@error_count, ROW_COUNT();
     SET @@GLOBAL.time_zone = SYSTEM;
     SELECT @@GLOBAL.time_zone, @@SESSION.time_zone, @@warning_count, @@error_count, ROW_COUNT();
     SET GLOBAL time_zone = 'system';
     SELECT @@GLOBAL.time_zone, @@SESSION.time_zone, @@warning_count, @@error_count, ROW_COUNT();
     SET GLOBAL time_zone = '+03:00';
     SELECT @@GLOBAL.time_zone, @@SESSION.time_zone, @@warning_count, ROW_COUNT();
     SET time_zone = DEFAULT;
     SELECT @@GLOBAL.time_zone, @@SESSION.time_zone, @@warning_count, ROW_COUNT();
     SET GLOBAL time_zone = 'SYSTEM';
     SET time_zone = DEFAULT;
     SELECT @@GLOBAL.time_zone, @@SESSION.time_zone;" \
    "$DATABASE"

expect_output \
    "timestamp column conversion deferred by mylite" \
    "2024-01-02 03:04:05	2024-01-02 03:04:05
2024-01-02 01:04:05	2024-01-02 03:04:05" \
    "SET time_zone = '+02:00';
     CREATE TABLE timestamp_probe(ts TIMESTAMP, dt DATETIME);
     INSERT INTO timestamp_probe VALUES ('2024-01-02 03:04:05', '2024-01-02 03:04:05');
     SELECT ts, dt FROM timestamp_probe;
     SET time_zone = '+00:00';
     SELECT ts, dt FROM timestamp_probe;" \
    "$DATABASE"

expect_error \
    "session system_time_zone is global only" \
    1238 \
    HY000 \
    "Variable 'system_time_zone' is a GLOBAL variable" \
    "SELECT @@SESSION.system_time_zone;" \
    "$DATABASE"

expect_error \
    "set system_time_zone is read only" \
    1238 \
    HY000 \
    "Variable 'system_time_zone' is a read only variable" \
    "SET system_time_zone = 'UTC';" \
    "$DATABASE"

expect_error "bad offset high" 1298 HY000 "Unknown or incorrect time zone: '+14:01'" \
    "SET time_zone = '+14:01';" "$DATABASE"
expect_error "bad offset low" 1298 HY000 "Unknown or incorrect time zone: '-14:00'" \
    "SET time_zone = '-14:00';" "$DATABASE"
expect_error "bad offset minutes" 1298 HY000 "Unknown or incorrect time zone: '+00:60'" \
    "SET time_zone = '+00:60';" "$DATABASE"
expect_error "bad offset shape" 1298 HY000 "Unknown or incorrect time zone: '+0'" \
    "SET time_zone = '+0';" "$DATABASE"
expect_error "bad no sign" 1298 HY000 "Unknown or incorrect time zone: '00:00'" \
    "SET time_zone = '00:00';" "$DATABASE"
expect_error "bad empty" 1298 HY000 "Unknown or incorrect time zone: ''" \
    "SET time_zone = '';" "$DATABASE"
expect_error "bad numeric" 1232 42000 "Incorrect argument type to variable 'time_zone'" \
    "SET time_zone = 0;" "$DATABASE"
expect_error "bad null" 1231 42000 "Variable 'time_zone' can't be set to the value of 'NULL'" \
    "SET time_zone = NULL;" "$DATABASE"

printf '%s\n' "mysql_baseline_time_zone_system_variable_expectations: ok"
