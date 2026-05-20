#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_nonstrict_dml_coercion_$$"

fail() {
    printf '%s\n' "mysql_baseline_nonstrict_dml_coercion_expectations: $1" >&2
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

expect_combined_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@" 2>&1)
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
    "strict omitted insert keeps error" \
    1364 \
    HY000 \
    "Field 'i' doesn't have a default value" \
    "CREATE TABLE strict_t(i INT NOT NULL, n INT NULL); "\
"INSERT INTO strict_t(n) VALUES (1);" \
    "$DATABASE"

expect_error \
    "nonstrict explicit null insert keeps error" \
    1048 \
    23000 \
    "Column 'i' cannot be null" \
    "DROP TABLE IF EXISTS strict_t; "\
"SET sql_mode=''; "\
"CREATE TABLE strict_t(i INT NOT NULL, n INT NULL); "\
"INSERT INTO strict_t(i,n) VALUES (NULL,1);" \
    "$DATABASE"

expect_output \
    "nonstrict insert replace and update adjusted rows" \
    "insert_omitted	2	3	0
insert_rows	0	[]	0000-00-00 00:00:00	1
insert_rows	0	[]	0000-00-00 00:00:00	2
insert_default	1	3	0
insert_default_rows	0	[]	0000-00-00 00:00:00	3
insert_set	1	3	0
insert_set_rows	0	[]	0000-00-00 00:00:00	4
replace_values	2	3	0
replace_rows	0	[]	0000-00-00 00:00:00	5
replace_rows	0	[]	0000-00-00 00:00:00	6
replace_set	1	3	0
replace_set_rows	0	[]	0000-00-00 00:00:00	7
insert_empty_values	1	3	0
insert_empty_values_row	0	[]	0000-00-00 00:00:00	NULL
insert_empty_value	1	3	0
insert_empty_row	1	3	0
replace_empty_values	1	3	0
update_null	2	6	0
update_null_rows	0	[]	0000-00-00 00:00:00	8
update_null_rows	0	[]	0000-00-00 00:00:00	9
update_null_no_match	0	0	0
update_default	1	3	0
update_default_rows	0	[]	0000-00-00 00:00:00	8
update_default_limit0	0	0	0" \
    "SET sql_mode=''; "\
"DROP TABLE IF EXISTS coerce_t; "\
"CREATE TABLE coerce_t(i INT NOT NULL, v VARCHAR(5) NOT NULL, dt DATETIME NOT NULL, n INT NULL); "\
"INSERT INTO coerce_t(n) VALUES (1),(2); "\
"SELECT 'insert_omitted', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'insert_rows', i, CONCAT('[',v,']'), dt, n FROM coerce_t ORDER BY n; "\
"TRUNCATE coerce_t; "\
"INSERT INTO coerce_t VALUES (DEFAULT, DEFAULT, DEFAULT, 3); "\
"SELECT 'insert_default', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'insert_default_rows', i, CONCAT('[',v,']'), dt, n FROM coerce_t ORDER BY n; "\
"TRUNCATE coerce_t; "\
"INSERT INTO coerce_t SET n=4; "\
"SELECT 'insert_set', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'insert_set_rows', i, CONCAT('[',v,']'), dt, n FROM coerce_t ORDER BY n; "\
"TRUNCATE coerce_t; "\
"REPLACE INTO coerce_t(n) VALUES (5),(6); "\
"SELECT 'replace_values', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'replace_rows', i, CONCAT('[',v,']'), dt, n FROM coerce_t ORDER BY n; "\
"TRUNCATE coerce_t; "\
"REPLACE INTO coerce_t SET n=7; "\
"SELECT 'replace_set', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'replace_set_rows', i, CONCAT('[',v,']'), dt, n FROM coerce_t ORDER BY n; "\
"TRUNCATE coerce_t; "\
"INSERT INTO coerce_t VALUES (); "\
"SELECT 'insert_empty_values', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'insert_empty_values_row', i, CONCAT('[',v,']'), dt, IF(n IS NULL,'NULL',n) FROM coerce_t; "\
"TRUNCATE coerce_t; "\
"INSERT INTO coerce_t VALUE (); "\
"SELECT 'insert_empty_value', ROW_COUNT(), @@warning_count, @@error_count; "\
"TRUNCATE coerce_t; "\
"INSERT INTO coerce_t VALUES ROW(); "\
"SELECT 'insert_empty_row', ROW_COUNT(), @@warning_count, @@error_count; "\
"TRUNCATE coerce_t; "\
"REPLACE INTO coerce_t VALUES (); "\
"SELECT 'replace_empty_values', ROW_COUNT(), @@warning_count, @@error_count; "\
"TRUNCATE coerce_t; "\
"INSERT INTO coerce_t VALUES (11,'abc','2020-01-02 03:04:05',8),(12,'def','2020-01-03 03:04:05',9); "\
"UPDATE coerce_t SET i=NULL, v=NULL, dt=NULL WHERE n IN (8,9); "\
"SELECT 'update_null', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'update_null_rows', i, CONCAT('[',v,']'), dt, n FROM coerce_t ORDER BY n; "\
"UPDATE coerce_t SET i=NULL WHERE n=999; "\
"SELECT 'update_null_no_match', ROW_COUNT(), @@warning_count, @@error_count; "\
"UPDATE coerce_t SET i=12, v='def', dt='2021-01-02 03:04:05' WHERE n=8; "\
"UPDATE coerce_t SET i=DEFAULT, v=DEFAULT, dt=DEFAULT WHERE n=8; "\
"SELECT 'update_default', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'update_default_rows', i, CONCAT('[',v,']'), dt, n FROM coerce_t WHERE n=8; "\
"UPDATE coerce_t SET i=DEFAULT WHERE n=8 LIMIT 0; "\
"SELECT 'update_default_limit0', ROW_COUNT(), @@warning_count, @@error_count;" \
    "$DATABASE"

expect_output \
    "nonstrict missing default warning rows" \
    "Warning	1364	Field 'i' doesn't have a default value
Warning	1364	Field 'v' doesn't have a default value
Warning	1364	Field 'dt' doesn't have a default value" \
    "SET sql_mode=''; "\
"TRUNCATE coerce_t; "\
"INSERT INTO coerce_t(n) VALUES (1),(2); "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "nonstrict update null warning rows" \
    "Warning	1048	Column 'i' cannot be null
Warning	1048	Column 'v' cannot be null
Warning	1048	Column 'dt' cannot be null
Warning	1048	Column 'i' cannot be null
Warning	1048	Column 'v' cannot be null
Warning	1048	Column 'dt' cannot be null" \
    "SET sql_mode=''; "\
"TRUNCATE coerce_t; "\
"INSERT INTO coerce_t VALUES (11,'abc','2020-01-02 03:04:05',8),(12,'def','2020-01-03 03:04:05',9); "\
"UPDATE coerce_t SET i=NULL, v=NULL, dt=NULL WHERE n IN (8,9); "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "nullable dropped default nonstrict rows" \
    "drop_insert	2	1	0
drop_rows	1	NULL
drop_rows	2	NULL
drop_update	0	1	0
drop_rows_after	1	NULL
drop_rows_after	2	NULL" \
    "SET sql_mode=''; "\
"DROP TABLE IF EXISTS drop_t; "\
"CREATE TABLE drop_t(id INT NOT NULL, n INT NULL); "\
"ALTER TABLE drop_t ALTER n DROP DEFAULT; "\
"INSERT INTO drop_t(id) VALUES (1),(2); "\
"SELECT 'drop_insert', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'drop_rows', id, IF(n IS NULL,'NULL',n) FROM drop_t ORDER BY id; "\
"UPDATE drop_t SET n=DEFAULT WHERE id=1; "\
"SELECT 'drop_update', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'drop_rows_after', id, IF(n IS NULL,'NULL',n) FROM drop_t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "no engine substitution is nonstrict for DML" \
    "no_engine	1	1	0
no_engine_row	0	9" \
    "SET sql_mode='NO_ENGINE_SUBSTITUTION'; "\
"DROP TABLE IF EXISTS no_engine_t; "\
"CREATE TABLE no_engine_t(i INT NOT NULL, n INT NULL); "\
"INSERT INTO no_engine_t(n) VALUES (9); "\
"SELECT 'no_engine', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT 'no_engine_row', i, n FROM no_engine_t;" \
    "$DATABASE"

expect_output \
    "nonstrict omitted auto increment still generates" \
    "auto_rows	0	20
auto_rows	1	30" \
    "SET sql_mode='NO_AUTO_VALUE_ON_ZERO'; "\
"DROP TABLE IF EXISTS auto_t; "\
"CREATE TABLE auto_t(id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, v INT NULL); "\
"INSERT INTO auto_t(id, v) VALUES (0, 20); "\
"INSERT INTO auto_t(v) VALUES (30); "\
"SELECT 'auto_rows', id, v FROM auto_t ORDER BY v;" \
    "$DATABASE"

expect_combined_output \
    "failed adjusted update preserves warnings and rows" \
    "ERROR 1062 (23000) at line 1: Duplicate entry '0' for key 'fail_t.i'
Warning	1048	Column 'i' cannot be null
Warning	1048	Column 'i' cannot be null
Error	1062	Duplicate entry '0' for key 'fail_t.i'
failed_rows	1	5
failed_rows	2	6" \
    "SET sql_mode=''; "\
"DROP TABLE IF EXISTS fail_t; "\
"CREATE TABLE fail_t(id INT NOT NULL PRIMARY KEY, i INT NOT NULL UNIQUE); "\
"INSERT INTO fail_t VALUES (1,5),(2,6); "\
"UPDATE fail_t SET i=NULL WHERE id IN (1,2); "\
"SHOW WARNINGS; "\
"SELECT 'failed_rows', id, i FROM fail_t ORDER BY id;" \
    --force \
    "$DATABASE"

printf '%s\n' "mysql_baseline_nonstrict_dml_coercion_expectations: ok"
