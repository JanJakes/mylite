#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_odku_composite_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_insert_duplicate_composite_keys_expectations: $1" >&2
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
    "composite primary duplicate changed and no-op rows" \
    "2	0	0
1	1	20	100
0	0	0
1	1	20	100" \
    "CREATE TABLE cp(a INT NOT NULL, b INT NOT NULL, v INT, n INT, PRIMARY KEY(a,b)); "\
"INSERT INTO cp VALUES (1,1,10,100); "\
"INSERT INTO cp VALUES (1,1,20,200) ON DUPLICATE KEY UPDATE v=20; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT a,b,v,n FROM cp ORDER BY a,b; "\
"INSERT INTO cp VALUES (1,1,20,300) ON DUPLICATE KEY UPDATE v=20; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT a,b,v,n FROM cp ORDER BY a,b;" \
    "$DATABASE"
run_mysql "DROP TABLE cp;" "$DATABASE" >/dev/null

expect_output \
    "composite primary multi-row values duplicate handling" \
    "6	1	0
1	1	40
1	2	30" \
    "CREATE TABLE cpv(a INT NOT NULL, b INT NOT NULL, v INT, PRIMARY KEY(a,b)); "\
"INSERT INTO cpv VALUES (1,1,10),(1,1,20),(1,2,30),(1,1,40) "\
"ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT a,b,v FROM cpv ORDER BY a,b;" \
    "$DATABASE"
run_mysql "DROP TABLE cpv;" "$DATABASE" >/dev/null

expect_output \
    "composite unique duplicate and null-key inserts" \
    "2	1	0
1	1	20
NULL	1	11
1	1	0
1	1	20
NULL	1	11
NULL	1	99" \
    "CREATE TABLE cu(a INT, b INT, v INT, UNIQUE KEY u_ab(a,b)); "\
"INSERT INTO cu VALUES (1,1,10),(NULL,1,11); "\
"INSERT INTO cu VALUES (1,1,20) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT IFNULL(a,'NULL'),b,v FROM cu ORDER BY a IS NULL,a,b,v; "\
"INSERT INTO cu VALUES (NULL,1,99) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT IFNULL(a,'NULL'),b,v FROM cu ORDER BY a IS NULL,a,b,v;" \
    "$DATABASE"
run_mysql "DROP TABLE cu;" "$DATABASE" >/dev/null

expect_output \
    "composite unique multiple duplicate assignments" \
    "2	1	0
1	1	20	40" \
    "CREATE TABLE cma(a INT, b INT, v INT, n INT, UNIQUE KEY u_ab(a,b)); "\
"INSERT INTO cma VALUES (1,1,10,30); "\
"INSERT INTO cma VALUES (1,1,20,50) ON DUPLICATE KEY UPDATE v=VALUES(v), n=40; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT a,b,v,n FROM cma ORDER BY a,b;" \
    "$DATABASE"
run_mysql "DROP TABLE cma;" "$DATABASE" >/dev/null

expect_output \
    "composite prefix unique duplicate update" \
    "2	1	0
abcdef	xyzz	2" \
    "CREATE TABLE cpu(a VARCHAR(20), b VARCHAR(20), n INT, UNIQUE KEY u_ab(a(3),b(2))); "\
"INSERT INTO cpu VALUES ('abcdef','xyzz',1); "\
"INSERT INTO cpu VALUES ('abcuvw','xyqq',2) ON DUPLICATE KEY UPDATE n=VALUES(n); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT a,b,n FROM cpu;" \
    "$DATABASE"
run_mysql "DROP TABLE cpu;" "$DATABASE" >/dev/null

expect_upstream_accepts \
    "mysql accepts duplicate updates on tables with multiple enforced keys" \
    "CREATE TABLE multiple_keys(a INT UNIQUE, b INT UNIQUE, v INT); "\
"INSERT INTO multiple_keys VALUES (1,10,100),(2,20,200); "\
"INSERT INTO multiple_keys VALUES (1,30,300) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"INSERT INTO multiple_keys VALUES (3,20,400) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"DROP TABLE multiple_keys;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts key-column duplicate assignments when the result is valid" \
    "CREATE TABLE key_assign(a INT NOT NULL, b INT NOT NULL, v INT, PRIMARY KEY(a,b)); "\
"INSERT INTO key_assign VALUES (1,1,10); "\
"INSERT INTO key_assign VALUES (1,1,20) ON DUPLICATE KEY UPDATE a=2; "\
"DROP TABLE key_assign;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_insert_duplicate_composite_keys_expectations: ok"
