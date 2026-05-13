#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_transaction_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_transaction_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_force() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --force "$@"
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
run_mysql "CREATE DATABASE ${DATABASE}; CREATE TABLE ${DATABASE}.t (id INT PRIMARY KEY, v INT);" \
    >/dev/null

expect_output \
    "commit outside transaction succeeds" \
    "0	0" \
    "COMMIT; SELECT ROW_COUNT(), @@warning_count;"

expect_output \
    "rollback outside transaction succeeds" \
    "0	0" \
    "ROLLBACK; SELECT ROW_COUNT(), @@warning_count;"

expect_output \
    "start transaction keeps visible autocommit enabled" \
    "1	0	0" \
    "START TRANSACTION; SELECT @@autocommit, ROW_COUNT(), @@warning_count; ROLLBACK;" \
    "$DATABASE"

expect_output \
    "commit persists inserted rows" \
    "0	0	1	10" \
    "START TRANSACTION; INSERT INTO t VALUES (1, 10); COMMIT; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), SUM(v) FROM t;" \
    "$DATABASE"

expect_output \
    "rollback discards inserted rows" \
    "0	0	1	10" \
    "START TRANSACTION; INSERT INTO t VALUES (2, 20); ROLLBACK; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), SUM(v) FROM t;" \
    "$DATABASE"

expect_output \
    "begin work and rollback work are supported synonyms" \
    "1	10" \
    "BEGIN WORK; INSERT INTO t VALUES (3, 30); ROLLBACK WORK; "\
"SELECT COUNT(*), SUM(v) FROM t;" \
    "$DATABASE"

expect_output \
    "commit work is supported synonym" \
    "0	0	2	50" \
    "BEGIN; INSERT INTO t VALUES (4, 40); COMMIT WORK; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), SUM(v) FROM t;" \
    "$DATABASE"

expect_output \
    "nested start commits prior transaction and starts a new one" \
    "1,4,5	100" \
    "START TRANSACTION; INSERT INTO t VALUES (5, 50); "\
"START TRANSACTION; INSERT INTO t VALUES (6, 60); ROLLBACK; "\
"SELECT GROUP_CONCAT(id ORDER BY id), SUM(v) FROM t;" \
    "$DATABASE"

expect_output \
    "ddl implicitly commits active transaction" \
    "1,4,5,7,8	250	1" \
    "START TRANSACTION; INSERT INTO t VALUES (7, 70); "\
"CREATE TABLE ddl_marker (id INT); "\
"INSERT INTO t VALUES (8, 80); ROLLBACK; "\
"SELECT GROUP_CONCAT(id ORDER BY id), SUM(v), "\
"(SELECT COUNT(*) FROM information_schema.tables "\
"WHERE table_schema = '${DATABASE}' AND table_name = 'ddl_marker') FROM t;" \
    "$DATABASE"

set +e
duplicate_output=$(run_mysql_force \
    "START TRANSACTION; "\
"INSERT INTO t VALUES (9, 90); "\
"INSERT INTO t VALUES (9, 91); "\
"SELECT 'after_error', ROW_COUNT(), @@error_count, @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t; "\
"INSERT INTO t VALUES (10, 100); "\
"COMMIT; "\
"SELECT 'after_commit', GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id), SUM(v) FROM t;" \
    "$DATABASE" 2>&1)
set -e

case "$duplicate_output" in
    *"ERROR 1062 (23000)"*"Duplicate entry '9' for key 't.PRIMARY'"*\
*"after_error	-1	1	1	1:10,4:40,5:50,7:70,8:80,9:90"*\
*"after_commit	1:10,4:40,5:50,7:70,8:80,9:90,10:100	440"*) ;;
    *)
        fail "duplicate statement rollback: unexpected output [$duplicate_output]"
        ;;
esac

run_mysql "START TRANSACTION; INSERT INTO t VALUES (11, 110);" "$DATABASE" >/dev/null
expect_output \
    "rolled back disconnect row is absent" \
    "0" \
    "SELECT COUNT(*) FROM t WHERE id = 11;" \
    "$DATABASE"

cleanup

printf '%s\n' "mysql_baseline_transaction_lifecycle_expectations: ok"
