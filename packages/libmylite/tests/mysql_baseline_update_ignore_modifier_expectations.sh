#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_update_ignore_modifier_$$"

fail() {
    printf '%s\n' "mysql_baseline_update_ignore_modifier_expectations: $1" >&2
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

expect_error \
    "modifier order" \
    1064 \
    42000 \
    "near 'LOW_PRIORITY t SET v = 2'" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL);
     UPDATE IGNORE LOW_PRIORITY t SET v = 2;" \
    "$DATABASE"
run_mysql "DROP TABLE IF EXISTS t;" "$DATABASE" >/dev/null

run_mysql \
    "SET sql_mode = 'STRICT_TRANS_TABLES';
     CREATE TABLE t(
         id INT PRIMARY KEY,
         u INT UNIQUE,
         nn INT NOT NULL,
         v VARCHAR(3) NOT NULL,
         i TINYINT,
         d DATETIME NOT NULL
     );
     INSERT INTO t VALUES
         (1, 1, 10, 'abc', 1, '2020-01-01 00:00:00'),
         (2, 2, 20, 'def', 2, '2020-01-02 00:00:00'),
         (3, 3, 30, 'ghi', 3, '2020-01-03 00:00:00');" \
    "$DATABASE" >/dev/null

expect_output \
    "low priority no-op" \
    "1	0	99" \
    "UPDATE LOW_PRIORITY t SET nn = 99 WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count, nn FROM t WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "null into not null" \
    "2	2	1:0,2:0,3:30" \
    "UPDATE IGNORE t SET nn = NULL WHERE id IN (1,2);
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', nn) ORDER BY id)
     FROM t;" \
    "$DATABASE"

expect_output \
    "string truncation no-op affected rows" \
    "1	2	1:abc,2:abc,3:ghi" \
    "UPDATE IGNORE t SET v = 'abcdef' WHERE id IN (1,2);
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id)
     FROM t;" \
    "$DATABASE"

expect_output \
    "integer range clipping" \
    "2	2	1:127,2:127,3:3" \
    "UPDATE IGNORE t SET i = 999 WHERE id IN (1,2);
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', i) ORDER BY id)
     FROM t;" \
    "$DATABASE"

expect_output \
    "invalid temporal adjustment" \
    "2	2	1:0000-00-00 00:00:00,2:0000-00-00 00:00:00,3:2020-01-03 00:00:00" \
    "UPDATE IGNORE t SET d = 'bad' WHERE id IN (1,2);
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', d) ORDER BY id)
     FROM t;" \
    "$DATABASE"

run_mysql \
    "DROP TABLE t;
     CREATE TABLE t(id INT PRIMARY KEY, nn INT NOT NULL, v VARCHAR(3) NOT NULL);
     INSERT INTO t VALUES (1, 10, 'abc'), (2, 20, 'def'), (3, 30, 'ghi');" \
    "$DATABASE" >/dev/null

expect_output \
    "order limit warning subset" \
    "1	1	1:10,2:20,3:0" \
    "UPDATE IGNORE t SET nn = NULL ORDER BY id DESC LIMIT 1;
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', nn) ORDER BY id)
     FROM t;" \
    "$DATABASE"

expect_output \
    "limit zero" \
    "0	0	1:10,2:20,3:0" \
    "UPDATE IGNORE t SET nn = NULL LIMIT 0;
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', nn) ORDER BY id)
     FROM t;" \
    "$DATABASE"

expect_output \
    "multiple assignments" \
    "2	4	1:0:abc,2:0:abc,3:0:ghi" \
    "UPDATE IGNORE t SET nn = NULL, v = 'abcdef' WHERE id IN (1,2);
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', nn, ':', v) ORDER BY id)
     FROM t;" \
    "$DATABASE"

expect_output \
    "default no explicit default" \
    "2	2	1:0,2:0" \
    "CREATE TABLE defaults_t(id INT PRIMARY KEY, nn INT NOT NULL);
     INSERT INTO defaults_t VALUES (1, 5), (2, 6);
     UPDATE IGNORE defaults_t SET nn = DEFAULT;
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', nn) ORDER BY id)
     FROM defaults_t;" \
    "$DATABASE"

expect_output \
    "warning rows" \
    "Warning	1048	Column 'nn' cannot be null
Warning	1048	Column 'nn' cannot be null" \
    "CREATE TABLE warnings_t(id INT PRIMARY KEY, nn INT NOT NULL);
     INSERT INTO warnings_t VALUES (1, 5), (2, 6);
     UPDATE IGNORE warnings_t SET nn = NULL;
     SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "mysql duplicate-key skip reference" \
    "0	2	1:1,2:2,3:3" \
    "CREATE TABLE dup_t(id INT PRIMARY KEY, u INT UNIQUE);
     INSERT INTO dup_t VALUES (1, 1), (2, 2), (3, 3);
     UPDATE IGNORE dup_t SET u = 1 WHERE id IN (2,3) ORDER BY id;
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', u) ORDER BY id)
     FROM dup_t;" \
    "$DATABASE"
