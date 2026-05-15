#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_replace_key_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_replace_key_lifecycle_expectations: $1" >&2
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
    "primary key changed replacement" \
    "2	0	1	1:20" \
    "CREATE TABLE pk_diff(id INT PRIMARY KEY, v INT); "\
"INSERT INTO pk_diff VALUES(1,10); "\
"REPLACE INTO pk_diff VALUES(1,20); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(CONCAT(id, ':', v)) FROM pk_diff;" \
    "$DATABASE"

expect_output \
    "primary key exact replacement" \
    "1	0	1	1:10" \
    "CREATE TABLE pk_same(id INT PRIMARY KEY, v INT); "\
"INSERT INTO pk_same VALUES(1,10); "\
"REPLACE INTO pk_same VALUES(1,10); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(CONCAT(id, ':', v)) FROM pk_same;" \
    "$DATABASE"

expect_output \
    "replace set key replacement" \
    "1	0	1	1:10
2	0	1	1:20" \
    "CREATE TABLE set_pk(id INT PRIMARY KEY, v INT); "\
"INSERT INTO set_pk VALUES(1,10); "\
"REPLACE INTO set_pk SET id=1, v=10; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(CONCAT(id, ':', v)) FROM set_pk; "\
"REPLACE INTO set_pk SET id=1, v=20; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(CONCAT(id, ':', v)) FROM set_pk;" \
    "$DATABASE"

expect_output \
    "unique secondary replacement" \
    "2	0	1	2:10:200" \
    "CREATE TABLE uniq_one(id INT PRIMARY KEY, u INT UNIQUE, v INT); "\
"INSERT INTO uniq_one VALUES(1,10,100); "\
"REPLACE INTO uniq_one VALUES(2,10,200); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(CONCAT(id, ':', u, ':', v) ORDER BY id) FROM uniq_one;" \
    "$DATABASE"

expect_output \
    "multiple unique rows replaced" \
    "3	0	1	1:4:99" \
    "CREATE TABLE multi_unique(a INT UNIQUE, b INT UNIQUE, v INT); "\
"INSERT INTO multi_unique VALUES(1,2,12),(3,4,34); "\
"REPLACE INTO multi_unique VALUES(1,4,99); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(CONCAT(a, ':', b, ':', v) ORDER BY a,b) FROM multi_unique;" \
    "$DATABASE"

expect_output \
    "composite primary key replacement" \
    "2	0	1	1:2:20" \
    "CREATE TABLE comp_pk(a INT, b INT, v INT, PRIMARY KEY(a,b)); "\
"INSERT INTO comp_pk VALUES(1,2,10); "\
"REPLACE INTO comp_pk VALUES(1,2,20); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(CONCAT(a, ':', b, ':', v) ORDER BY a,b) FROM comp_pk;" \
    "$DATABASE"

expect_output \
    "string and prefix key replacement" \
    "2	0	1	abc:20
2	0	1	abczzz:20" \
    "CREATE TABLE str_pk(id VARCHAR(10) PRIMARY KEY, v INT); "\
"INSERT INTO str_pk VALUES('abc',10); "\
"REPLACE INTO str_pk VALUES('abc',20); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(CONCAT(id, ':', v)) FROM str_pk; "\
"CREATE TABLE prefix_u(name VARCHAR(10), v INT, UNIQUE KEY name_u(name(3))); "\
"INSERT INTO prefix_u VALUES('abcdef',10); "\
"REPLACE INTO prefix_u VALUES('abczzz',20); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(CONCAT(name, ':', v) ORDER BY name) FROM prefix_u;" \
    "$DATABASE"

expect_output \
    "nullable unique nulls do not conflict" \
    "1	0	2	1:N:10,2:N:20" \
    "CREATE TABLE nullable_unique(id INT PRIMARY KEY, u INT UNIQUE, v INT); "\
"INSERT INTO nullable_unique VALUES(1,NULL,10); "\
"REPLACE INTO nullable_unique VALUES(2,NULL,20); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(u, 'N'), ':', v) ORDER BY id) FROM nullable_unique;" \
    "$DATABASE"

expect_output \
    "auto increment generated and explicit replacements" \
    "1	1	1:10:100
2	2	2:10:200
2	2	7:10:700
1	8	7:10:700,8:20:300" \
    "CREATE TABLE ai_sec(id INT AUTO_INCREMENT PRIMARY KEY, u INT UNIQUE, v INT); "\
"REPLACE INTO ai_sec(u,v) VALUES(10,100); "\
"SELECT ROW_COUNT(), LAST_INSERT_ID(), GROUP_CONCAT(CONCAT(id, ':', u, ':', v) ORDER BY id) FROM ai_sec; "\
"REPLACE INTO ai_sec(u,v) VALUES(10,200); "\
"SELECT ROW_COUNT(), LAST_INSERT_ID(), GROUP_CONCAT(CONCAT(id, ':', u, ':', v) ORDER BY id) FROM ai_sec; "\
"REPLACE INTO ai_sec(id,u,v) VALUES(7,10,700); "\
"SELECT ROW_COUNT(), LAST_INSERT_ID(), GROUP_CONCAT(CONCAT(id, ':', u, ':', v) ORDER BY id) FROM ai_sec; "\
"REPLACE INTO ai_sec(u,v) VALUES(20,300); "\
"SELECT ROW_COUNT(), LAST_INSERT_ID(), GROUP_CONCAT(CONCAT(id, ':', u, ':', v) ORDER BY id) FROM ai_sec;" \
    "$DATABASE"

expect_output \
    "multi-row replacement within statement" \
    "4	0	2	1:20,2:30" \
    "CREATE TABLE multi_rows(id INT PRIMARY KEY, v INT); "\
"REPLACE INTO multi_rows VALUES(1,10),(1,20),(2,30); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM multi_rows;" \
    "$DATABASE"

expect_error \
    "parent-side foreign key replacement rejected" \
    1451 \
    23000 \
    "Cannot delete or update a parent row" \
    "CREATE TABLE parent(id INT PRIMARY KEY, v INT); "\
"CREATE TABLE child(id INT PRIMARY KEY, parent_id INT, FOREIGN KEY(parent_id) REFERENCES parent(id)); "\
"INSERT INTO parent VALUES(1,10); "\
"INSERT INTO child VALUES(1,1); "\
"REPLACE INTO parent VALUES(1,10);" \
    "$DATABASE"

expect_error \
    "child-side foreign key replacement rejected" \
    1452 \
    23000 \
    "Cannot add or update a child row" \
    "REPLACE INTO child VALUES(1,2);" \
    "$DATABASE"

expect_output \
    "child-side exact replacement succeeds" \
    "1	0	1:1" \
    "REPLACE INTO child VALUES(1,1); "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', parent_id) ORDER BY id) FROM child;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_replace_key_lifecycle_expectations: ok"
