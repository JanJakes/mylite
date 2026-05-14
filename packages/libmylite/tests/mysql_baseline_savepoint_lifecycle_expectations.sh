#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_savepoint_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_savepoint_lifecycle_expectations: $1" >&2
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
    expected=$2
    sql=$3
    shift 3

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status=$?
    set -e

    if [ "$status" -eq 0 ]; then
        fail "$label: expected error containing [$expected], got success [$output]"
    fi

    case "$output" in
        *"$expected"*) ;;
        *) fail "$label: expected error containing [$expected], got [$output]" ;;
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
run_mysql "CREATE DATABASE ${DATABASE}; CREATE TABLE ${DATABASE}.t (id INT PRIMARY KEY, v INT);" \
    >/dev/null

expect_output \
    "savepoint outside transaction is no-op success" \
    "0	0" \
    "SAVEPOINT outside_sp; SELECT ROW_COUNT(), @@warning_count;"

expect_error \
    "rollback to autocommit savepoint fails later" \
    "ERROR 1305 (42000)" \
    "ROLLBACK TO SAVEPOINT outside_sp;"

expect_error \
    "release unknown savepoint fails" \
    "ERROR 1305 (42000)" \
    "RELEASE SAVEPOINT missing_sp;" \
    "$DATABASE"

expect_output \
    "rollback to savepoint keeps target and removes later savepoints" \
    "after_a	1	10	0	0
after_a_again	1	10	0	0" \
    "START TRANSACTION; "\
"INSERT INTO t VALUES (1, 10); "\
"SAVEPOINT a; "\
"INSERT INTO t VALUES (2, 20); "\
"SAVEPOINT b; "\
"INSERT INTO t VALUES (3, 30); "\
"ROLLBACK TO SAVEPOINT a; "\
"SELECT 'after_a', GROUP_CONCAT(id ORDER BY id), SUM(v), ROW_COUNT(), @@warning_count FROM t; "\
"ROLLBACK TO a; "\
"SELECT 'after_a_again', GROUP_CONCAT(id ORDER BY id), SUM(v), ROW_COUNT(), @@warning_count FROM t; "\
"COMMIT;" \
    "$DATABASE"

expect_error \
    "rollback to removed later savepoint fails" \
    "ERROR 1305 (42000)" \
    "START TRANSACTION; SAVEPOINT a; SAVEPOINT b; ROLLBACK TO a; ROLLBACK TO b;" \
    "$DATABASE"

expect_output \
    "release removes target and keeps row changes" \
    "1,4,5	100	0	0" \
    "START TRANSACTION; "\
"INSERT INTO t VALUES (4, 40); "\
"SAVEPOINT r; "\
"INSERT INTO t VALUES (5, 50); "\
"RELEASE SAVEPOINT r; "\
"COMMIT; "\
"SELECT GROUP_CONCAT(id ORDER BY id), SUM(v), ROW_COUNT(), @@warning_count FROM t;" \
    "$DATABASE"

expect_output \
    "duplicate savepoint replacement rolls back to latest same-name point" \
    "1,4,5,6,7	230" \
    "START TRANSACTION; "\
"INSERT INTO t VALUES (6, 60); "\
"SAVEPOINT dup; "\
"INSERT INTO t VALUES (7, 70); "\
"SAVEPOINT dup; "\
"INSERT INTO t VALUES (8, 80); "\
"ROLLBACK TO SAVEPOINT dup; "\
"COMMIT; "\
"SELECT GROUP_CONCAT(id ORDER BY id), SUM(v) FROM t;" \
    "$DATABASE"

expect_error \
    "release after duplicate replacement leaves no older same-name savepoint" \
    "ERROR 1305 (42000)" \
    "START TRANSACTION; SAVEPOINT dup; SAVEPOINT dup; RELEASE SAVEPOINT dup; ROLLBACK TO SAVEPOINT dup;" \
    "$DATABASE"

expect_output \
    "duplicate savepoint replacement preserves differently named later savepoints" \
    "after_b	1	0	0" \
    "START TRANSACTION; "\
"SAVEPOINT a; "\
"INSERT INTO t VALUES (14, 140); "\
"SAVEPOINT b; "\
"INSERT INTO t VALUES (15, 150); "\
"SAVEPOINT a; "\
"INSERT INTO t VALUES (16, 160); "\
"ROLLBACK TO b; "\
"SELECT 'after_b', (SELECT COUNT(*) FROM t WHERE id IN (14, 15, 16)), "\
"ROW_COUNT(), @@warning_count; "\
"ROLLBACK;" \
    "$DATABASE"

expect_output \
    "rollback work to savepoint forms are accepted" \
    "0	0	5" \
    "START TRANSACTION; "\
"SAVEPOINT work_form; "\
"INSERT INTO t VALUES (9, 90); "\
"ROLLBACK WORK TO work_form; "\
"SAVEPOINT savepoint_form; "\
"INSERT INTO t VALUES (10, 100); "\
"ROLLBACK WORK TO SAVEPOINT savepoint_form; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM t; "\
"COMMIT;" \
    "$DATABASE"

expect_output \
    "savepoint names are case-insensitive and may be quoted" \
    "0	0	5" \
    "START TRANSACTION; "\
"SAVEPOINT MixedName; "\
"INSERT INTO t VALUES (11, 110); "\
"ROLLBACK TO mixedname; "\
"SAVEPOINT \`sp ace\`; "\
"INSERT INTO t VALUES (12, 120); "\
"ROLLBACK TO SAVEPOINT \`SP ACE\`; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM t; "\
"COMMIT;" \
    "$DATABASE"

expect_error \
    "commit clears savepoints" \
    "ERROR 1305 (42000)" \
    "START TRANSACTION; SAVEPOINT c; COMMIT; ROLLBACK TO SAVEPOINT c;" \
    "$DATABASE"

expect_error \
    "full rollback clears savepoints" \
    "ERROR 1305 (42000)" \
    "START TRANSACTION; SAVEPOINT rb; ROLLBACK; RELEASE SAVEPOINT rb;" \
    "$DATABASE"

expect_error \
    "ddl implicit commit clears savepoints" \
    "ERROR 1305 (42000)" \
    "START TRANSACTION; INSERT INTO t VALUES (13, 130); SAVEPOINT ddl_sp; CREATE TABLE ddl_marker (id INT); ROLLBACK TO ddl_sp;" \
    "$DATABASE"

expect_output \
    "ddl implicit commit persisted row and ddl" \
    "1
1" \
    "SELECT COUNT(*) FROM t WHERE id = 13; "\
"SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = '${DATABASE}' AND table_name = 'ddl_marker';" \
    "$DATABASE"

expect_output \
    "successful savepoint statements report zero affected rows and warnings" \
    "0	0
0	0
0	0" \
    "START TRANSACTION; "\
"SAVEPOINT report_sp; SELECT ROW_COUNT(), @@warning_count; "\
"ROLLBACK TO SAVEPOINT report_sp; SELECT ROW_COUNT(), @@warning_count; "\
"RELEASE SAVEPOINT report_sp; SELECT ROW_COUNT(), @@warning_count; "\
"COMMIT;" \
    "$DATABASE"

cleanup

printf '%s\n' "mysql_baseline_savepoint_lifecycle_expectations: ok"
