#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_charset_collation_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_charset_collation_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; USE ${DATABASE}; SET NAMES utf8mb4;" >/dev/null

scalar_expected=$(cat <<EXPECTED
utf8mb4	utf8mb4_0900_ai_ci	binary	binary	binary	binary	binary	binary	utf8mb3	utf8mb3_general_ci	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4	utf8mb4_0900_ai_ci	binary	binary
0	-1
EXPECTED
)
expect_output \
    "scalar charset and collation values" \
    "$scalar_expected" \
    "SET NAMES utf8mb4; "\
"SELECT CHARSET('abc'), COLLATION('abc'), CHARSET(CAST('ABC' AS BINARY)), "\
"COLLATION(CONVERT('ABC', BINARY)), CHARSET(CONVERT('ABC' USING BINARY)), "\
"COLLATION(NULL), CHARSET(123), COLLATION(RAND(0)), CHARSET(DATABASE()), "\
"COLLATION(DATABASE()), CHARSET(CONVERT('ABC' USING utf8mb4)), "\
"COLLATION(CONCAT(1, 'a')), CHARSET(CONCAT(NULL, 1)), COLLATION(CONCAT(NULL, 1)), "\
"CHARSET(CONCAT(CAST('a' AS BINARY), 'b')), COLLATION(CONCAT(X'41', 'b')); "\
"SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

nondefault_collation_expected=$(cat <<EXPECTED
utf8mb4_unicode_ci	utf8mb4_unicode_ci	utf8mb4_0900_ai_ci	utf8mb4
EXPECTED
)
expect_output \
    "nondefault connection collation" \
    "$nondefault_collation_expected" \
    "SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci; "\
"SELECT COLLATION('abc'), COLLATION(CONCAT('a', 'b')), "\
"COLLATION(CONVERT('ABC' USING utf8mb4)), CHARSET(CONVERT('ABC' USING utf8mb4));" \
    "$DATABASE"

expect_output \
    "from dual charset value" \
    "utf8mb4	binary" \
    "SELECT CHARSET ('a'), COLLATION(CONVERT('A' USING BINARY)) FROM DUAL;" \
    "$DATABASE"

expect_output \
    "do row count" \
    "0	0" \
    "DO CHARSET('abc'), COLLATION(NULL), CHARSET(CAST('A' AS BINARY)); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(10), c CHAR(5), txt TEXT, b VARBINARY(5), bl BLOB, "\
"e ENUM('a','b'), s SET('a','b'), i INT, d DATE"\
") CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci; "\
"INSERT INTO t VALUES "\
"(1, 'x', 'y', 'z', X'41', X'42', 'a', 'a,b', 7, '2024-01-02'), "\
"(2, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<EXPECTED
1	utf8mb4	utf8mb4_unicode_ci	utf8mb4	utf8mb4_unicode_ci	binary	binary	utf8mb4	utf8mb4_unicode_ci	binary	binary
2	utf8mb4	utf8mb4_unicode_ci	utf8mb4	utf8mb4_unicode_ci	binary	binary	utf8mb4	utf8mb4_unicode_ci	binary	binary
EXPECTED
)
expect_output \
    "table charset and collation values" \
    "$table_expected" \
    "SELECT id, CHARSET(v), COLLATION(v), CHARSET(c), COLLATION(txt), "\
"CHARSET(b), COLLATION(bl), CHARSET(e), COLLATION(s), CHARSET(i), COLLATION(d) "\
"FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table row envelope" \
    "2	utf8mb4
1	utf8mb4" \
    "SELECT id, CHARSET(v) FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

expect_error \
    "charset rejects zero arguments" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT CHARSET();" \
    "$DATABASE"

expect_error \
    "collation rejects too many arguments" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT COLLATION('a', 'b');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_charset_collation_functions_expectations: ok"
