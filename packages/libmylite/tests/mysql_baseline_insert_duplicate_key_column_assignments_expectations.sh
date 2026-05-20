#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_odku_key_assign_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_insert_duplicate_key_column_assignments: $1" >&2
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
    expected=$2
    sql=$3
    shift 3

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
        fail "$label: expected error [$expected], got success"
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "unique key-column assignment" \
    "2	0	0
2	20	200
3	10	100" \
    "CREATE TABLE key_assign(a INT UNIQUE, b INT UNIQUE, v INT); "\
"INSERT INTO key_assign VALUES (1,10,100),(2,20,200); "\
"INSERT INTO key_assign VALUES (1,30,300) ON DUPLICATE KEY UPDATE a=3; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT a,b,v FROM key_assign ORDER BY a;" \
    "$DATABASE"
run_mysql "DROP TABLE key_assign;" "$DATABASE" >/dev/null

expect_output \
    "primary key-column assignment with VALUES warning" \
    "2	1	0
2	20	200
3	10	300" \
    "CREATE TABLE pk(id INT PRIMARY KEY, u INT UNIQUE, v INT); "\
"INSERT INTO pk VALUES(1,10,100),(2,20,200); "\
"INSERT INTO pk VALUES(1,30,300) ON DUPLICATE KEY UPDATE id=3, v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,u,v FROM pk ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE pk;" "$DATABASE" >/dev/null

expect_output \
    "same key VALUES assignment is no-op" \
    "0	1	0
1	10	100" \
    "CREATE TABLE no_op(a INT UNIQUE, b INT, v INT); "\
"INSERT INTO no_op VALUES(1,10,100); "\
"INSERT INTO no_op VALUES(1,20,200) ON DUPLICATE KEY UPDATE a=VALUES(a); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT a,b,v FROM no_op;" \
    "$DATABASE"
run_mysql "DROP TABLE no_op;" "$DATABASE" >/dev/null

expect_output \
    "composite unique key-column assignment" \
    "2	0	0
2	1	200
3	1	100" \
    "CREATE TABLE composite_key(a INT NOT NULL, b INT NOT NULL, v INT, "\
"UNIQUE KEY u_ab(a,b)); "\
"INSERT INTO composite_key VALUES(1,1,100),(2,1,200); "\
"INSERT INTO composite_key VALUES(1,1,300) ON DUPLICATE KEY UPDATE a=3; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT a,b,v FROM composite_key ORDER BY a,b;" \
    "$DATABASE"
run_mysql "DROP TABLE composite_key;" "$DATABASE" >/dev/null

expect_output \
    "prefix unique key-column assignment" \
    "2	0	0
defghi	xyzz	100
qqqaaa	rstu	200" \
    "CREATE TABLE prefix_key(name VARCHAR(20), code VARCHAR(20), v INT, "\
"UNIQUE KEY u_name_code(name(3), code(2))); "\
"INSERT INTO prefix_key VALUES('abcdef','xyzz',100),('qqqaaa','rstu',200); "\
"INSERT INTO prefix_key VALUES('abcuvw','xy11',300) ON DUPLICATE KEY UPDATE "\
"name='defghi'; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT name,code,v FROM prefix_key ORDER BY name;" \
    "$DATABASE"
run_mysql "DROP TABLE prefix_key;" "$DATABASE" >/dev/null

run_mysql \
    "CREATE TABLE prefix_conflict(name VARCHAR(20), code VARCHAR(20), v INT, "\
"UNIQUE KEY u_name_code(name(3), code(2))); "\
"INSERT INTO prefix_conflict VALUES('abcdef','xyzz',100),('defghi','xy99',200);" \
    "$DATABASE" >/dev/null
expect_error \
    "prefix unique key-column assignment duplicate conflict" \
    "ERROR 1062 (23000) at line 1: Duplicate entry 'def-xy' for key 'prefix_conflict.u_name_code'" \
    "INSERT INTO prefix_conflict VALUES('abcuvw','xy11',300) ON DUPLICATE KEY UPDATE "\
"name='defuvw';" \
    "$DATABASE"
run_mysql "DROP TABLE prefix_conflict;" "$DATABASE" >/dev/null

