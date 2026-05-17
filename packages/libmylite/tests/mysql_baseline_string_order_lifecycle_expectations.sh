#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_string_order_lifecycle_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_string_order_lifecycle_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;" \
    >/dev/null

run_mysql \
    "CREATE TABLE strings (id INT, k VARCHAR(10), c CHAR(5), t TEXT, v VARCHAR(10)); "\
"INSERT INTO strings VALUES "\
"(1, NULL, NULL, NULL, 'one'), "\
"(2, 'b', 'b', 'b', 'two'), "\
"(3, 'A', 'A', 'A', 'three'), "\
"(4, 'c', 'c', 'c', 'four'), "\
"(5, 'aa', 'aa', 'aa', 'five'), "\
"(6, 'd', 'd', 'd', 'six'), "\
"(7, 'abc  ', 'abc  ', 'abc  ', 'seven');" \
    "$DATABASE" >/dev/null

varchar_ascending_expected=$(cat <<\EXPECTED
1	NULL
3	A
5	aa
7	abc  
2	b
EXPECTED
)
expect_output \
    "varchar default ascending order with nulls and trailing spaces" \
    "$varchar_ascending_expected" \
    "SELECT id, k FROM strings ORDER BY k LIMIT 5;" \
    "$DATABASE"

varchar_descending_expected=$(cat <<\EXPECTED
6	d
4	c
2	b
7	abc  
EXPECTED
)
expect_output \
    "varchar descending order puts nulls last" \
    "$varchar_descending_expected" \
    "SELECT id, k FROM strings ORDER BY k DESC LIMIT 4;" \
    "$DATABASE"

text_ascending_expected=$(cat <<\EXPECTED
1	NULL
3	A
5	aa
7	abc  
EXPECTED
)
expect_output \
    "text explicit ascending order" \
    "$text_ascending_expected" \
    "SELECT id, t FROM strings ORDER BY t ASC LIMIT 4;" \
    "$DATABASE"

char_ascending_expected=$(cat <<\EXPECTED
1	NULL
3	A
5	aa
7	abc
EXPECTED
)
expect_output \
    "char ordering observes canonical char readback" \
    "$char_ascending_expected" \
    "SELECT id, c FROM strings ORDER BY c LIMIT 4;" \
    "$DATABASE"

update_varchar_order_expected=$(cat <<\EXPECTED
2	0	1:hit,2:two,3:hit,4:four,5:five
EXPECTED
)
expect_output \
    "ordered limited update by varchar key" \
    "$update_varchar_order_expected" \
    "DROP TABLE IF EXISTS upd; "\
"CREATE TABLE upd (id INT, k VARCHAR(10), v VARCHAR(10)); "\
"INSERT INTO upd VALUES "\
"(1, NULL, 'one'), (2, 'b', 'two'), (3, 'a', 'three'), "\
"(4, 'c', 'four'), (5, 'aa', 'five'); "\
"UPDATE upd SET v = 'hit' WHERE v <> 'skip' ORDER BY k LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM upd;" \
    "$DATABASE"

update_text_order_expected=$(cat <<\EXPECTED
5	0	1:all,2:all,3:all,4:all,5:all
EXPECTED
)
expect_output \
    "ordered update by text key without limit is accepted" \
    "$update_text_order_expected" \
    "UPDATE upd SET v = 'all' ORDER BY v; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM upd;" \
    "$DATABASE"

update_varchar_descending_expected=$(cat <<\EXPECTED
2	0	1:one,2:desc,3:three,4:desc,5:five
EXPECTED
)
expect_output \
    "ordered limited update by varchar key descending" \
    "$update_varchar_descending_expected" \
    "DROP TABLE IF EXISTS upd_desc; "\
"CREATE TABLE upd_desc (id INT, k VARCHAR(10), v VARCHAR(10)); "\
"INSERT INTO upd_desc VALUES "\
"(1, NULL, 'one'), (2, 'b', 'two'), (3, 'a', 'three'), "\
"(4, 'c', 'four'), (5, 'aa', 'five'); "\
"UPDATE upd_desc SET v = 'desc' ORDER BY k DESC LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM upd_desc;" \
    "$DATABASE"

wp_ordered_update_expected=$(cat <<\EXPECTED
1	0	a:x,b:old,c:old
EXPECTED
)
expect_output \
    "wp shaped ordered limited update without explicit id" \
    "$wp_ordered_update_expected" \
    "DROP TABLE IF EXISTS wp_update; "\
"CREATE TABLE wp_update (k VARCHAR(191), v LONGTEXT); "\
"INSERT INTO wp_update VALUES ('b', 'old'), ('a', 'old'), ('c', 'old'); "\
"UPDATE wp_update SET v = 'x' WHERE k = 'a' ORDER BY k LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(k, ':', v) ORDER BY k) FROM wp_update;" \
    "$DATABASE"

delete_order_expected=$(cat <<\EXPECTED
2	0	2:b,4:c,5:aa
EXPECTED
)
expect_output \
    "ordered limited delete by varchar key" \
    "$delete_order_expected" \
    "DROP TABLE IF EXISTS del; "\
"CREATE TABLE del (id INT, k VARCHAR(10)); "\
"INSERT INTO del VALUES (1, NULL), (2, 'b'), (3, 'a'), (4, 'c'), (5, 'aa'); "\
"DELETE FROM del WHERE id <> 999 ORDER BY k LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(k, 'NULL')) ORDER BY id) FROM del;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_string_order_lifecycle_expectations: ok"
