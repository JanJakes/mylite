#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_table_partition_selection_$$"

fail() {
    printf '%s\n' "mysql_baseline_table_partition_selection_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names "$@"
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; "\
"CREATE TABLE sales (id INT NOT NULL, region INT NOT NULL, label VARCHAR(16)) "\
"PARTITION BY RANGE (region) "\
"(PARTITION p0 VALUES LESS THAN (10), PARTITION p1 VALUES LESS THAN MAXVALUE); "\
"INSERT INTO sales VALUES (1,3,'west'),(2,12,'east'),(3,8,'north');" >/dev/null

expect_output \
    "select from p0" \
    "1,3" \
    "USE ${DATABASE}; SELECT GROUP_CONCAT(id ORDER BY id) FROM sales PARTITION (p0);"
expect_output \
    "select from p1" \
    "2" \
    "USE ${DATABASE}; SELECT GROUP_CONCAT(id ORDER BY id) FROM sales PARTITION (p1);"

run_mysql "USE ${DATABASE}; UPDATE sales PARTITION (p0) SET label = 'hit' WHERE id = 2;" \
    >/dev/null
expect_output \
    "update ignores rows outside selected partition" \
    "east" \
    "USE ${DATABASE}; SELECT label FROM sales WHERE id = 2;"

run_mysql "USE ${DATABASE}; DELETE FROM sales PARTITION (p1) WHERE id = 1;" >/dev/null
expect_output \
    "delete ignores rows outside selected partition" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM sales WHERE id = 1;"

run_mysql "USE ${DATABASE}; INSERT INTO sales PARTITION (p0) VALUES (4,4,'ok');" \
    >/dev/null
expect_output \
    "insert into matching partition" \
    "1" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM sales WHERE id = 4;"

expect_error \
    "insert row outside named partition" \
    1748 \
    HY000 \
    "Found a row not matching the given partition set" \
    "USE ${DATABASE}; INSERT INTO sales PARTITION (p0) VALUES (5,99,'bad');"

expect_error \
    "select unknown partition" \
    1735 \
    HY000 \
    "Unknown partition 'missing_p' in table 'sales'" \
    "USE ${DATABASE}; SELECT COUNT(*) FROM sales PARTITION (missing_p);"

run_mysql "USE ${DATABASE}; CREATE TABLE sales_copy (id INT NOT NULL, region INT NOT NULL, "\
"label VARCHAR(16)); "\
"INSERT INTO sales_copy SELECT * FROM sales PARTITION (p0);" >/dev/null
expect_output \
    "insert select source partition" \
    "1,3,4" \
    "USE ${DATABASE}; SELECT GROUP_CONCAT(id ORDER BY id) FROM sales_copy;"

run_mysql "USE ${DATABASE}; CREATE TABLE keyed_sales (id INT NOT NULL, region INT NOT NULL, "\
"label VARCHAR(16), PRIMARY KEY (id, region)) PARTITION BY RANGE (region) "\
"(PARTITION p0 VALUES LESS THAN (10), PARTITION p1 VALUES LESS THAN MAXVALUE); "\
"INSERT INTO keyed_sales VALUES (1,4,'old'); "\
"REPLACE INTO keyed_sales PARTITION (p0) VALUES (1,4,'new');" >/dev/null
expect_output \
    "replace into matching partition" \
    "new" \
    "USE ${DATABASE}; SELECT label FROM keyed_sales WHERE id = 1 AND region = 4;"

expect_error \
    "replace row outside named partition" \
    1748 \
    HY000 \
    "Found a row not matching the given partition set" \
    "USE ${DATABASE}; REPLACE INTO keyed_sales PARTITION (p0) VALUES (2,99,'bad');"

cleanup

printf '%s\n' "mysql_baseline_table_partition_selection_expectations: ok"
