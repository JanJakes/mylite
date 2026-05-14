#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_string_length_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_string_length_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --default-character-set=utf8mb4 "$@"
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

database_length=${#DATABASE}

scalar_expected=$(cat <<EXPECTED
3	3	3	3	24	2	1	4	1	NULL	NULL	3	2	8	${database_length}
-1	0
EXPECTED
)
expect_output \
    "scalar string length values" \
    "$scalar_expected" \
    "DO 0; SELECT LENGTH('abc'), OCTET_LENGTH('abc'), CHAR_LENGTH('abc'), "\
"CHARACTER_LENGTH('abc'), BIT_LENGTH('abc'), LENGTH('é'), CHAR_LENGTH('é'), "\
"LENGTH('🙂'), CHAR_LENGTH('🙂'), LENGTH(NULL), CHAR_LENGTH(NULL), LENGTH(123), "\
"CHAR_LENGTH(-7), BIT_LENGTH(TRUE), LENGTH(DATABASE()); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "from dual whitespace values" \
    "1	1	8	1	1" \
    "SELECT LENGTH ('a'), OCTET_LENGTH ('a'), BIT_LENGTH ('a'), CHAR_LENGTH ('a'), "\
"CHARACTER_LENGTH ('a') FROM DUAL;" \
    "$DATABASE"

expect_output \
    "do row count" \
    "0	0" \
    "DO LENGTH('abc'), CHAR_LENGTH(NULL), BIT_LENGTH(TRUE); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(20), c CHAR(5), txt TEXT, b VARBINARY(20), bl BLOB, "\
"b1 BIT(1), b9 BIT(9), i INT, d DECIMAL(6,2), dt DATE"\
"); "\
"INSERT INTO t VALUES "\
"(1, 'abc', 'a  ', 'hello', X'410042', X'00ff', b'1', b'100000001', 123, 12.30, '2024-01-02'), "\
"(2, 'é🙂', 'é', '', X'', X'', b'0', b'100000001', -7, -4.50, NULL), "\
"(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<\EXPECTED
1	3	3	24	1	1	5	5	3	3	24	2	2	1	2	3	5	10
2	6	2	48	2	1	0	0	0	0	0	0	0	1	2	2	5	NULL
3	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table string length values" \
    "$table_expected" \
    "SELECT id, LENGTH(v), CHAR_LENGTH(v), BIT_LENGTH(v), LENGTH(c), CHAR_LENGTH(c), "\
"LENGTH(txt), CHAR_LENGTH(txt), LENGTH(b), CHAR_LENGTH(b), BIT_LENGTH(b), "\
"LENGTH(bl), CHAR_LENGTH(bl), LENGTH(b1), LENGTH(b9), LENGTH(i), LENGTH(d), LENGTH(dt) "\
"FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table row envelope" \
    "3	NULL
2	6" \
    "SELECT id, LENGTH(v) AS bytes FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

labels_expected=$(cat <<\EXPECTED
LENGTH(v)	b	CHAR_LENGTH(v)	CHARACTER_LENGTH(v)	BIT_LENGTH(v)
3	3	3	3	24
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT LENGTH(v), OCTET_LENGTH(v) AS b, CHAR_LENGTH(v), "\
"CHARACTER_LENGTH(v), BIT_LENGTH(v) FROM t WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepted deferred nested forms" \
    "2	3	3	24	3	3	24" \
    "SELECT LENGTH(CONCAT('a','b')), LENGTH(CAST('ABC' AS BINARY)), "\
"CHAR_LENGTH(CAST('ABC' AS BINARY)), BIT_LENGTH(CAST('ABC' AS BINARY)), "\
"LENGTH(X'410042'), CHAR_LENGTH(X'410042'), BIT_LENGTH(X'410042');" \
    "$DATABASE"

expect_output \
    "mysql accepted deferred predicate" \
    "1" \
    "SELECT COUNT(*) FROM t WHERE LENGTH(v) = 3;" \
    "$DATABASE"

expect_error \
    "length rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'LENGTH'" \
    "SELECT LENGTH();" \
    "$DATABASE"

expect_error \
    "bit length rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'BIT_LENGTH'" \
    "SELECT BIT_LENGTH('a', 'b');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_string_length_functions_expectations: ok"
