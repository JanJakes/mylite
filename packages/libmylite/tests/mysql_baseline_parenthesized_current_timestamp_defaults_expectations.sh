#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_parenthesized_current_timestamp_defaults_$$"

fail() {
    printf '%s\n' "mysql_baseline_parenthesized_current_timestamp_defaults: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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

expect_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept behavior, got [$output]"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9, got $version" ;;
esac

run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; CREATE DATABASE ${DATABASE};" >/dev/null

show_columns_expected=$(
    printf 'id\tint\tYES\t\tNULL\t\n'
    cat <<\EXPECTED
dt	datetime	YES		now()	DEFAULT_GENERATED
dt_call	datetime	YES		now()	DEFAULT_GENERATED
dt_now	datetime	YES		now()	DEFAULT_GENERATED
dt_local	datetime	YES		now()	DEFAULT_GENERATED
dt_local_call	datetime	YES		now()	DEFAULT_GENERATED
dt_lts	datetime	YES		now()	DEFAULT_GENERATED
dt_lts_call	datetime	YES		now()	DEFAULT_GENERATED
dt_nested	datetime	YES		now()	DEFAULT_GENERATED
ts	timestamp	YES		now()	DEFAULT_GENERATED
EXPECTED
)
expect_output \
    "show columns normalizes admitted parenthesized current timestamp defaults" \
    "$show_columns_expected" \
    "SET time_zone = '+00:00'; USE ${DATABASE}; "\
"CREATE TABLE current_exprs ("\
"id INT, "\
"dt DATETIME DEFAULT (CURRENT_TIMESTAMP), "\
"dt_call DATETIME DEFAULT (CURRENT_TIMESTAMP()), "\
"dt_now DATETIME DEFAULT (NOW()), "\
"dt_local DATETIME DEFAULT (LOCALTIME), "\
"dt_local_call DATETIME DEFAULT (LOCALTIME()), "\
"dt_lts DATETIME DEFAULT (LOCALTIMESTAMP), "\
"dt_lts_call DATETIME DEFAULT (LOCALTIMESTAMP()), "\
"dt_nested DATETIME DEFAULT ((NOW())), "\
"ts TIMESTAMP DEFAULT (NOW())); "\
"SHOW COLUMNS FROM current_exprs;" \
    "$DATABASE"

information_schema_expected=$(
    printf 'id\tNULL\t\n'
    cat <<\EXPECTED
dt	now()	DEFAULT_GENERATED
dt_call	now()	DEFAULT_GENERATED
dt_now	now()	DEFAULT_GENERATED
dt_local	now()	DEFAULT_GENERATED
dt_local_call	now()	DEFAULT_GENERATED
dt_lts	now()	DEFAULT_GENERATED
dt_lts_call	now()	DEFAULT_GENERATED
dt_nested	now()	DEFAULT_GENERATED
ts	now()	DEFAULT_GENERATED
EXPECTED
)
expect_output \
    "information schema reports generated now defaults" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, COLUMN_DEFAULT, EXTRA FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'current_exprs' "\
"ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
current_exprs	CREATE TABLE `current_exprs` (
  `id` int DEFAULT NULL,
  `dt` datetime DEFAULT (now()),
  `dt_call` datetime DEFAULT (now()),
  `dt_now` datetime DEFAULT (now()),
  `dt_local` datetime DEFAULT (now()),
  `dt_local_call` datetime DEFAULT (now()),
  `dt_lts` datetime DEFAULT (now()),
  `dt_lts_call` datetime DEFAULT (now()),
  `dt_nested` datetime DEFAULT (now()),
  `ts` timestamp NULL DEFAULT (now())
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders generated now defaults" \
    "$show_create_expected" \
    "SHOW CREATE TABLE ${DATABASE}.current_exprs;" \
    "$DATABASE"

insert_expected=$(cat <<\EXPECTED
1	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20	2023-11-14 22:13:20	1	0
EXPECTED
)
expect_output \
    "insert materializes parenthesized current timestamp defaults" \
    "$insert_expected" \
    "SET time_zone = '+00:00'; SET timestamp = 1700000000; "\
"INSERT INTO ${DATABASE}.current_exprs(id) VALUES (1); "\
"SELECT id, dt, dt_call, dt_now, dt_local, dt_local_call, dt_lts, dt_lts_call, "\
"dt_nested, ts, ROW_COUNT(), @@warning_count FROM ${DATABASE}.current_exprs;" \
    "$DATABASE"

alter_expected=$(
    cat <<\EXPECTED
t	CREATE TABLE `t` (
  `id` int DEFAULT NULL,
  `dt2` datetime DEFAULT (now())
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
    printf 'id\tint\tYES\t\tNULL\t\n'
    cat <<\EXPECTED
dt2	datetime	YES		now()	DEFAULT_GENERATED
1	2023-11-14 22:14:20
2	2023-11-14 22:14:20
3	2023-11-14 22:16:20
EXPECTED
)
expect_output \
    "alter add set modify and change preserve generated now defaults" \
    "$alter_expected" \
    "USE ${DATABASE}; SET time_zone = '+00:00'; CREATE TABLE t(id INT); "\
"INSERT INTO t(id) VALUES (1), (2); "\
"SET timestamp = 1700000060; ALTER TABLE t ADD COLUMN dt DATETIME DEFAULT (NOW()); "\
"SET timestamp = 1700000120; ALTER TABLE t ALTER COLUMN dt SET DEFAULT (CURRENT_TIMESTAMP); "\
"SET timestamp = 1700000180; INSERT INTO t(id) VALUES (3); "\
"ALTER TABLE t MODIFY COLUMN dt DATETIME DEFAULT (LOCALTIMESTAMP); "\
"ALTER TABLE t CHANGE COLUMN dt dt2 DATETIME DEFAULT (LOCALTIME()); "\
"SHOW CREATE TABLE t; SHOW COLUMNS FROM t; SELECT id, dt2 FROM t ORDER BY id;" \
    "$DATABASE"

like_expected=$(cat <<\EXPECTED
clone	CREATE TABLE `clone` (
  `id` int DEFAULT NULL,
  `dt` datetime DEFAULT (now())
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1	2023-11-14 22:17:20
EXPECTED
)
expect_output \
    "create table like preserves generated now default" \
    "$like_expected" \
    "USE ${DATABASE}; SET time_zone = '+00:00'; "\
"CREATE TABLE source_like(id INT, dt DATETIME DEFAULT (NOW())); "\
"CREATE TABLE clone LIKE source_like; SET timestamp = 1700000240; "\
"SHOW CREATE TABLE clone; INSERT INTO clone(id) VALUES (1); SELECT * FROM clone;" \
    "$DATABASE"

expect_error \
    "parenthesized on update remains syntax error" \
    1064 \
    "42000" \
    "near '(CURRENT_TIMESTAMP))'" \
    "USE ${DATABASE}; CREATE TABLE bad_on_update(dt DATETIME ON UPDATE (CURRENT_TIMESTAMP));" \
    "$DATABASE"

expect_accepts \
    "broader int expression default is MySQL behavior but deferred by MyLite" \
    "USE ${DATABASE}; CREATE TABLE broader_int(i INT DEFAULT (CURRENT_TIMESTAMP)); DROP TABLE broader_int;" \
    "$DATABASE"

expect_accepts \
    "fractional expression default is MySQL behavior but deferred by MyLite" \
    "USE ${DATABASE}; CREATE TABLE broader_fsp(dt DATETIME DEFAULT (CURRENT_TIMESTAMP(1))); DROP TABLE broader_fsp;" \
    "$DATABASE"

expect_accepts \
    "utc timestamp expression default is MySQL behavior but deferred by MyLite" \
    "USE ${DATABASE}; CREATE TABLE broader_utc(dt DATETIME DEFAULT (UTC_TIMESTAMP())); DROP TABLE broader_utc;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_parenthesized_current_timestamp_defaults: ok"
