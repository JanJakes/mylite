#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_current_timestamp_defaults_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_current_timestamp_defaults_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "current timestamp synonyms use statement timestamp" \
    "functions	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20" \
    "SET time_zone = '+00:00'; SET timestamp = 1700000000; "\
"SELECT 'functions', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP(), NOW(), "\
"LOCALTIME, LOCALTIME(), LOCALTIMESTAMP, LOCALTIMESTAMP();" \
    "$DATABASE"

expect_output \
    "timestamp variable assignments and readback" \
    "plain	1700000000.000000	2023-11-14 22:13:20
at_at	1700000060.000000	2023-11-14 22:14:20
session	1700000120.000000	2023-11-14 22:15:20
at_at_session	1700000180.000000	2023-11-14 22:16:20
plus	1700000240.000000	2023-11-14 22:17:20
max	2147483647.000000	2038-01-19 03:14:07" \
    "SET time_zone = '+00:00'; "\
"SET timestamp = 1700000000; SELECT 'plain', @@timestamp, NOW(); "\
"SET @@timestamp = 1700000060; SELECT 'at_at', @@timestamp, NOW(); "\
"SET SESSION timestamp = 1700000120; SELECT 'session', @@timestamp, NOW(); "\
"SET @@SESSION.timestamp = 1700000180; SELECT 'at_at_session', @@timestamp, NOW(); "\
"SET timestamp = +1700000240; SELECT 'plus', @@timestamp, NOW(); "\
"SET timestamp = 2147483647; SELECT 'max', @@timestamp, NOW();" \
    "$DATABASE"

expect_error \
    "timestamp system variable rejects out of range" \
    1231 \
    "42000" \
    "Variable 'timestamp' can't be set to the value of '2147483648'" \
    "SET timestamp = 2147483648;" \
    "$DATABASE"

run_mysql \
    "SET time_zone = '+00:00'; SET timestamp = 1700000000; "\
"CREATE TABLE automatic_temporals ("\
"id INT, "\
"ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "\
"dt DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "\
"ts_init TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "\
"dt_init DATETIME DEFAULT CURRENT_TIMESTAMP, "\
"ts_up TIMESTAMP NULL ON UPDATE CURRENT_TIMESTAMP, "\
"dt_up DATETIME ON UPDATE CURRENT_TIMESTAMP);" \
    "$DATABASE" >/dev/null

show_columns_expected=$(
    printf 'id\tint\tYES\t\tNULL\t\n'
    printf 'ts\ttimestamp\tYES\t\tCURRENT_TIMESTAMP\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP\n'
    printf 'dt\tdatetime\tYES\t\tCURRENT_TIMESTAMP\tDEFAULT_GENERATED on update CURRENT_TIMESTAMP\n'
    printf 'ts_init\ttimestamp\tYES\t\tCURRENT_TIMESTAMP\tDEFAULT_GENERATED\n'
    printf 'dt_init\tdatetime\tYES\t\tCURRENT_TIMESTAMP\tDEFAULT_GENERATED\n'
    printf 'ts_up\ttimestamp\tYES\t\tNULL\ton update CURRENT_TIMESTAMP\n'
    printf 'dt_up\tdatetime\tYES\t\tNULL\ton update CURRENT_TIMESTAMP'
)
expect_output \
    "show columns renders current timestamp metadata" \
    "$show_columns_expected" \
    "SET time_zone = '+00:00'; SHOW COLUMNS FROM automatic_temporals;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
automatic_temporals	CREATE TABLE `automatic_temporals` (
  `id` int DEFAULT NULL,
  `ts` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `dt` datetime DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `ts_init` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `dt_init` datetime DEFAULT CURRENT_TIMESTAMP,
  `ts_up` timestamp NULL DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
  `dt_up` datetime DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders current timestamp metadata" \
    "$show_create_expected" \
    "SET time_zone = '+00:00'; SHOW CREATE TABLE automatic_temporals;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
id	int	NULL	YES		NULL
ts	timestamp	CURRENT_TIMESTAMP	YES	DEFAULT_GENERATED on update CURRENT_TIMESTAMP	0
dt	datetime	CURRENT_TIMESTAMP	YES	DEFAULT_GENERATED on update CURRENT_TIMESTAMP	0
ts_init	timestamp	CURRENT_TIMESTAMP	YES	DEFAULT_GENERATED	0
dt_init	datetime	CURRENT_TIMESTAMP	YES	DEFAULT_GENERATED	0
ts_up	timestamp	NULL	YES	on update CURRENT_TIMESTAMP	0
dt_up	datetime	NULL	YES	on update CURRENT_TIMESTAMP	0
EXPECTED
)
expect_output \
    "information schema renders current timestamp metadata" \
    "$information_schema_expected" \
    "SET time_zone = '+00:00'; "\
