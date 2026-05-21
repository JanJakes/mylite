#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_nonstrict_odku_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_nonstrict_duplicate_key_coercion_expectations: $1" >&2
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
    "nonstrict duplicate default no explicit default" \
    "2	1	0
1	0" \
    "SET sql_mode=''; "\
"CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t VALUES (1, 10); "\
"INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = DEFAULT; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id, v FROM t;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "nonstrict duplicate default warning" \
    "Warning	1364	Field 'v' doesn't have a default value" \
    "SET sql_mode=''; "\
"CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t VALUES (1, 10); "\
"INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = DEFAULT; "\
"SHOW WARNINGS;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "nonstrict duplicate string truncation" \
    "2	1	0
1	ab" \
    "SET sql_mode=''; "\
"CREATE TABLE t(id INT PRIMARY KEY, s VARCHAR(2) NOT NULL); "\
"INSERT INTO t VALUES (1, 'aa'); "\
"INSERT INTO t VALUES (1, 'bb') ON DUPLICATE KEY UPDATE s = 'abcd'; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id, s FROM t;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "nonstrict duplicate string warning row" \
    "Warning	1265	Data truncated for column 's' at row 2" \
    "SET sql_mode=''; "\
"CREATE TABLE t(id INT PRIMARY KEY, s VARCHAR(2) NOT NULL); "\
"INSERT INTO t VALUES (1, 'aa'); "\
"INSERT INTO t VALUES (2, 'bb'), (1, 'cc') ON DUPLICATE KEY UPDATE s = 'abcd'; "\
"SHOW WARNINGS;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "nonstrict multi-row duplicate null adjustment" \
    "3	1	0
1	0
2	20" \
    "SET sql_mode=''; "\
"CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t VALUES (1, 10); "\
"INSERT INTO t VALUES (2, 20), (1, 30) ON DUPLICATE KEY UPDATE v = NULL; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id, v FROM t ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "nonstrict multi-row duplicate null warning" \
    "Warning	1048	Column 'v' cannot be null" \
    "SET sql_mode=''; "\
"CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t VALUES (1, 10); "\
"INSERT INTO t VALUES (2, 20), (1, 30) ON DUPLICATE KEY UPDATE v = NULL; "\
"SHOW WARNINGS;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_error \
    "nonstrict single-row duplicate null remains error" \
    1048 \
    23000 \
    "Column 'v' cannot be null" \
    "SET sql_mode=''; "\
"CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t VALUES (1, 10); "\
"INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = NULL;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_error \
    "strict duplicate default remains error" \
    1364 \
    HY000 \
    "Field 'v' doesn't have a default value" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t VALUES (1, 10); "\
"INSERT INTO t VALUES (1, 20) ON DUPLICATE KEY UPDATE v = DEFAULT;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_error \
    "strict duplicate string remains error" \
    1406 \
    22001 \
    "Data too long for column 's' at row 1" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"CREATE TABLE t(id INT PRIMARY KEY, s VARCHAR(2) NOT NULL); "\
"INSERT INTO t VALUES (1, 'aa'); "\
"INSERT INTO t VALUES (1, 'bb') ON DUPLICATE KEY UPDATE s = 'abcd';" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_nonstrict_duplicate_key_coercion_expectations: ok"
