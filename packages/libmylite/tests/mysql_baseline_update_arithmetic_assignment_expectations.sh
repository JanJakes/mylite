#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_update_arithmetic_assignment_$$"

fail() {
    printf '%s\n' "mysql_baseline_update_arithmetic_assignment_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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

reset_numbers() {
    run_mysql \
        "DROP TABLE IF EXISTS numbers; "\
"CREATE TABLE numbers ("\
"id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "\
"i INT NULL, nn INT NOT NULL, iu INT UNSIGNED NULL, integeru INTEGER UNSIGNED NULL, "\
"b BIGINT NULL, bu BIGINT UNSIGNED NULL, n INT NULL, tie INT NULL); "\
"INSERT INTO numbers(i, nn, iu, integeru, b, bu, n, tie) VALUES "\
"(1, 1, 1, 7, 1, 1, NULL, 2), "\
"(NULL, 2, 0, 8, 6, 6, 9, NULL), "\
"(3, 3, 4, 9, -7, 7, NULL, 1), "\
"(0, 4, 8, 10, 8, 8, 9, 1);" \
        "$DATABASE" >/dev/null
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
run_mysql "CREATE DATABASE ${DATABASE}; SET GLOBAL sql_mode = 'STRICT_TRANS_TABLES';" >/dev/null

reset_numbers
expect_output \
    "full-table increment changed rows and null propagation" \
    "3	0	1:2,2:N,3:4,4:1" \
    "SET sql_mode = 'STRICT_TRANS_TABLES';
     UPDATE numbers SET i = i + 1 ORDER BY id;
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', IFNULL(i, 'N')) ORDER BY id)
     FROM numbers;" \
    "$DATABASE"

expect_output \
    "no-op plus zero changed rows" \
    "0	0	1:2,2:N,3:4,4:1" \
    "UPDATE numbers SET i = i + 0;
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', IFNULL(i, 'N')) ORDER BY id)
     FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "filtered decrement" \
    "1	0	0" \
    "UPDATE numbers SET nn = nn - 1 WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count, nn FROM numbers WHERE id = 1;" \
    "$DATABASE"

reset_numbers
expect_output \
    "unsigned increment in range" \
    "1	0	2" \
    "UPDATE numbers SET iu = iu + 1 WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count, iu FROM numbers WHERE id = 1;" \
    "$DATABASE"

reset_numbers
expect_output \
    "no-match skips oversized literal evaluation" \
    "0	0" \
    "UPDATE numbers SET i = i + 999999999999999999999 WHERE id = 999;
     SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

reset_numbers
expect_output \
    "limit zero skips oversized literal evaluation" \
    "0	0" \
    "UPDATE numbers SET i = i + 999999999999999999999 LIMIT 0;
     SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

reset_numbers
expect_output \
    "order limit default asc null first" \
    "1	0	1:1,2:N,3:3,4:5" \
    "UPDATE numbers SET i = i + 5 ORDER BY tie LIMIT 2;
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', IFNULL(i, 'N')) ORDER BY id)
     FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "order limit desc" \
    "1	0	1:-4,2:N,3:3,4:0" \
    "UPDATE numbers SET i = i - 5 ORDER BY tie DESC LIMIT 1;
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', IFNULL(i, 'N')) ORDER BY id)
     FROM numbers;" \
    "$DATABASE"

reset_numbers
expect_output \
    "duplicate order-key ties do not update beyond limit" \
    "2	0	1:1,2:N,3:4,4:1" \
    "UPDATE numbers SET i = i + 1 ORDER BY tie ASC LIMIT 3;
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', IFNULL(i, 'N')) ORDER BY id)
     FROM numbers;" \
    "$DATABASE"

expect_error \
    "unknown arithmetic source column resolves before no-match" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "UPDATE numbers SET i = missing + 1 WHERE id = 999;" \
    "$DATABASE"

expect_error \
    "int addition assignment overflow" \
    1264 \
    22003 \
    "Out of range value for column 'i' at row 1" \
    "DROP TABLE IF EXISTS bounds;
     CREATE TABLE bounds(id INT PRIMARY KEY, i INT, iu INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED);
     INSERT INTO bounds VALUES (1, 2147483647, 0, 0, 0);
     UPDATE bounds SET i = i + 1 WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "int subtraction assignment overflow" \
    1264 \
    22003 \
    "Out of range value for column 'i' at row 1" \
    "DROP TABLE IF EXISTS bounds;
     CREATE TABLE bounds(id INT PRIMARY KEY, i INT, iu INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED);
     INSERT INTO bounds VALUES (1, -2147483648, 0, 0, 0);
     UPDATE bounds SET i = i - 1 WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "unsigned int addition assignment overflow" \
    1264 \
    22003 \
    "Out of range value for column 'iu' at row 1" \
    "DROP TABLE IF EXISTS bounds;
     CREATE TABLE bounds(id INT PRIMARY KEY, i INT, iu INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED);
     INSERT INTO bounds VALUES (1, 0, 4294967295, 0, 0);
     UPDATE bounds SET iu = iu + 1 WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "unsigned int subtraction expression overflow" \
    1690 \
    22003 \
    "BIGINT UNSIGNED value is out of range" \
    "DROP TABLE IF EXISTS bounds;
     CREATE TABLE bounds(id INT PRIMARY KEY, i INT, iu INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED);
     INSERT INTO bounds VALUES (1, 0, 0, 0, 0);
     UPDATE bounds SET iu = iu - 1 WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "bigint addition expression overflow" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "DROP TABLE IF EXISTS bounds;
     CREATE TABLE bounds(id INT PRIMARY KEY, i INT, iu INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED);
     INSERT INTO bounds VALUES (1, 0, 0, 9223372036854775807, 0);
     UPDATE bounds SET b = b + 1 WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "bigint subtraction expression overflow" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "DROP TABLE IF EXISTS bounds;
     CREATE TABLE bounds(id INT PRIMARY KEY, i INT, iu INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED);
     INSERT INTO bounds VALUES (1, 0, 0, -9223372036854775808, 0);
     UPDATE bounds SET b = b - 1 WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepts unsigned bigint above current mylite physical range" \
    "9223372036854775808	1	0" \
    "DROP TABLE IF EXISTS bounds;
     CREATE TABLE bounds(id INT PRIMARY KEY, bu BIGINT UNSIGNED);
     INSERT INTO bounds VALUES (1, 9223372036854775807);
     UPDATE bounds SET bu = bu + 1 WHERE id = 1;
     SELECT bu, ROW_COUNT(), @@warning_count FROM bounds;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_update_arithmetic_assignment_expectations: ok"
