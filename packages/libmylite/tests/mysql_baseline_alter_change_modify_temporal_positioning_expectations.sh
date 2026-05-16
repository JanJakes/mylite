#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_temporal_positioning_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_alter_change_modify_temporal_positioning_expectations: $1" >&2
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
    output=$(printf '%s\n' "$output" | tr '\t' '|')
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

run_mysql \
    "SET time_zone = '+00:00'; SET timestamp = 1700000000; "\
"CREATE TABLE positioned (id INT NOT NULL, n INT, dt DATETIME NULL, ts TIMESTAMP NULL, tail INT); "\
"INSERT INTO positioned VALUES "\
"(1, 10, '2020-01-01 01:02:03', '2020-01-01 01:02:03', 99), "\
"(2, 20, '2021-02-03 04:05:06', NULL, 88);" \
    "$DATABASE" >/dev/null

positioned_expected=$(cat <<'EXPECTED'
0|0
2|0
0|0
n|bigint|YES||NULL|
id|int|NO||NULL|
dt|datetime|YES||NULL|
tail|int|YES||NULL|
ts_auto|timestamp|YES||CURRENT_TIMESTAMP|DEFAULT_GENERATED on update CURRENT_TIMESTAMP
positioned|CREATE TABLE `positioned` (
  `n` bigint DEFAULT NULL,
  `id` int NOT NULL,
  `dt` datetime DEFAULT NULL,
  `tail` int DEFAULT NULL,
  `ts_auto` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
n|1|NULL|YES|bigint|
id|2|NULL|NO|int|
dt|3|NULL|YES|datetime|
tail|4|NULL|YES|int|
ts_auto|5|CURRENT_TIMESTAMP|YES|timestamp|DEFAULT_GENERATED on update CURRENT_TIMESTAMP
1:10:2020-01-01 01:02:03:2020-01-01 01:02:03:99,2:20:2021-02-03 04:05:06:NULL:88
2|0
n|bigint|YES||NULL|
id|int|NO||NULL|
dt_ts|timestamp|YES||NULL|
tail|int|YES||NULL|
ts_auto|timestamp|YES||CURRENT_TIMESTAMP|DEFAULT_GENERATED on update CURRENT_TIMESTAMP
n|1|NULL|YES|bigint|
id|2|NULL|NO|int|
dt_ts|3|NULL|YES|timestamp|
tail|4|NULL|YES|int|
ts_auto|5|CURRENT_TIMESTAMP|YES|timestamp|DEFAULT_GENERATED on update CURRENT_TIMESTAMP
EXPECTED
)
expect_output \
    "positioning and temporal replacement metadata" \
    "$positioned_expected" \
    "ALTER TABLE positioned MODIFY n INT FIRST; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE positioned MODIFY n BIGINT FIRST; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE positioned CHANGE ts ts_auto TIMESTAMP NULL DEFAULT CURRENT_TIMESTAMP "\
"ON UPDATE CURRENT_TIMESTAMP AFTER tail; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM positioned; "\
"SHOW CREATE TABLE positioned; "\
"SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, COLUMN_TYPE, EXTRA "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='${DATABASE}' "\
"AND TABLE_NAME='positioned' ORDER BY ORDINAL_POSITION; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', n, ':', COALESCE(dt, 'NULL'), ':', "\
"COALESCE(ts_auto, 'NULL'), ':', tail) ORDER BY id) FROM positioned; "\
"ALTER TABLE positioned CHANGE dt dt_ts TIMESTAMP NULL AFTER id; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM positioned; "\
"SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, COLUMN_TYPE, EXTRA "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='${DATABASE}' "\
"AND TABLE_NAME='positioned' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

run_mysql \
    "SET time_zone = '+00:00'; SET timestamp = 1700000000; "\
"CREATE TABLE modified_temporal (id INT, dt DATETIME NULL, ts TIMESTAMP NULL); "\
"INSERT INTO modified_temporal VALUES "\
"(1, '2020-01-01 01:02:03', '2020-01-01 01:02:03'), "\
"(2, '2021-02-03 04:05:06', NULL);" \
    "$DATABASE" >/dev/null

modified_temporal_expected=$(cat <<'EXPECTED'
0|0
2|0
dt|datetime|YES||NULL|on update CURRENT_TIMESTAMP
id|int|YES||NULL|
ts|datetime|YES||NULL|
EXPECTED
)
expect_output \
    "modify temporal first and timestamp to datetime" \
    "$modified_temporal_expected" \
    "ALTER TABLE modified_temporal MODIFY dt DATETIME NULL ON UPDATE CURRENT_TIMESTAMP FIRST; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE modified_temporal MODIFY ts DATETIME NULL AFTER id; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM modified_temporal;" \
    "$DATABASE"

run_mysql "CREATE TABLE after_errors (a INT, b TIMESTAMP NULL, c INT);" "$DATABASE" >/dev/null
expect_error \
    "change after unknown column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'after_errors'" \
    "ALTER TABLE after_errors CHANGE b bb TIMESTAMP NULL AFTER missing;" \
    "$DATABASE"
expect_error \
    "change after old self" \
    1054 \
    42S22 \
    "Unknown column 'b' in 'after_errors'" \
    "ALTER TABLE after_errors CHANGE b bb TIMESTAMP NULL AFTER b;" \
    "$DATABASE"
expect_error \
    "modify after self" \
    1054 \
    42S22 \
    "Unknown column 'b' in 'after_errors'" \
    "ALTER TABLE after_errors MODIFY b TIMESTAMP NULL AFTER b;" \
    "$DATABASE"

run_mysql "CREATE TABLE null_bad (id INT, ts TIMESTAMP NULL); INSERT INTO null_bad VALUES (1, NULL);" \
    "$DATABASE" >/dev/null
expect_error \
    "null to not null temporal replacement" \
    1138 \
    22004 \
    "Invalid use of NULL value" \
    "ALTER TABLE null_bad CHANGE ts ts TIMESTAMP NOT NULL FIRST;" \
    "$DATABASE"
