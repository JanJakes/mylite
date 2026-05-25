#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_straight_join_noop_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_straight_join_noop_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw "$@"
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

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null
run_mysql \
    "CREATE TABLE lefts (id INT NOT NULL, k INT NULL, v INT NULL, name VARCHAR(20)); "\
"CREATE TABLE rights (id INT NOT NULL, k INT NULL, w INT NULL, name VARCHAR(20)); "\
"CREATE TABLE extras (id INT NOT NULL, right_w INT NULL, z INT NULL); "\
"INSERT INTO lefts VALUES "\
"(1,10,100,'alpha'),(2,20,200,'Beta'),(3,NULL,300,'none'); "\
"INSERT INTO rights VALUES "\
"(7,10,700,'ALPHA'),(8,10,800,'beta'),(9,NULL,900,'none'); "\
"INSERT INTO extras VALUES (30,700,3000),(31,800,3100),(32,900,3200);" \
    "$DATABASE" >/dev/null

expect_output_with_headers \
    "straight join equality and diagnostics" \
    "id	id
1	7
1	8
@@warning_count	ROW_COUNT()
0	-1" \
    "DO 0; SELECT l.id, r.id FROM lefts AS l STRAIGHT_JOIN rights AS r "\
"ON l.k = r.k ORDER BY r.id, l.id; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "straight join without ON is cartesian" \
    "id	id
1	7
1	8" \
    "SELECT l.id, r.id FROM lefts l STRAIGHT_JOIN rights r "\
"WHERE l.id = 1 ORDER BY r.id LIMIT 2;" \
    "$DATABASE"

expect_output_with_headers \
    "straight join star projection order" \
    "id	k	v	name	id	k	w	name
1	10	100	alpha	7	10	700	ALPHA
1	10	100	alpha	8	10	800	beta" \
    "SELECT * FROM lefts STRAIGHT_JOIN rights ON lefts.k = rights.k "\
"ORDER BY rights.id;" \
    "$DATABASE"

expect_output_with_headers \
    "straight join string collation" \
    "id	id
1	7
2	8
3	9" \
    "SELECT l.id, r.id FROM lefts l STRAIGHT_JOIN rights r "\
"ON l.name = r.name ORDER BY r.id;" \
    "$DATABASE"

expect_output_with_headers \
    "chained straight join" \
    "id	id	id
1	7	30
1	8	31" \
    "SELECT l.id, r.id, e.id FROM lefts l STRAIGHT_JOIN rights r ON l.k = r.k "\
"STRAIGHT_JOIN extras e ON r.w = e.right_w ORDER BY e.id;" \
    "$DATABASE"

expect_output_with_headers \
    "select modifier and join operator together" \
    "id	id
1	7
1	8" \
    "SELECT STRAIGHT_JOIN l.id, r.id FROM lefts l STRAIGHT_JOIN rights r "\
"ON l.k = r.k ORDER BY r.id;" \
    "$DATABASE"

expect_output \
    "joined update straight join" \
    "1	0
1	999
2	200
3	300" \
    "UPDATE lefts AS l STRAIGHT_JOIN rights AS r ON l.k = r.k "\
"SET l.v = 999 WHERE r.id = 7; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT id, v FROM lefts ORDER BY id;" \
    "$DATABASE"

expect_output \
    "joined delete straight join" \
    "1	0
2
3" \
    "DELETE l FROM lefts AS l STRAIGHT_JOIN rights AS r ON l.k = r.k "\
"WHERE r.id = 8; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT id FROM lefts ORDER BY id;" \
    "$DATABASE"
