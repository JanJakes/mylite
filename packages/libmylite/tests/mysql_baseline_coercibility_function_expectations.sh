#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_coercibility_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_coercibility_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 --skip-column-names \
            --default-character-set=utf8mb4 "$@"
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
run_mysql \
    "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"USE ${DATABASE}; SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci;" >/dev/null

scalar_expected=$(cat <<EXPECTED
4	5	6	5	5	3	3	4	4	6	2	2	2	2	2	2	4	4	4
0	-1
EXPECTED
)
expect_output \
    "scalar coercibility values" \
    "$scalar_expected" \
    "SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"SELECT COERCIBILITY('abc'), COERCIBILITY(1000), COERCIBILITY(NULL), "\
"COERCIBILITY(TRUE), COERCIBILITY(RAND(0)), COERCIBILITY(DATABASE()), "\
"COERCIBILITY(VERSION()), COERCIBILITY(CONCAT('a','b')), "\
"COERCIBILITY(CONCAT(NULL, 1)), COERCIBILITY(CONCAT(NULL, NULL)), "\
"COERCIBILITY(CAST('ABC' AS BINARY)), COERCIBILITY(CONVERT('ABC', BINARY)), "\
"COERCIBILITY(CONVERT('ABC' USING BINARY)), "\
"COERCIBILITY(CONVERT('ABC' USING utf8mb4)), "\
"COERCIBILITY(CAST(DATABASE() AS BINARY)), COERCIBILITY(CONVERT(1 USING BINARY)), "\
"COERCIBILITY(X'41'), "\
"COERCIBILITY(B'101'), COERCIBILITY(CONCAT(X'41', 'b')); "\
"SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

concat_expected="3	2	2	4"
expect_output \
    "concat coercibility inheritance" \
    "$concat_expected" \
    "SELECT COERCIBILITY(CONCAT(DATABASE(), 'x')), "\
"COERCIBILITY(CONCAT(CAST('a' AS BINARY), 'b')), "\
"COERCIBILITY(CONCAT(CONVERT('a' USING utf8mb4), 'x')), "\
"COERCIBILITY(CONCAT(RAND(0), 'x'));" \
    "$DATABASE"

expect_output \
    "nondefault connection collation does not change literal coercibility" \
    "4	4	2" \
    "SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci; "\
"SELECT COERCIBILITY('abc'), COERCIBILITY(CONCAT('a', 'b')), "\
"COERCIBILITY(CONVERT('ABC' USING utf8mb4));" \
    "$DATABASE"

expect_output \
    "from dual coercibility value" \
    "4	2" \
    "SELECT COERCIBILITY ('a'), COERCIBILITY(CONVERT('A' USING BINARY)) FROM DUAL;" \
    "$DATABASE"

expect_output \
    "do row count" \
    "0	0" \
    "DO COERCIBILITY('abc'), COERCIBILITY(NULL), COERCIBILITY(CAST('A' AS BINARY)); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(10), c CHAR(5), txt TEXT, b VARBINARY(5), bl BLOB, bt BIT(3), "\
"e ENUM('a','b'), s SET('a','b'), i INT, d DATE, j JSON"\
") CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci; "\
"INSERT INTO t VALUES "\
"(1, 'x', 'y', 'z', X'41', X'42', B'101', 'a', 'a,b', 7, '2024-01-02', '{\"a\":1}'), "\
"(2, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<EXPECTED
1	2	2	2	2	2	2	2	2	5	5	2	2	2	2
2	2	2	2	2	2	2	2	2	5	5	2	2	2	2
EXPECTED
)
expect_output \
    "table coercibility values" \
    "$table_expected" \
    "SELECT id, COERCIBILITY(v), COERCIBILITY(c), COERCIBILITY(txt), "\
"COERCIBILITY(b), COERCIBILITY(bl), COERCIBILITY(bt), COERCIBILITY(e), COERCIBILITY(s), "\
"COERCIBILITY(i), COERCIBILITY(d), COERCIBILITY(j), COERCIBILITY(CAST(v AS BINARY)), "\
"COERCIBILITY(CONVERT(v, BINARY)), COERCIBILITY(CONVERT(v USING BINARY)) FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table row envelope" \
    "2	2
1	2" \
    "SELECT id, COERCIBILITY(v) FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

expect_error \
    "coercibility rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT COERCIBILITY();" \
    "$DATABASE"

expect_error \
    "coercibility rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT COERCIBILITY('a', 'b');" \
    "$DATABASE"

expect_error \
    "coercibility binary cast resolves unknown columns" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "SELECT COERCIBILITY(CAST(missing AS BINARY));" \
    "$DATABASE"

expect_error \
    "coercibility convert binary type resolves unknown columns" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "SELECT COERCIBILITY(CONVERT(missing, BINARY));" \
    "$DATABASE"

expect_error \
    "coercibility convert using binary resolves unknown columns" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "SELECT COERCIBILITY(CONVERT(missing USING BINARY));" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_coercibility_function_expectations: ok"
