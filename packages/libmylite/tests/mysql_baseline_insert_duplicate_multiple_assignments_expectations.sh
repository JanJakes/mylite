#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_odku_multi_expectations_$$"
VALUES_WARNING="'VALUES function' is deprecated and will be removed in a future release. Please use an alias (INSERT INTO ... VALUES (...) AS alias) and replace VALUES(col) in the ON DUPLICATE KEY UPDATE clause with alias.col instead"

fail() {
    printf '%s\n' "mysql_baseline_insert_duplicate_multiple_assignments_expectations: $1" >&2
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "multiple values assignments changed and warn twice" \
    "2	2	0
1	20	30" \
    "CREATE TABLE t(id INT PRIMARY KEY, a INT NOT NULL, b INT NOT NULL); "\
"INSERT INTO t VALUES (1,10,20); "\
"INSERT INTO t VALUES (1,20,30) ON DUPLICATE KEY UPDATE a=VALUES(a), b=VALUES(b); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,a,b FROM t;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "multiple values assignments warning text" \
    "Warning	1287	${VALUES_WARNING}
Warning	1287	${VALUES_WARNING}" \
    "CREATE TABLE t(id INT PRIMARY KEY, a INT NOT NULL, b INT NOT NULL); "\
"INSERT INTO t VALUES (1,10,20); "\
"INSERT INTO t VALUES (1,20,30) ON DUPLICATE KEY UPDATE a=VALUES(a), b=VALUES(b); "\
"SHOW WARNINGS;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "multiple values assignments no-op still warn twice" \
    "0	2	0
1	20	30" \
    "CREATE TABLE t(id INT PRIMARY KEY, a INT NOT NULL, b INT NOT NULL); "\
"INSERT INTO t VALUES (1,20,30); "\
"INSERT INTO t VALUES (1,20,30) ON DUPLICATE KEY UPDATE a=VALUES(a), b=VALUES(b); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,a,b FROM t;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "mixed no-op and changed assignments count as changed duplicate" \
    "2	2	0
1	10	30" \
    "CREATE TABLE t(id INT PRIMARY KEY, a INT NOT NULL, b INT NOT NULL); "\
"INSERT INTO t VALUES (1,10,20); "\
"INSERT INTO t VALUES (1,10,30) ON DUPLICATE KEY UPDATE a=VALUES(a), b=VALUES(b); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,a,b FROM t;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "insert set multiple duplicate assignments" \
    "2	1	0
1	22	33" \
    "CREATE TABLE t(id INT PRIMARY KEY, a INT NOT NULL, b INT NOT NULL); "\
"INSERT INTO t SET id=1, a=10, b=20; "\
"INSERT INTO t SET id=1, a=22, b=33 ON DUPLICATE KEY UPDATE a=VALUES(a), b=33; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,a,b FROM t;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "no-key table inserts normally but values warnings remain" \
    "1	2	0
1	10	20" \
    "CREATE TABLE nk(id INT, a INT, b INT); "\
"INSERT INTO nk VALUES (1,10,20) ON DUPLICATE KEY UPDATE a=VALUES(a), b=VALUES(b); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT COUNT(*), SUM(a), SUM(b) FROM nk;" \
    "$DATABASE"
run_mysql "DROP TABLE nk;" "$DATABASE" >/dev/null

expect_output \
    "default and null multiple duplicate assignments" \
    "2	0	0
1	7	N" \
    "CREATE TABLE t(id INT PRIMARY KEY, a INT NOT NULL DEFAULT 7, b INT NULL); "\
"INSERT INTO t VALUES (1,10,20); "\
"INSERT INTO t VALUES (1,99,88) ON DUPLICATE KEY UPDATE a=DEFAULT, b=NULL; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,a,IFNULL(b,'N') FROM t;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

run_mysql \
    "CREATE TABLE rollback_t(id INT PRIMARY KEY, ti TINYINT NOT NULL, b INT NOT NULL); "\
"INSERT INTO rollback_t VALUES (1,1,10);" \
    "$DATABASE" >/dev/null
expect_error \
    "later duplicate assignment conversion failure" \
    1264 \
    22003 \
    "Out of range value for column 'ti' at row 1" \
    "INSERT INTO rollback_t VALUES (1,2,20) "\
"ON DUPLICATE KEY UPDATE b=20, ti=128;" \
    "$DATABASE"
expect_output \
    "failed duplicate branch rolls back all duplicate assignments" \
    "1	1	10" \
    "SELECT id,ti,b FROM rollback_t;" \
    "$DATABASE"
run_mysql "DROP TABLE rollback_t;" "$DATABASE" >/dev/null

expect_output \
    "mysql accepts duplicate targets with left-to-right effects" \
    "2	0	0
1	12	12" \
    "CREATE TABLE t(id INT PRIMARY KEY, a INT NOT NULL, b INT NOT NULL); "\
"INSERT INTO t VALUES (1,7,8); "\
"INSERT INTO t VALUES (1,9,10) ON DUPLICATE KEY UPDATE a=11, a=12, b=a; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,a,b FROM t;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

printf '%s\n' "mysql_baseline_insert_duplicate_multiple_assignments_expectations: ok"
