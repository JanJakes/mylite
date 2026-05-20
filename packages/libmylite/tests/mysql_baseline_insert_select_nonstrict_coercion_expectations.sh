#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_insert_select_nonstrict_coercion_$$"

fail() {
    printf '%s\n' "mysql_baseline_insert_select_nonstrict_coercion_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | mysql --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_error \
    "strict insert select omitted target column" \
    1364 \
    HY000 \
    "Field 'i' doesn't have a default value" \
    "CREATE TABLE src(id INT NOT NULL, n INT NULL, b BIGINT); "\
"INSERT INTO src VALUES (1,10,1),(2,NULL,2147483648),(3,30,-2147483649); "\
"CREATE TABLE dst(i INT NOT NULL, v VARCHAR(5) NOT NULL, dt DATETIME NOT NULL, n INT NULL); "\
"INSERT INTO dst(n) SELECT n FROM src WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "strict insert select selected null target" \
    1048 \
    23000 \
    "Column 'i' cannot be null" \
    "DROP TABLE IF EXISTS dst; "\
"CREATE TABLE dst(i INT NOT NULL, v VARCHAR(5) NOT NULL, dt DATETIME NOT NULL, n INT NULL); "\
"INSERT INTO dst(i,n) SELECT n,id FROM src WHERE id = 2;" \
    "$DATABASE"

expect_error \
    "strict insert select selected integer out of range" \
    1264 \
    22003 \
    "Out of range value for column 'i' at row 1" \
    "DROP TABLE IF EXISTS dst; "\
"CREATE TABLE dst(i INT NOT NULL, v VARCHAR(5) NOT NULL, dt DATETIME NOT NULL, n INT NULL); "\
"INSERT INTO dst(i,n) SELECT b,id FROM src WHERE id = 2;" \
    "$DATABASE"

expect_output \
    "nonstrict insert select adjusted rows" \
    "omit	2	3	0
omit_row	0	[]	0000-00-00 00:00:00	10
omit_row	0	[]	0000-00-00 00:00:00	NULL
selected_null	2	3	0
selected_null_row	10	[]	0000-00-00 00:00:00	1
selected_null_row	0	[]	0000-00-00 00:00:00	2
range	3	4	0
range_row	1	1
range_row	2147483647	2
range_row	-2147483648	3
zero_source	0	0	0
scalar_omit	1	3	0
scalar_omit_row	0	[]	0000-00-00 00:00:00	4
scalar_null	1	3	0
scalar_null_row	0	[]	0000-00-00 00:00:00	5" \
    "SET sql_mode=''; "\
"DROP TABLE IF EXISTS dst; "\
"CREATE TABLE dst(i INT NOT NULL, v VARCHAR(5) NOT NULL, dt DATETIME NOT NULL, n INT NULL); "\
"INSERT INTO dst(n) SELECT n FROM src WHERE id IN (1,2) ORDER BY id; "\
"SELECT 'omit', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'omit_row', i, CONCAT('[',v,']'), dt, IF(n IS NULL,'NULL',n) FROM dst ORDER BY n IS NULL, n; "\
"TRUNCATE dst; "\
"INSERT INTO dst(i,n) SELECT n,id FROM src WHERE id IN (1,2) ORDER BY id; "\
"SELECT 'selected_null', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'selected_null_row', i, CONCAT('[',v,']'), dt, n FROM dst ORDER BY n; "\
"TRUNCATE dst; "\
"INSERT INTO dst(i,n) SELECT b,id FROM src WHERE id IN (1,2,3) ORDER BY id; "\
"SELECT 'range', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'range_row', i, n FROM dst ORDER BY n; "\
"TRUNCATE dst; "\
"INSERT INTO dst(i,n) SELECT n,id FROM src WHERE id > 99; "\
"SELECT 'zero_source', ROW_COUNT(), @@warning_count, @@error_count; "\
"INSERT INTO dst(n) SELECT 4; "\
"SELECT 'scalar_omit', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'scalar_omit_row', i, CONCAT('[',v,']'), dt, n FROM dst; "\
"TRUNCATE dst; "\
"INSERT INTO dst(i,n) SELECT NULL,5; "\
"SELECT 'scalar_null', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'scalar_null_row', i, CONCAT('[',v,']'), dt, n FROM dst;" \
    "$DATABASE"

expect_output \
    "nonstrict insert select omitted warning rows" \
    "Warning	1364	Field 'i' doesn't have a default value
Warning	1364	Field 'v' doesn't have a default value
Warning	1364	Field 'dt' doesn't have a default value" \
    "SET sql_mode=''; "\
"TRUNCATE dst; "\
"INSERT INTO dst(n) SELECT n FROM src WHERE id IN (1,2) ORDER BY id; "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "nonstrict insert select selected null warning rows" \
    "Warning	1364	Field 'v' doesn't have a default value
Warning	1364	Field 'dt' doesn't have a default value
Warning	1048	Column 'i' cannot be null" \
    "SET sql_mode=''; "\
"TRUNCATE dst; "\
"INSERT INTO dst(i,n) SELECT n,id FROM src WHERE id IN (1,2) ORDER BY id; "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "nonstrict insert select range warning rows" \
    "Warning	1364	Field 'v' doesn't have a default value
Warning	1364	Field 'dt' doesn't have a default value
Warning	1264	Out of range value for column 'i' at row 2
Warning	1264	Out of range value for column 'i' at row 3" \
    "SET sql_mode=''; "\
"TRUNCATE dst; "\
"INSERT INTO dst(i,n) SELECT b,id FROM src WHERE id IN (1,2,3) ORDER BY id; "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "strict insert ignore select adjusted rows" \
    "ignore_null	2	3	0
ignore_null_row	10	[]	0000-00-00 00:00:00	1
ignore_null_row	0	[]	0000-00-00 00:00:00	2
ignore_range	3	4	0
ignore_range_row	1	1
ignore_range_row	2147483647	2
ignore_range_row	-2147483648	3" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE dst; "\
"INSERT IGNORE INTO dst(i,n) SELECT n,id FROM src WHERE id IN (1,2) ORDER BY id; "\
"SELECT 'ignore_null', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'ignore_null_row', i, CONCAT('[',v,']'), dt, n FROM dst ORDER BY n; "\
"TRUNCATE dst; "\
"INSERT IGNORE INTO dst(i,n) SELECT b,id FROM src WHERE id IN (1,2,3) ORDER BY id; "\
"SELECT 'ignore_range', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'ignore_range_row', i, n FROM dst ORDER BY n;" \
    "$DATABASE"

expect_output \
    "strict insert ignore select warning rows" \
    "Warning	1364	Field 'v' doesn't have a default value
Warning	1364	Field 'dt' doesn't have a default value
Warning	1048	Column 'i' cannot be null" \
    "SET sql_mode='STRICT_TRANS_TABLES'; "\
"TRUNCATE dst; "\
"INSERT IGNORE INTO dst(i,n) SELECT n,id FROM src WHERE id IN (1,2) ORDER BY id; "\
"SHOW WARNINGS;" \
    "$DATABASE"

cleanup
