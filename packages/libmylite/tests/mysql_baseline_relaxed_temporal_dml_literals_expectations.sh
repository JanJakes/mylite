#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_relaxed_temporal_dml_literals_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_relaxed_temporal_dml_literals_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    fi
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "relaxed temporal storage literals normalize" \
    "dt_values|3|1
1|2024-01-02 03:04:05
2|2024-01-02 00:34:05
3|2024-01-02 03:04:05
ts_values|3|1
1|2024-01-02 03:04:05
2|2024-01-02 00:34:05
3|2024-01-02 03:04:05" \
    "SET time_zone = '+00:00'; "\
"SET sql_mode = ''; "\
"CREATE TABLE dt_values (id INT PRIMARY KEY, v DATETIME NULL); "\
"INSERT INTO dt_values VALUES "\
"(1, '2024-01-02T03:04:05'), "\
"(2, '2024-01-02 03:04:05+02:30'), "\
"(3, '2024-01-02T03:04:05Z'); "\
"SELECT 'dt_values', ROW_COUNT(), @@warning_count; "\
"SELECT id, v FROM dt_values ORDER BY id; "\
"CREATE TABLE ts_values (id INT PRIMARY KEY, v TIMESTAMP NULL); "\
"INSERT INTO ts_values VALUES "\
"(1, '2024-01-02T03:04:05'), "\
"(2, '2024-01-02 03:04:05+02:30'), "\
"(3, '2024-01-02T03:04:05Z'); "\
"SELECT 'ts_values', ROW_COUNT(), @@warning_count; "\
"SELECT id, v FROM ts_values ORDER BY id;" \
    "$DATABASE"

expect_output \
    "date relaxed DML emits notes and warnings" \
    "0
1|2024-01-02
1
Note|1265|Data truncated for column 'v' at row 1
2|2024-01-02
0
3|2024-01-01
1
Warning|1265|Data truncated for column 'v' at row 1" \
    "SET sql_mode = ''; "\
"CREATE TABLE d_note (id INT PRIMARY KEY, v DATE NULL); "\
"INSERT INTO d_note VALUES (1, '2024-01-02T00:00:00'); "\
"SHOW COUNT(*) WARNINGS; SELECT id, v FROM d_note WHERE id = 1; "\
"INSERT INTO d_note VALUES (2, '2024-01-02T03:04:05'); "\
"SHOW COUNT(*) WARNINGS; SHOW WARNINGS; SELECT id, v FROM d_note WHERE id = 2; "\
"INSERT INTO d_note VALUES (3, '2024-01-02T00:00:00+14:00'); "\
"SHOW COUNT(*) WARNINGS; SELECT id, v FROM d_note WHERE id = 3; "\
"CREATE TABLE d_warning (id INT PRIMARY KEY, v DATE NULL); "\
"INSERT INTO d_warning VALUES (1, '2024-01-02T03:04:05Z'); "\
"SHOW COUNT(*) WARNINGS; SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "datetime trailing Z DML emits truncation warning" \
    "1
Warning|1265|Data truncated for column 'v' at row 1" \
    "SET sql_mode = ''; "\
"CREATE TABLE dt_warning (id INT PRIMARY KEY, v DATETIME NULL); "\
"INSERT INTO dt_warning VALUES (1, '2024-01-02T03:04:05Z'); "\
"SHOW COUNT(*) WARNINGS; SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "relaxed temporal defaults normalize" \
    "defaults|0|2
2024-01-02|2024-01-02 00:34:05|2024-01-02 03:04:05" \
    "SET time_zone = '+00:00'; "\
"SET sql_mode = ''; "\
"CREATE TABLE defaults ("\
"d DATE DEFAULT '2024-01-02T03:04:05', "\
"dt DATETIME DEFAULT '2024-01-02 03:04:05+02:30', "\
"ts TIMESTAMP NULL DEFAULT '2024-01-02T03:04:05Z'); "\
"SELECT 'defaults', ROW_COUNT(), @@warning_count; "\
"INSERT INTO defaults () VALUES (); "\
"SELECT d, dt, ts FROM defaults;" \
    "$DATABASE"

expect_output \
    "datetime offset defaults and DML use session target offset" \
    "dt_tz|2024-01-02 05:04:05" \
    "SET time_zone = '+02:00'; "\
"SET sql_mode = ''; "\
"CREATE TABLE dt_tz (id INT PRIMARY KEY, v DATETIME NULL); "\
"INSERT INTO dt_tz VALUES (1, '2024-01-02T03:04:05+00:00'); "\
"SELECT 'dt_tz', v FROM dt_tz;" \
    "$DATABASE"

expect_output \
    "relaxed temporal update and replace normalize" \
    "dt_update|1|1|2024-01-02 03:04:05
dt_replace|2|0|2024-01-02 00:34:05" \
    "SET time_zone = '+00:00'; "\
"SET sql_mode = ''; "\
"CREATE TABLE dt_change (id INT PRIMARY KEY, v DATETIME NULL); "\
"INSERT INTO dt_change VALUES (1, '2024-01-01 00:00:00'); "\
"UPDATE dt_change SET v = '2024-01-02T03:04:05Z' WHERE id = 1; "\
"SELECT 'dt_update', ROW_COUNT(), @@warning_count, v FROM dt_change; "\
"REPLACE INTO dt_change VALUES (1, '2024-01-02T03:04:05+02:30'); "\
"SELECT 'dt_replace', ROW_COUNT(), @@warning_count, v FROM dt_change;" \
    "$DATABASE"

expect_error \
    "strict datetime trailing Z insert is rejected" \
    1292 \
    22007 \
    "Incorrect datetime value: '2024-01-02T03:04:05Z' for column 'v' at row 1" \
    "SET sql_mode = 'STRICT_TRANS_TABLES'; "\
"CREATE TABLE strict_dt (id INT PRIMARY KEY, v DATETIME NULL); "\
"INSERT INTO strict_dt VALUES (1, '2024-01-02T03:04:05Z');" \
    "$DATABASE"

expect_error \
    "strict date trailing Z insert is rejected" \
    1292 \
    22007 \
    "Incorrect date value: '2024-01-02T03:04:05Z' for column 'v' at row 1" \
    "SET sql_mode = 'STRICT_TRANS_TABLES'; "\
"CREATE TABLE strict_d (id INT PRIMARY KEY, v DATE NULL); "\
"INSERT INTO strict_d VALUES (1, '2024-01-02T03:04:05Z');" \
    "$DATABASE"

expect_error \
    "strict datetime trailing Z default is rejected" \
    1067 \
    42000 \
    "Invalid default value for 'v'" \
    "SET sql_mode = 'STRICT_TRANS_TABLES'; "\
"CREATE TABLE strict_def_dt (v DATETIME DEFAULT '2024-01-02T03:04:05Z');" \
    "$DATABASE"

expect_error \
    "strict date trailing Z default is rejected" \
    1067 \
    42000 \
    "Invalid default value for 'v'" \
    "SET sql_mode = 'STRICT_TRANS_TABLES'; "\
"CREATE TABLE strict_def_d (v DATE DEFAULT '2024-01-02T03:04:05Z');" \
    "$DATABASE"