run_mysql \
    "CREATE TABLE conflict_assign(a INT UNIQUE, b INT UNIQUE, v INT); "\
"INSERT INTO conflict_assign VALUES (1,10,100),(2,20,200);" \
    "$DATABASE" >/dev/null
expect_error \
    "key-column assignment duplicate conflict" \
    "ERROR 1062 (23000) at line 1: Duplicate entry '2' for key 'conflict_assign.a'" \
    "INSERT INTO conflict_assign VALUES (1,30,300) ON DUPLICATE KEY UPDATE a=2;" \
    "$DATABASE"
expect_error \
    "second key duplicate conflict" \
    "ERROR 1062 (23000) at line 1: Duplicate entry '20' for key 'conflict_assign.b'" \
    "INSERT INTO conflict_assign VALUES (1,30,300) ON DUPLICATE KEY UPDATE b=20;" \
    "$DATABASE"
run_mysql "DROP TABLE conflict_assign;" "$DATABASE" >/dev/null

run_mysql \
    "CREATE TABLE rollback_t(a INT UNIQUE, b INT UNIQUE, v INT); "\
"INSERT INTO rollback_t VALUES (1,10,100),(2,20,200);" \
    "$DATABASE" >/dev/null
expect_error \
    "multi-row key-column conflict rolls back" \
    "ERROR 1062 (23000) at line 1: Duplicate entry '2' for key 'rollback_t.a'" \
    "INSERT INTO rollback_t VALUES(3,30,300),(1,40,400) "\
"ON DUPLICATE KEY UPDATE a=2;" \
    "$DATABASE"
expect_output \
    "rollback state after key-column conflict" \
    "1	10	100
2	20	200" \
    "SELECT a,b,v FROM rollback_t ORDER BY a;" \
    "$DATABASE"
run_mysql "DROP TABLE rollback_t;" "$DATABASE" >/dev/null

expect_output \
    "non-auto key assignment with auto-increment present" \
    "2	1	1
1	30	300
2	20	200
4	40	400" \
    "CREATE TABLE auto_key_assign(id INT AUTO_INCREMENT PRIMARY KEY, u INT UNIQUE, v INT); "\
"INSERT INTO auto_key_assign(u,v) VALUES(10,100),(20,200); "\
"INSERT INTO auto_key_assign(u,v) VALUES(10,300) "\
"ON DUPLICATE KEY UPDATE u=30, v=VALUES(v); "\
"SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count; "\
"INSERT INTO auto_key_assign(u,v) VALUES(40,400); "\
"SELECT id,u,v FROM auto_key_assign ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE auto_key_assign;" "$DATABASE" >/dev/null

expect_output \
    "mysql accepts auto-increment key assignment" \
    "2	1	1
2	20	200
100	10	300
101	30	400" \
    "CREATE TABLE ai(id INT AUTO_INCREMENT PRIMARY KEY, u INT UNIQUE, v INT); "\
"INSERT INTO ai(u,v) VALUES(10,100),(20,200); "\
"INSERT INTO ai(u,v) VALUES(10,300) ON DUPLICATE KEY UPDATE id=100, v=VALUES(v); "\
"SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count; "\
"INSERT INTO ai(u,v) VALUES(30,400); "\
"SELECT id,u,v FROM ai ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE ai;" "$DATABASE" >/dev/null

expect_output \
    "mysql cascades parent key assignment" \
    "2	0	0
2	20
3	10
1	3" \
    "CREATE TABLE parent(id INT PRIMARY KEY, u INT UNIQUE); "\
"CREATE TABLE child(id INT PRIMARY KEY, parent_id INT, "\
"FOREIGN KEY(parent_id) REFERENCES parent(id) ON UPDATE CASCADE); "\
"INSERT INTO parent VALUES(1,10),(2,20); "\
"INSERT INTO child VALUES(1,1); "\
"INSERT INTO parent VALUES(1,30) ON DUPLICATE KEY UPDATE id=3; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,u FROM parent ORDER BY id; "\
"SELECT id,parent_id FROM child ORDER BY id;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_insert_duplicate_key_column_assignments: ok"
