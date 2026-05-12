#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_odku_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_insert_on_duplicate_key_update_expectations: $1" >&2
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
    "literal duplicate update affected rows" \
    "2	0	0
1	20	30
0	0	0
1	20	30" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL, n INT NULL); "\
"INSERT INTO t VALUES (1,10,NULL); "\
"INSERT INTO t VALUES (1,20,30) ON DUPLICATE KEY UPDATE v=20, n=30; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v,IFNULL(n,'N') FROM t ORDER BY id; "\
"INSERT INTO t VALUES (1,20,30) ON DUPLICATE KEY UPDATE v=20, n=30; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v,IFNULL(n,'N') FROM t ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "values function duplicate update" \
    "2	1	0
1	20" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t VALUES (1,10); "\
"INSERT INTO t VALUES (1,20) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v FROM t ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "values function warning text" \
    "Warning	1287	'VALUES function' is deprecated and will be removed in a future release. Please use an alias (INSERT INTO ... VALUES (...) AS alias) and replace VALUES(col) in the ON DUPLICATE KEY UPDATE clause with alias.col instead" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t VALUES (1,10); "\
"INSERT INTO t VALUES (1,20) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SHOW WARNINGS;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "multi-row row-by-row duplicate handling" \
    "4	1	0
1	20
2	30" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t VALUES (1,10),(1,20),(2,30) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v FROM t ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "no-key table inserts normally" \
    "1	0	0
1	10
1	20" \
    "CREATE TABLE nk(id INT, v INT); "\
"INSERT INTO nk VALUES (1,10); "\
"INSERT INTO nk VALUES (1,20) ON DUPLICATE KEY UPDATE v=20; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v FROM nk ORDER BY v;" \
    "$DATABASE"
run_mysql "DROP TABLE nk;" "$DATABASE" >/dev/null

expect_output \
    "unique-key duplicate update" \
    "2	1	0
1	10	200" \
    "CREATE TABLE u(id INT, email INT UNIQUE, v INT); "\
"INSERT INTO u VALUES (1,10,100); "\
"INSERT INTO u VALUES (2,10,200) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,email,v FROM u ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE u;" "$DATABASE" >/dev/null

expect_output \
    "insert set duplicate update" \
    "2	1	0
1	20" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t SET id=1, v=10; "\
"INSERT INTO t SET id=1, v=20 ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v FROM t ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "delayed duplicate update warning count" \
    "1	1	0
1	10" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT DELAYED INTO t VALUES (1,10) ON DUPLICATE KEY UPDATE v=20; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v FROM t ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "default duplicate assignment" \
    "2	0	0
1	7" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL DEFAULT 7); "\
"INSERT INTO t VALUES (1,10); "\
"INSERT INTO t VALUES (1,20) ON DUPLICATE KEY UPDATE v=DEFAULT; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v FROM t ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_error \
    "unknown duplicate assignment column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT); "\
"INSERT INTO t VALUES (1,10) ON DUPLICATE KEY UPDATE missing=1;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_error \
    "unknown values function column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT); "\
"INSERT INTO t VALUES (1,10) ON DUPLICATE KEY UPDATE v=VALUES(missing);" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_error \
    "null into not null duplicate assignment" \
    1048 \
    23000 \
    "Column 'v' cannot be null" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t VALUES (1,10); "\
"INSERT INTO t VALUES (1,20) ON DUPLICATE KEY UPDATE v=NULL;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_error \
    "default without explicit default duplicate assignment" \
    1364 \
    HY000 \
    "Field 'v' doesn't have a default value" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t VALUES (1,10); "\
"INSERT INTO t VALUES (1,20) ON DUPLICATE KEY UPDATE v=DEFAULT;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_insert_on_duplicate_key_update_expectations: ok"
