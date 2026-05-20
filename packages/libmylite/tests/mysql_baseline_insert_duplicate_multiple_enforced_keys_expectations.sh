#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_odku_multiple_keys_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_insert_duplicate_multiple_enforced_keys_expectations: $1" >&2
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

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
    fi
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
    "primary and secondary unique conflicts" \
    "2	1	0
1	10	300
2	20	200
2	1	0
1	10	300
2	20	400
2	1	0
1	10	500
2	20	400" \
    "CREATE TABLE pk_u(id INT PRIMARY KEY, u INT UNIQUE, v INT); "\
"INSERT INTO pk_u VALUES (1,10,100),(2,20,200); "\
"INSERT INTO pk_u VALUES (1,30,300) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,u,v FROM pk_u ORDER BY id; "\
"INSERT INTO pk_u VALUES (3,20,400) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,u,v FROM pk_u ORDER BY id; "\
"INSERT INTO pk_u VALUES (1,20,500) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,u,v FROM pk_u ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE pk_u;" "$DATABASE" >/dev/null

expect_output \
    "two secondary unique conflicts" \
    "2	1	0
1	10	300
2	20	200
2	1	0
1	10	300
2	20	400
2	1	0
1	10	500
2	20	400" \
    "CREATE TABLE two_u(a INT UNIQUE, b INT UNIQUE, v INT); "\
"INSERT INTO two_u VALUES (1,10,100),(2,20,200); "\
"INSERT INTO two_u VALUES (1,30,300) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT a,b,v FROM two_u ORDER BY a; "\
"INSERT INTO two_u VALUES (3,20,400) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT a,b,v FROM two_u ORDER BY a; "\
"INSERT INTO two_u VALUES (1,20,500) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT a,b,v FROM two_u ORDER BY a;" \
    "$DATABASE"
run_mysql "DROP TABLE two_u;" "$DATABASE" >/dev/null

expect_output \
    "unique order after alter add unique" \
    "2	1	0
1	10	500
2	20	200" \
    "CREATE TABLE alter_order(a INT UNIQUE, b INT, v INT); "\
"ALTER TABLE alter_order ADD UNIQUE KEY u_b(b); "\
"INSERT INTO alter_order VALUES (1,10,100),(2,20,200); "\
"INSERT INTO alter_order VALUES (1,20,500) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT a,b,v FROM alter_order ORDER BY a;" \
    "$DATABASE"
run_mysql "DROP TABLE alter_order;" "$DATABASE" >/dev/null

expect_output \
    "temporary table shadows persistent target" \
    "2	1	0
1	10	500
2	20	200
9	90	900" \
    "CREATE TABLE temp_shadow(a INT UNIQUE, b INT UNIQUE, v INT); "\
"INSERT INTO temp_shadow VALUES (9,90,900); "\
"CREATE TEMPORARY TABLE temp_shadow(a INT UNIQUE, b INT UNIQUE, v INT); "\
"INSERT INTO temp_shadow VALUES (1,10,100),(2,20,200); "\
"INSERT INTO temp_shadow VALUES (1,20,500) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT a,b,v FROM temp_shadow ORDER BY a; "\
"DROP TEMPORARY TABLE temp_shadow; "\
"SELECT a,b,v FROM temp_shadow ORDER BY a;" \
    "$DATABASE"
run_mysql "DROP TABLE temp_shadow;" "$DATABASE" >/dev/null

expect_output \
    "composite primary and secondary unique conflicts" \
    "2	1	0
1	1	10	300
2	2	20	200
2	1	0
1	1	10	300
2	2	20	400" \
    "CREATE TABLE comp_u(a INT NOT NULL, b INT NOT NULL, u INT UNIQUE, v INT, PRIMARY KEY(a,b)); "\
"INSERT INTO comp_u VALUES (1,1,10,100),(2,2,20,200); "\
"INSERT INTO comp_u VALUES (1,1,30,300) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT a,b,u,v FROM comp_u ORDER BY a,b; "\
"INSERT INTO comp_u VALUES (3,3,20,400) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT a,b,u,v FROM comp_u ORDER BY a,b;" \
    "$DATABASE"
run_mysql "DROP TABLE comp_u;" "$DATABASE" >/dev/null

expect_output \
    "nullable unique parts and no-op duplicate branch" \
    "1	1	0
NULL	10	100
NULL	20	200
0	0	0
1	10	100" \
    "CREATE TABLE null_u(a INT UNIQUE, b INT UNIQUE, v INT); "\
"INSERT INTO null_u VALUES (NULL,10,100); "\
"INSERT INTO null_u VALUES (NULL,20,200) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT IFNULL(a,'NULL'), b, v FROM null_u ORDER BY b; "\
"CREATE TABLE no_op(a INT UNIQUE, b INT UNIQUE, v INT); "\
"INSERT INTO no_op VALUES (1,10,100); "\
"INSERT INTO no_op VALUES (1,20,100) ON DUPLICATE KEY UPDATE v=100; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT a,b,v FROM no_op ORDER BY a;" \
    "$DATABASE"
run_mysql "DROP TABLE null_u;" "$DATABASE" >/dev/null
run_mysql "DROP TABLE no_op;" "$DATABASE" >/dev/null

expect_output \
    "auto increment duplicate attempt advances next generated value" \
    "2	1	1
1	10	300
2	20	200
1	4	0
1	10	300
2	20	200
4	30	400" \
    "CREATE TABLE ai(id INT AUTO_INCREMENT PRIMARY KEY, u INT UNIQUE, v INT); "\
"INSERT INTO ai(u,v) VALUES(10,100),(20,200); "\
"INSERT INTO ai(u,v) VALUES(10,300) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count; "\
"SELECT id,u,v FROM ai ORDER BY id; "\
"INSERT INTO ai(u,v) VALUES(30,400); "\
"SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count; "\
"SELECT id,u,v FROM ai ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE ai;" "$DATABASE" >/dev/null

expect_upstream_accepts \
    "mysql accepts key-column duplicate assignments when valid" \
    "CREATE TABLE key_assign(a INT UNIQUE, b INT UNIQUE, v INT); "\
"INSERT INTO key_assign VALUES (1,10,100),(2,20,200); "\
"INSERT INTO key_assign VALUES (1,30,300) ON DUPLICATE KEY UPDATE a=3; "\
"DROP TABLE key_assign;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_insert_duplicate_multiple_enforced_keys_expectations: ok"
