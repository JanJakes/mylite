#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_row_count_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_row_count_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
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

expect_output \
    "fresh row count starts at minus one" \
    "-1" \
    "SELECT ROW_COUNT();"

expected_labels=$(cat <<EOF
row_count()	Row_Count()	ROW_COUNT ()	(ROW_COUNT())
-1	-1	-1	-1
EOF
)
expect_output_with_headers \
    "source expression labels are preserved" \
    "$expected_labels" \
    "SELECT row_count(), Row_Count(), ROW_COUNT (), (ROW_COUNT());"

expected_transitions=$(cat <<EOF
create_database	1	0
use_database	0	${DATABASE}	0
create_table	0	0
insert_three	3	0
1	10
2	20
3	30
after_select_table	-1	0
-1
after_row_count_select	-1	0
update_noop	0	0
update_changed	1	0
delete_nomatch	0	0
delete_one	1	0
truncate_table	0	0
rename_table	0	0
drop_table	0	0
drop_database	-1	0	NULL
EOF
)
expect_output \
    "statement transitions match mysql row count state" \
    "$expected_transitions" \
    "CREATE DATABASE ${DATABASE}; "\
"SELECT 'create_database', ROW_COUNT(), @@warning_count; "\
"USE ${DATABASE}; "\
"SELECT 'use_database', ROW_COUNT(), DATABASE(), @@warning_count; "\
"CREATE TABLE t (id INT NOT NULL, v INT NULL); "\
"SELECT 'create_table', ROW_COUNT(), @@warning_count; "\
"INSERT INTO t VALUES (1, 10), (2, 20), (3, 30); "\
"SELECT 'insert_three', ROW_COUNT(), @@warning_count; "\
"SELECT id, v FROM t ORDER BY id; "\
"SELECT 'after_select_table', ROW_COUNT(), @@warning_count; "\
"SELECT ROW_COUNT(); "\
"SELECT 'after_row_count_select', ROW_COUNT(), @@warning_count; "\
"UPDATE t SET v = 10 WHERE id = 1; "\
"SELECT 'update_noop', ROW_COUNT(), @@warning_count; "\
"UPDATE t SET v = 11 WHERE id = 1; "\
"SELECT 'update_changed', ROW_COUNT(), @@warning_count; "\
"DELETE FROM t WHERE id = 999; "\
"SELECT 'delete_nomatch', ROW_COUNT(), @@warning_count; "\
"DELETE FROM t WHERE id = 2; "\
"SELECT 'delete_one', ROW_COUNT(), @@warning_count; "\
"TRUNCATE TABLE t; "\
"SELECT 'truncate_table', ROW_COUNT(), @@warning_count; "\
"RENAME TABLE t TO renamed; "\
"SELECT 'rename_table', ROW_COUNT(), @@warning_count; "\
"DROP TABLE renamed; "\
"SELECT 'drop_table', ROW_COUNT(), @@warning_count; "\
"DROP DATABASE ${DATABASE}; "\
"SELECT 'drop_database', ROW_COUNT(), @@warning_count, DATABASE();"

expect_output \
    "from dual returns saved row count and then result-set row count" \
    "2	2
-1	0" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE t (id INT); "\
"INSERT INTO t VALUES (1), (2); "\
"SELECT ROW_COUNT(), (ROW_COUNT()) FROM DUAL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"DROP DATABASE ${DATABASE};"

expect_output \
    "mysql accepts table-backed row count outside mylite slice" \
    "2
2
-1	0" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE t (id INT); "\
"INSERT INTO t VALUES (1), (2); "\
"SELECT ROW_COUNT() FROM t ORDER BY id; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"DROP DATABASE ${DATABASE};"

expect_error \
    "row count rejects integer argument" \
    1064 \
    42000 \
    "near '1)'" \
    "SELECT ROW_COUNT(1);"

expect_error \
    "row count rejects null argument" \
    1064 \
    42000 \
    "near 'NULL)'" \
    "SELECT ROW_COUNT(NULL);"

expect_error \
    "row count rejects multiple arguments" \
    1064 \
    42000 \
    "near '1, 2)'" \
    "SELECT ROW_COUNT(1, 2);"

expect_error \
    "bare row count is not a function call" \
    1054 \
    42S22 \
    "Unknown column 'ROW_COUNT'" \
    "SELECT ROW_COUNT;"