"SELECT COLUMN_NAME, DATA_TYPE, COLUMN_DEFAULT, IS_NULLABLE, EXTRA, DATETIME_PRECISION "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'automatic_temporals' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

expect_output \
    "insert and update automatic temporal behavior" \
    "after_insert	1	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20	NULL	NULL
after_change	1	0	2	2023-11-14 22:14:20	2023-11-14 22:14:20	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:14:20	2023-11-14 22:14:20
after_noop	0	0	2	2023-11-14 22:14:20	2023-11-14 22:14:20	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:14:20	2023-11-14 22:14:20
after_explicit_current	1	0	2	2023-11-14 22:16:20	2023-11-14 22:16:20	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:16:20	2023-11-14 22:16:20" \
    "SET time_zone = '+00:00'; "\
"SET timestamp = 1700000000; INSERT INTO automatic_temporals(id) VALUES(1); "\
"SELECT 'after_insert', id, ts, dt, ts_init, dt_init, ts_up, dt_up FROM automatic_temporals; "\
"SET timestamp = 1700000060; UPDATE automatic_temporals SET id = 2 WHERE id = 1; "\
"SELECT 'after_change', ROW_COUNT(), @@warning_count, id, ts, dt, ts_init, dt_init, ts_up, dt_up FROM automatic_temporals; "\
"SET timestamp = 1700000120; UPDATE automatic_temporals SET id = 2 WHERE id = 2; "\
"SELECT 'after_noop', ROW_COUNT(), @@warning_count, id, ts, dt, ts_init, dt_init, ts_up, dt_up FROM automatic_temporals; "\
"SET timestamp = 1700000180; UPDATE automatic_temporals SET ts = CURRENT_TIMESTAMP WHERE id = 2; "\
"SELECT 'after_explicit_current', ROW_COUNT(), @@warning_count, id, ts, dt, ts_init, dt_init, ts_up, dt_up FROM automatic_temporals;" \
    "$DATABASE"

alter_add_expected=$(cat <<\EXPECTED
t	CREATE TABLE `t` (
  `id` int DEFAULT NULL,
  `ts` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `dt` datetime DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1	2023-11-14 22:14:20	2023-11-14 22:14:20
2	2023-11-14 22:14:20	2023-11-14 22:14:20
1	0
2	2023-11-14 22:14:20	2023-11-14 22:14:20
11	2023-11-14 22:15:20	2023-11-14 22:15:20
EXPECTED
)
expect_output \
    "alter add current timestamp default backfills statement timestamp" \
    "$alter_add_expected" \
    "SET time_zone = '+00:00'; CREATE TABLE t (id INT); INSERT INTO t VALUES (1), (2); "\
"SET timestamp = 1700000060; ALTER TABLE t ADD COLUMN ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, ADD COLUMN dt DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP; "\
"SHOW CREATE TABLE t; SELECT * FROM t ORDER BY id; "\
"SET timestamp = 1700000120; UPDATE t SET id = id + 10 WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT * FROM t ORDER BY id;" \
    "$DATABASE"

expect_error \
    "current timestamp default rejected on int" \
    1067 \
    "42000" \
    "Invalid default value for 'i'" \
    "CREATE TABLE bad_int (i INT DEFAULT CURRENT_TIMESTAMP);" \
    "$DATABASE"

expect_error \
    "fractional mismatch remains rejected" \
    1067 \
    "42000" \
    "Invalid default value for 'ts'" \
    "CREATE TABLE bad_fsp_mismatch (ts TIMESTAMP(6) DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP(3));" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_current_timestamp_defaults_expectations: ok"
