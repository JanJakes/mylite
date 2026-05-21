#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_update_constant_arithmetic_assignment_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_update_constant_arithmetic_assignment_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "${MYSQL_BIN:-mysql}" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
        return
    fi
    if [ -n "$MYSQL_BIN" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot --default-character-set=utf8mb4 \
                --batch --raw --skip-column-names "$@"
        return
    fi
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

run_mysql \
    "USE ${DATABASE}; "\
"CREATE TABLE numbers ("\
"id INT PRIMARY KEY, "\
"i INT NULL, "\
"nn INT NOT NULL, "\
"u INT UNSIGNED NULL, "\
"bi BIGINT NULL, "\
"bu BIGINT UNSIGNED NULL"\
"); "\
"INSERT INTO numbers VALUES "\
"(1, 0, 7, 0, 0, 0), "\
"(2, 10, 8, 10, 10, 10), "\
"(3, NULL, 9, NULL, NULL, NULL);" >/dev/null

core_expected=$(cat <<EXPECTED
3	14	20	1	0	0
EXPECTED
)
expect_output \
    "constant arithmetic core update values" \
    "$core_expected" \
    "USE ${DATABASE}; "\
"UPDATE numbers SET i = 1 + 2 WHERE id = 1; "\
"UPDATE numbers SET i = 2 + 3 * 4 WHERE id = 2; "\
"UPDATE numbers SET i = (2 + 3) * 4 WHERE id = 3; "\
"SELECT "\
"(SELECT i FROM numbers WHERE id = 1), "\
"(SELECT i FROM numbers WHERE id = 2), "\
"(SELECT i FROM numbers WHERE id = 3), "\
"ROW_COUNT(), @@warning_count, "\
"(SELECT COUNT(*) FROM numbers WHERE i IS NULL);"

expect_output \
    "boolean and unary operands" \
    "2	1	0	1" \
    "USE ${DATABASE}; "\
"UPDATE numbers SET i = TRUE + 1 WHERE id = 1; "\
"UPDATE numbers SET i = -1 + +2 WHERE id = 2; "\
"SELECT "\
"(SELECT i FROM numbers WHERE id = 1), "\
"(SELECT i FROM numbers WHERE id = 2), "\
"@@warning_count, ROW_COUNT();"

expect_output \
    "null arithmetic into nullable target" \
    "1	0	1" \
    "USE ${DATABASE}; "\
"UPDATE numbers SET i = NULL + 1 WHERE id = 1; "\
"SELECT (SELECT i IS NULL FROM numbers WHERE id = 1), @@warning_count, ROW_COUNT();"

expect_output \
    "integer family targets" \
    "11	12	13	14	0	1" \
    "USE ${DATABASE}; "\
"UPDATE numbers SET i = 10 + 1 WHERE id = 1; "\
"UPDATE numbers SET u = 10 + 2 WHERE id = 1; "\
"UPDATE numbers SET bi = 10 + 3 WHERE id = 1; "\
"UPDATE numbers SET bu = 10 + 4 WHERE id = 1; "\
"SELECT i, u, bi, bu, @@warning_count, ROW_COUNT() FROM numbers WHERE id = 1;"

expect_output \
    "key and auto-increment targets" \
    "1	0	2:20:2:2,3:21:11:1,4:40:12:4" \
    "USE ${DATABASE}; "\
"CREATE TABLE key_numbers ("\
"id INT PRIMARY KEY, "\
"u INT UNIQUE, "\
"a INT AUTO_INCREMENT UNIQUE, "\
"v INT"\
"); "\
"INSERT INTO key_numbers(id, u, a, v) VALUES (1, 10, NULL, 1), (2, 20, NULL, 2); "\
"UPDATE key_numbers SET id = 2 + 1 WHERE id = 1; "\
"UPDATE key_numbers SET u = 10 + 11 WHERE id = 3; "\
"UPDATE key_numbers SET a = 10 + 1 WHERE id = 3; "\
"INSERT INTO key_numbers(id, u, v) VALUES (4, 40, 4); "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', u, ':', a, ':', v) ORDER BY id) FROM key_numbers;"

run_mysql \
    "USE ${DATABASE}; "\
"CREATE TABLE key_conflicts (id INT PRIMARY KEY, u INT UNIQUE); "\
"INSERT INTO key_conflicts VALUES (1, 10), (2, 20);" >/dev/null

expect_error \
    "primary key constant arithmetic duplicate" \
    1062 \
    "23000" \
    "Duplicate entry '2' for key 'key_conflicts.PRIMARY'" \
    "USE ${DATABASE}; UPDATE key_conflicts SET id = 1 + 1 WHERE id = 1;"

expect_error \
    "unique key constant arithmetic duplicate" \
    1062 \
    "23000" \
    "Duplicate entry '20' for key 'key_conflicts.u'" \
    "USE ${DATABASE}; UPDATE key_conflicts SET u = 10 + 10 WHERE id = 1;"

expect_output \
    "where order limit and no-op changed rows" \
    "$(cat <<EXPECTED
2	0	1:30,2:30,3:20
0	0
EXPECTED
)" \
    "USE ${DATABASE}; "\
"UPDATE numbers SET i = 20 + 10 WHERE id IN (1,2,3) ORDER BY id LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', COALESCE(i, 'NULL')) ORDER BY id) FROM numbers; "\
"UPDATE numbers SET i = 30 WHERE id IN (1,2); "\
"SELECT ROW_COUNT(), @@warning_count;"

expect_output \
    "limit zero does not update" \
    "0	0	0" \
    "USE ${DATABASE}; "\
"UPDATE numbers SET i = 77 + 1 ORDER BY id LIMIT 0; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers WHERE i = 78;"

expect_output \
    "null expression into not null non-strict adjustment" \
    "2	3	1:0,2:0,3:0" \
    "USE ${DATABASE}; "\
"UPDATE numbers SET nn = 0 WHERE id = 3; "\
"SET sql_mode = ''; "\
"UPDATE numbers SET nn = NULL + 1; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', nn) ORDER BY id) FROM numbers;"

expect_error \
    "null expression into not null strict error" \
    1048 \
    "23000" \
    "Column 'nn' cannot be null" \
    "USE ${DATABASE}; SET sql_mode = 'STRICT_TRANS_TABLES'; UPDATE numbers SET nn = NULL + 1 WHERE id = 1;"

expect_error \
    "unsigned target out of range" \
    1264 \
    "22003" \
    "Out of range value for column 'u' at row 1" \
    "USE ${DATABASE}; UPDATE numbers SET u = -1 + 0 WHERE id = 1;"

expect_error \
    "signed int target out of range" \
    1264 \
    "22003" \
    "Out of range value for column 'i' at row 1" \
    "USE ${DATABASE}; UPDATE numbers SET i = 2147483647 + 1 WHERE id = 1;"

expect_error \
    "signed bigint expression overflow" \
    1690 \
    "22003" \
    "BIGINT value is out of range" \
    "USE ${DATABASE}; UPDATE numbers SET bu = 9223372036854775807 + 1 WHERE id = 1;"
